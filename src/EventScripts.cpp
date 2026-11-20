// Game events sent to Discord: player deaths, level milestones and boss kills (with realm first kills).

#include <set>
#include <unordered_map>

#include "Creature.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"

#include "ChatTransmitter.h"
#include "ChatTransmitterScripts.h"

namespace ModChatTransmitter
{
    namespace
    {
        // A death waits one world update, so the killer (sent by a second hook) can join it
        struct PendingDeath
        {
            nlohmann::json player;
            std::string zone;
            std::string map;
            bool instance = false;
            std::string killer;
            bool killerIsBoss = false;
        };

        std::unordered_map<ObjectGuid::LowType, PendingDeath> pendingDeaths;
        std::set<std::pair<uint32, uint32>> killedBosses; // (creature entry, difficulty) killed at least once
        bool killedBossesLoaded = false;

        nlohmann::json PlayerJson(Player* player)
        {
            return {
                { "name", player->GetName() },
                { "guid", player->GetGUID().GetCounter() },
                { "level", player->GetLevel() },
                { "raceId", player->getRace() },
                { "classId", player->getClass() },
                { "gender", player->getGender() },
            };
        }

        std::string ZoneName(Player* player)
        {
            AreaTableEntry const* area = sAreaTableStore.LookupEntry(player->GetZoneId());
            return area ? area->area_name[LOCALE_enUS] : "";
        }

        bool IsBoss(Creature* creature)
        {
            return creature && (creature->isWorldBoss() || creature->IsDungeonBoss());
        }

        bool Skip(Player* player)
        {
            ChatTransmitter& ct = ChatTransmitter::Instance();
            return !player || !ct.IsEnabled() || (ct.IgnoreBots() && ChatTransmitter::IsBot(player));
        }

        PendingDeath& Pending(Player* player)
        {
            auto [itr, added] = pendingDeaths.try_emplace(player->GetGUID().GetCounter());
            if (added)
            {
                Map* map = player->GetMap();
                itr->second.player = PlayerJson(player);
                itr->second.zone = ZoneName(player);
                itr->second.map = map ? map->GetMapName() : "";
                itr->second.instance = map && map->IsDungeon();
            }
            return itr->second;
        }

        bool DeathWanted(Player* player)
        {
            ChatTransmitter::EventSettings const& s = ChatTransmitter::Instance().Settings();
            if (Skip(player) || s.deaths == 0 || player->GetLevel() < s.deathMinLevel)
            {
                return false;
            }
            Map* map = player->GetMap();
            return !(map && map->IsBattlegroundOrArena());
        }
    }

    void LoadKilledBosses()
    {
        killedBosses.clear();
        if (QueryResult result = CharacterDatabase.Query("SELECT `entry`, `difficulty` FROM `chat_transmitter_boss_kills`"))
        {
            do
            {
                Field* fields = result->Fetch();
                killedBosses.emplace(fields[0].Get<uint32>(), fields[1].Get<uint32>());
            } while (result->NextRow());
        }
        killedBossesLoaded = true;
    }

    void ProcessPendingEvents()
    {
        if (pendingDeaths.empty())
        {
            return;
        }

        uint32 mode = ChatTransmitter::Instance().Settings().deaths;
        for (auto& [guid, death] : pendingDeaths)
        {
            if (mode == 1 && !death.killerIsBoss)
            {
                continue;
            }

            ChatTransmitter::Instance().QueueEvent("death", {
                { "player", death.player },
                { "killer", death.killer },
                { "killerIsBoss", death.killerIsBoss },
                { "zone", death.zone },
                { "map", death.map },
                { "instance", death.instance },
            });
        }
        pendingDeaths.clear();
    }

    class EventPlayerScripts : public PlayerScript
    {
    public:
        EventPlayerScripts() : PlayerScript("ModChatTransmitterEventPlayerScripts", {
            PLAYERHOOK_ON_PLAYER_JUST_DIED,
            PLAYERHOOK_ON_PLAYER_KILLED_BY_CREATURE,
            PLAYERHOOK_ON_LEVEL_CHANGED,
            PLAYERHOOK_ON_LOGIN,
            PLAYERHOOK_ON_LOGOUT
        })
        { }

