// Events read from database tables every ChatTransmitter.Events.PollInterval seconds:
//
// 1. Expedition completions. mod-temucraft-expeditions has no hook that other modules can use, and modules do not
//    include each other's code, so this reads its table mod_temucraft_expedition_progress (characters database)
//    for the players online on this server and sends a "expedition" event when a completion count goes up.
//    Nothing happens when that module is not installed.
//
// 2. The extension point for any module or tool: a row added to chat_transmitter_event_queue (characters database)
//    is sent to the bot as a notification (source + message) and then deleted. Example:
//      INSERT INTO chat_transmitter_event_queue (source, message) VALUES ('expeditions', 'Ryan finished Tier 5!');
//    In C++ a module can call sChatTransmitter->QueueNotification(source, message) or QueueEvent(type, json) directly.

#include <map>
#include <set>
#include <unordered_map>

#include "AsyncCallbackProcessor.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Log.h"
#include "Player.h"
#include "QueryCallback.h"
#include "StringFormat.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

#include "ChatTransmitter.h"
#include "ChatTransmitterScripts.h"

namespace ModChatTransmitter
{
    namespace
    {
        struct Progress
        {
            uint32 completions = 0;
            uint32 bestTier = 0;
        };

        QueryCallbackProcessor callbacks;
        uint32 pollTimer = 0;
        bool expeditionsAvailable = false;
        bool eventQueueAvailable = false;
        bool expeditionPollBusy = false;
        bool eventQueuePollBusy = false;
        std::map<uint32, std::string> expeditionNames;                                           // map id -> name
        std::set<ObjectGuid::LowType> watched;                                                    // real players online
        std::unordered_map<ObjectGuid::LowType, std::map<uint32, Progress>> baseline;             // what they had

        bool TableExists(DatabaseWorkerPool<CharacterDatabaseConnection>& db, std::string const& table)
        {
            return db.Query("SHOW TABLES LIKE '{}'", table) != nullptr;
        }

        std::string ExpeditionName(uint32 mapId)
        {
            auto itr = expeditionNames.find(mapId);
            if (itr != expeditionNames.end())
            {
                return itr->second;
            }
            MapEntry const* entry = sMapStore.LookupEntry(mapId);
            return entry ? entry->name[LOCALE_enUS] : Acore::StringFormat("Map {}", mapId);
        }

        void PollExpeditions()
        {
            if (!expeditionsAvailable || expeditionPollBusy || baseline.empty())
            {
                return;
            }

            std::string guids;
            for (auto const& [guid, rows] : baseline)
            {
                guids += (guids.empty() ? "" : ",") + std::to_string(guid);
            }

            expeditionPollBusy = true;
            callbacks.AddCallback(CharacterDatabase.AsyncQuery(Acore::StringFormat(
                "SELECT p.`owner`, p.`map`, p.`completions`, p.`best_tier`, c.`name`, c.`level`, c.`race`, c.`class`, c.`gender` "
                "FROM `mod_temucraft_expedition_progress` p JOIN `characters` c ON c.`guid` = p.`owner` WHERE p.`owner` IN ({})", guids))
                .WithCallback([](QueryResult result)
            {
                expeditionPollBusy = false;
                if (!result)
                {
                    return;
                }

                do
                {
                    Field* f = result->Fetch();
                    ObjectGuid::LowType owner = f[0].Get<uint32>();
                    uint32 mapId = f[1].Get<uint32>();
                    Progress now { f[2].Get<uint32>(), f[3].Get<uint32>() };

                    auto known = baseline.find(owner);
                    if (known == baseline.end())
                    {
                        continue; // logged out while the query ran
                    }

                    Progress& before = known->second[mapId];
                    if (now.completions > before.completions)
                    {
                        ChatTransmitter::Instance().QueueEvent("expedition", {
                            { "player", {
                                { "name", f[4].Get<std::string>() }, { "guid", owner }, { "level", f[5].Get<uint8>() },
                                { "raceId", f[6].Get<uint8>() }, { "classId", f[7].Get<uint8>() }, { "gender", f[8].Get<uint8>() } } },
                            { "map", { { "id", mapId }, { "name", ExpeditionName(mapId) } } },
                            { "completions", now.completions },
                            { "bestTier", now.bestTier },
                            { "newBestTier", now.bestTier > before.bestTier },
                        });
                    }
                    before = now;
                } while (result->NextRow());
            }));
        }