        void OnPlayerJustDied(Player* player) override
        {
            if (DeathWanted(player))
            {
                Pending(player);
            }
        }

        void OnPlayerKilledByCreature(Creature* killer, Player* killed) override
        {
            if (!DeathWanted(killed))
            {
                return;
            }

            PendingDeath& death = Pending(killed);
            if (killer)
            {
                death.killer = killer->GetName();
                death.killerIsBoss = IsBoss(killer);
            }
        }

        void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
        {
            uint8 level = player->GetLevel();
            if (Skip(player) || level <= oldLevel || !ChatTransmitter::Instance().Settings().levelMilestones.count(level))
            {
                return;
            }

            ChatTransmitter::Instance().QueueEvent("levelUp", {
                { "player", PlayerJson(player) },
                { "level", level },
                { "zone", ZoneName(player) },
            });
        }

        void OnPlayerLogin(Player* player) override
        {
            EventPollerLogin(player);
        }

        void OnPlayerLogout(Player* player) override
        {
            EventPollerLogout(player);
            pendingDeaths.erase(player->GetGUID().GetCounter());
        }
    };

    class EventUnitScripts : public UnitScript
    {
    public:
        EventUnitScripts() : UnitScript("ModChatTransmitterEventUnitScripts", true, { UNITHOOK_ON_UNIT_DEATH }) { }

        void OnUnitDeath(Unit* unit, Unit* killer) override
        {
            ChatTransmitter& ct = ChatTransmitter::Instance();
            uint32 mode = ct.Settings().bossKills;
            Creature* boss = unit ? unit->ToCreature() : nullptr;
            if (!ct.IsEnabled() || mode == 0 || !IsBoss(boss) || !boss->GetMap())
            {
                return;
            }

            Player* player = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
            if (!player)
            {
                player = boss->GetLootRecipient();
            }
            if (!player)
            {
                return;
            }

            // The players on the boss's map: the group, or the killer alone
            std::vector<std::string> names;
            bool anyRealPlayer = !ChatTransmitter::IsBot(player);
            if (Group* group = player->GetGroup())
            {
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                {
                    Player* member = ref->GetSource();
                    if (member && member->GetMap() == boss->GetMap())
                    {
                        names.push_back(member->GetName());
                        anyRealPlayer = anyRealPlayer || !ChatTransmitter::IsBot(member);
                    }
                }
            }
            if (names.empty())
            {
                names.push_back(player->GetName());
            }
            if (ct.IgnoreBots() && !anyRealPlayer)
            {
                return;
            }

            if (!killedBossesLoaded)
            {
                LoadKilledBosses();
            }

            Map* map = boss->GetMap();
            uint32 difficulty = static_cast<uint32>(map->GetDifficulty());
            bool firstKill = killedBosses.emplace(boss->GetEntry(), difficulty).second;
            if (firstKill)
            {
                std::string killers;
                for (std::string const& name : names)
                {
                    killers += (killers.empty() ? "" : ", ") + name;
                }
                killers = CleanForGame(killers, 250);
                std::string guildName = player->GetGuildId() ? player->GetGuildName() : "";
                CharacterDatabase.EscapeString(killers);
                CharacterDatabase.EscapeString(guildName);
                CharacterDatabase.Execute("INSERT IGNORE INTO `chat_transmitter_boss_kills` (`entry`, `difficulty`, `map`, `killed_at`, `killers`, `guild_name`) VALUES ({}, {}, {}, {}, '{}', '{}')",
                    boss->GetEntry(), difficulty, map->GetId(), static_cast<uint64>(GameTime::GetGameTime().count()), killers, guildName);
            }

            if (mode == 1 && !firstKill)
            {
                return;
            }

            ct.QueueEvent("bossKill", {
                { "boss", { { "entry", boss->GetEntry() }, { "name", boss->GetName() } } },
                { "map", { { "id", map->GetId() }, { "name", map->GetMapName() } } },
                { "difficulty", difficulty },
                { "raid", map->IsRaid() },
                { "firstKill", firstKill },
                { "players", names },
                { "guild", player->GetGuildId() ? player->GetGuildName() : "" },
            });
        }
    };

    void AddEventScripts()
    {
        new EventPlayerScripts();
        new EventUnitScripts();
    }
}