        void PollEventQueue()
        {
            if (!eventQueueAvailable || eventQueuePollBusy)
            {
                return;
            }

            eventQueuePollBusy = true;
            callbacks.AddCallback(CharacterDatabase.AsyncQuery("SELECT `id`, `source`, `message` FROM `chat_transmitter_event_queue` ORDER BY `id` LIMIT 25")
                .WithCallback([](QueryResult result)
            {
                eventQueuePollBusy = false;
                if (!result)
                {
                    return;
                }

                uint32 lastId = 0;
                do
                {
                    Field* f = result->Fetch();
                    lastId = f[0].Get<uint32>();
                    ChatTransmitter::Instance().QueueNotification(f[1].Get<std::string>(), f[2].Get<std::string>());
                } while (result->NextRow());

                CharacterDatabase.Execute("DELETE FROM `chat_transmitter_event_queue` WHERE `id` <= {}", lastId);
            }));
        }
    }

    void StartEventPoller()
    {
        ChatTransmitter::EventSettings const& s = ChatTransmitter::Instance().Settings();
        pollTimer = 0;
        LoadKilledBosses();

        eventQueueAvailable = s.eventQueue && TableExists(CharacterDatabase, "chat_transmitter_event_queue");

        expeditionsAvailable = s.expeditions && TableExists(CharacterDatabase, "mod_temucraft_expedition_progress");
        expeditionNames.clear();
        if (expeditionsAvailable && WorldDatabase.Query("SHOW TABLES LIKE 'mod_temucraft_expedition'"))
        {
            if (QueryResult result = WorldDatabase.Query("SELECT `map`, `name` FROM `mod_temucraft_expedition` WHERE `name` <> ''"))
            {
                do
                {
                    Field* f = result->Fetch();
                    expeditionNames[f[0].Get<uint32>()] = f[1].Get<std::string>();
                } while (result->NextRow());
            }
        }
        if (expeditionsAvailable)
        {
            LOG_INFO("module", "[ModChatTransmitter] Expedition completions will be sent to Discord.");
        }

        // After a config reload the players are already online
        watched.clear();
        baseline.clear();
        for (auto const& [accountId, session] : sWorldSessionMgr->GetAllSessions())
        {
            if (Player* player = session ? session->GetPlayer() : nullptr)
            {
                EventPollerLogin(player);
            }
        }
    }

    void EventPollerLogin(Player* player)
    {
        ChatTransmitter& ct = ChatTransmitter::Instance();
        if (!expeditionsAvailable || !ct.IsEnabled() || (ct.IgnoreBots() && ChatTransmitter::IsBot(player)))
        {
            return;
        }

        ObjectGuid::LowType guid = player->GetGUID().GetCounter();
        watched.insert(guid);
        callbacks.AddCallback(CharacterDatabase.AsyncQuery(Acore::StringFormat(
            "SELECT `map`, `completions`, `best_tier` FROM `mod_temucraft_expedition_progress` WHERE `owner` = {}", guid))
            .WithCallback([guid](QueryResult result)
        {
            if (!watched.count(guid))
            {
                return;
            }

            std::map<uint32, Progress>& rows = baseline[guid];
            rows.clear();
            if (result)
            {
                do
                {
                    Field* f = result->Fetch();
                    rows[f[0].Get<uint32>()] = Progress { f[1].Get<uint32>(), f[2].Get<uint32>() };
                } while (result->NextRow());
            }
        }));
    }

    void EventPollerLogout(Player* player)
    {
        ObjectGuid::LowType guid = player->GetGUID().GetCounter();
        watched.erase(guid);
        baseline.erase(guid);
    }

    void UpdateEventPoller(uint32 diff)
    {
        callbacks.ProcessReadyCallbacks();

        pollTimer += diff;
        if (pollTimer < ChatTransmitter::Instance().Settings().pollInterval * IN_MILLISECONDS)
        {
            return;
        }
        pollTimer = 0;

        PollExpeditions();
        PollEventQueue();
    }
}
