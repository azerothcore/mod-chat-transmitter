#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>

#include "Config.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Guild.h"
#include "ChannelMgr.h"
#include "Chat.h"
#include "GuildMgr.h"
#include "Log.h"
#include "Random.h"
#include "World.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

#include "ChatTransmitter.h"
#include "ChatTransmitterScripts.h"
#include "Requests/RequestChat.h"
#include "Requests/RequestJson.h"
#include "Requests/RequestChatChannel.h"
#include "Requests/RequestQueryResult.h"
#include "Requests/RequestCommandResult.h"
#include "Requests/RequestAnticheatReport.h"
#include "Requests/RequestNotification.h"

#if __has_include("mod-anticheat/src/AnticheatMgr.h")
#include "mod-anticheat/src/AnticheatMgr.h"
#endif

// Eluna is optional: its error forwarding is only built when mod-chat-transmitter.cmake found Eluna.
#if CHAT_TRANSMITTER_ELUNA
#include "Requests/RequestElunaError.h"
#if __has_include("mod-eluna/src/LuaEngine/LuaEngine.h")
#include "mod-eluna/src/LuaEngine/LuaEngine.h"
#endif
#endif

namespace ModChatTransmitter
{
    bool PlayerInfo::SendPrivateAccountInfo()
    {
        return ChatTransmitter::Instance().SendAccountInfo();
    }

    std::string CleanForGame(std::string text, std::size_t maxLength)
    {
        std::string out;
        out.reserve(std::min(text.size(), maxLength));
        for (char c : text)
        {
            if (c == '|' || c == '\r' || c == '\n' || c == '\t' || static_cast<unsigned char>(c) < 0x20)
            {
                c = (c == '|') ? '/' : ' ';
            }
            out.push_back(c);
        }
        if (out.size() > maxLength)
        {
            out.resize(maxLength);
            // Do not cut a UTF-8 character in half
            while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80)
            {
                out.pop_back();
            }
            if (!out.empty() && (static_cast<unsigned char>(out.back()) & 0x80))
            {
                out.pop_back();
            }
        }
        return out;
    }

    std::set<uint32> ParseNumberList(std::string const& text)
    {
        std::set<uint32> out;
        std::string token;
        std::istringstream stream(text);
        while (std::getline(stream, token, ','))
        {
            token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char ch) { return std::isspace(ch); }), token.end());
            if (!token.empty() && std::all_of(token.begin(), token.end(), [](unsigned char ch) { return std::isdigit(ch); }))
            {
                out.insert(static_cast<uint32>(std::stoul(token)));
            }
        }
        return out;
    }

    std::vector<std::string> ParseNameList(std::string const& text)
    {
        std::vector<std::string> out;
        std::string token;
        std::istringstream stream(text);
        while (std::getline(stream, token, ','))
        {
            std::size_t first = token.find_first_not_of(" \t");
            std::size_t last = token.find_last_not_of(" \t");
            if (first != std::string::npos)
            {
                out.push_back(token.substr(first, last - first + 1));
            }
        }
        return out;
    }

    // Player keeps its joined channels in a protected list and the core has no public "is in channel" call.
    // A class derived from Player may name the protected member, and the member pointer it gives is a plain
    // "Player::*" that works on any Player. Nothing is ever created from this class.
    struct PlayerChannelList : public Player
    {
        static bool IsIn(Player const* player, Channel const* channel)
        {
            JoinedChannelsList const& list = player->*(&PlayerChannelList::m_channels);
            return std::find(list.begin(), list.end(), channel) != list.end();
        }
    };

    static bool EqualsNoCase(std::string const& a, std::string const& b)
    {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y)
        {
            return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
        });
    }

    Command::Command(const std::string& id)
        : id(id),
        output("")
    { }

    ChatTransmitter& ChatTransmitter::Instance()
    {
        static ChatTransmitter instance;
        return instance;
    }

    ChatTransmitter::ChatTransmitter()
        : wsClient(nullptr),
        dbManager(nullptr),
        announceOnline(false)
    {
        if (!IsEnabled())
        {
            return;
        }

        AddScripts();
    }

    bool ChatTransmitter::IsEnabled() const
    {
        return sConfigMgr->GetOption<bool>("ChatTransmitter.Enabled", false);
    }

    std::string ChatTransmitter::GetDiscordGuildId() const
    {
        return sConfigMgr->GetOption<std::string>("ChatTransmitter.DiscordGuildId", "");
    }

    std::string ChatTransmitter::GetBotWsHost() const
    {
        return sConfigMgr->GetOption<std::string>("ChatTransmitter.BotWsHost", "127.0.0.1");
    }

    std::string ChatTransmitter::GetBotWsKey() const
    {
        return sConfigMgr->GetOption<std::string>("ChatTransmitter.BotWsKey", "");
    }

    int ChatTransmitter::GetBotWsPort() const
    {
        return sConfigMgr->GetOption<int32>("ChatTransmitter.BotWsPort", 22141);
    }

    std::string ChatTransmitter::GetElunaDatabaseInfo() const
    {
        return sConfigMgr->GetOption<std::string>("ChatTransmitter.ElunaDatabaseInfo", "", false);
    }

    bool ChatTransmitter::SendAccountInfo() const
    {
        return sConfigMgr->GetOption<bool>("ChatTransmitter.SendAccountInfo", false, false);
    }

    bool ChatTransmitter::IgnoreBots() const
    {
        return sConfigMgr->GetOption<bool>("ChatTransmitter.IgnoreBots", true, false);
    }

    bool ChatTransmitter::IsBot(Player* player)
    {
        WorldSession* session = player ? player->GetSession() : nullptr;
        return !session || session->IsSocketClosed();
    }

    void ChatTransmitter::LoadSettings()
    {
        settings.status = sConfigMgr->GetOption<bool>("ChatTransmitter.Events.ServerStatus", true, false);
        settings.deaths = sConfigMgr->GetOption<uint32>("ChatTransmitter.Events.Deaths", 1, false);
        settings.deathMinLevel = sConfigMgr->GetOption<uint32>("ChatTransmitter.Events.Deaths.MinLevel", 10, false);
        settings.levelMilestones = ParseNumberList(sConfigMgr->GetOption<std::string>("ChatTransmitter.Events.LevelUp.Milestones", "10,20,30,40,50,60,70,80", false));
        settings.bossKills = sConfigMgr->GetOption<uint32>("ChatTransmitter.Events.BossKills", 1, false);
        settings.expeditions = sConfigMgr->GetOption<bool>("ChatTransmitter.Events.Expeditions", true, false);
        settings.pollInterval = std::max<uint32>(5, sConfigMgr->GetOption<uint32>("ChatTransmitter.Events.PollInterval", 30, false));
        settings.eventQueue = sConfigMgr->GetOption<bool>("ChatTransmitter.Events.Queue", true, false);
        settings.relayGuilds = ParseNumberList(sConfigMgr->GetOption<std::string>("ChatTransmitter.Relay.Guilds", "", false));
        settings.relayChannels = ParseNameList(sConfigMgr->GetOption<std::string>("ChatTransmitter.Relay.Channels", "World", false));
        settings.relayPrefix = sConfigMgr->GetOption<std::string>("ChatTransmitter.Relay.Prefix", "|cff7289da[Discord]|r", false);
        settings.linkCodeMinutes = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("ChatTransmitter.Link.CodeMinutes", 10, false));
    }

    void ChatTransmitter::QueueChat(Player* player, uint32 type, std::string& msg)
    {
        if (wsClient && IsEnabled() && !(IgnoreBots() && IsBot(player)))
        {
            QueueRequest(new Requests::Chat(player, type, msg));
        }
    }

    void ChatTransmitter::QueueChat(Player* player, uint32 type, std::string& msg, Channel* channel)
    {
        if (wsClient && IsEnabled() && !(IgnoreBots() && IsBot(player)))
        {
            QueueRequest(new Requests::ChatChannel(player, type, msg, channel));
        }
    }

    void ChatTransmitter::QueueGuildChat(Player* player, std::string& msg, Guild* guild)
    {
        if (!wsClient || !IsEnabled() || !guild || (IgnoreBots() && IsBot(player)))
        {
            return;
        }

        // Only guilds that chose to share their chat (ChatTransmitter.Relay.Guilds) leave the server
        if (!settings.relayGuilds.count(guild->GetId()))
        {
            return;
        }

        nlohmann::json data;
        nlohmann::to_json(data["player"], PlayerInfo(player));
        data["guild"] = { { "id", guild->GetId() }, { "name", guild->GetName() } };
        data["text"] = msg;
        QueueRequest(new Requests::Json("guildChat", data));
    }

    void ChatTransmitter::QueueNotification(std::string const& source, std::string const& message)
    {
        if (wsClient && IsEnabled())
        {
            QueueRequest(new Requests::Notification(source, message));
        }
    }

    void ChatTransmitter::QueueEvent(std::string const& type, nlohmann::json payload)
    {
        if (!wsClient || !IsEnabled())
        {
            return;
        }

        payload["type"] = type;
        payload["time"] = static_cast<uint64>(GameTime::GetGameTime().count());
        QueueRequest(new Requests::Json("gameEvent", payload));
    }

    std::string ChatTransmitter::CreateLinkCode(Player* player)
    {
        static char const alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // no 0/O or 1/I
        uint64 now = static_cast<uint64>(GameTime::GetGameTime().count());
        uint32 accountId = player->GetSession()->GetAccountId();

        // At most one new code every 30 seconds per account
        auto last = lastLinkCodeAt.find(accountId);
        if (last != lastLinkCodeAt.end() && last->second + 30 > now)
        {
            return "";
        }

        // One code per account at a time, and expired codes go away
        for (auto itr = linkCodes.begin(); itr != linkCodes.end();)
        {
            if (itr->second.accountId == accountId || itr->second.expiresAt <= now)
            {
                itr = linkCodes.erase(itr);
            }
            else
            {
                ++itr;
            }
        }

        std::string code;
        do
        {
            code.clear();
            for (int i = 0; i < 8; ++i)
            {
                code.push_back(alphabet[urand(0, sizeof(alphabet) - 2)]);
            }
        } while (linkCodes.count(code));

        LinkCode& entry = linkCodes[code];
        entry.accountId = accountId;
        entry.characterName = player->GetName();
        entry.expiresAt = now + settings.linkCodeMinutes * MINUTE;
        lastLinkCodeAt[accountId] = now;
        return code;
    }

    void ChatTransmitter::Update(uint32 diff)
    {
        std::string json;
        while (wsClient && wsClient->GetReceivedMessage(json))
        {
            HandleMessage(json);
        }

        Requests::QueryResult* queryResult;
        while (dbManager && dbManager->GetResult(queryResult))
        {
            QueueRequest(queryResult);
        }

        if (dbManager)
        {
            dbManager->KeepAlive();
        }

        // The connection is made on its own thread, so "online" is sent once it is ready
        if (announceOnline && wsClient && wsClient->IsReady())
        {
            announceOnline = false;
            if (settings.status)
            {
                QueueEvent("serverStatus", {
                    { "status", "online" },
                    { "players", sWorldSessionMgr->GetPlayerCount() },
                    { "startedAt", static_cast<uint64>(GameTime::GetStartTime().count()) },
                });
            }
        }

        ProcessPendingEvents();
        UpdateEventPoller(diff);
    }

    void ChatTransmitter::HandleMessage(std::string const& json)
    {
        try
        {
            nlohmann::json item = nlohmann::json::parse(json);
            std::string message = item.value("message", "");
            nlohmann::json const& data = item["data"];

            if (message == "command")
            {
                HandleCommand(data.at("id").get<std::string>(), data.at("command").get<std::string>());
            }
            // raw SQL "query" messages are not accepted (Temucraft): the bot reads the database with its own read-only user
            else if (message == "relayChat")
            {
                HandleRelay(data);
            }
            else if (message == "linkAccount")
            {
                HandleLink(data);
            }
            else if (message == "unlinkAccount")
            {
                HandleUnlink(data);
            }
        }
        catch (std::exception const& e)
        {
            LOG_ERROR("module", "[ModChatTransmitter] Bad message from the bot: {}", e.what());
        }
    }

    // A message written on Discord, shown in game in a guild or a custom channel
    void ChatTransmitter::HandleRelay(nlohmann::json const& data)
    {
        std::string author = CleanForGame(data.value("author", "Discord"), 32);
        std::string text = CleanForGame(data.value("text", ""), 255);
        std::string target = data.value("target", "");
        if (text.empty())
        {
            return;
        }

        if (target == "guild")
        {
            uint32 guildId = data.value("guild", 0u);
            if (!settings.relayGuilds.count(guildId))
            {
                return;
            }
            if (Guild* guild = sGuildMgr->GetGuildById(guildId))
            {
                std::string line = settings.relayPrefix + " " + author + ": " + text;
                WorldPacket packet;
                ChatHandler::BuildChatPacket(packet, CHAT_MSG_SYSTEM, LANG_UNIVERSAL, static_cast<WorldObject const*>(nullptr), static_cast<WorldObject const*>(nullptr), line);
                guild->BroadcastPacket(&packet);
            }
            return;
        }

        if (target == "channel")
        {
            std::string channelName = data.value("channel", "");
            auto allowed = std::find_if(settings.relayChannels.begin(), settings.relayChannels.end(), [&](std::string const& name) { return EqualsNoCase(name, channelName); });
            if (allowed == settings.relayChannels.end())
            {
                return;
            }

            // The channel can exist once per faction (or once for both when channels are shared)
            std::vector<Channel*> channels;
            for (TeamId team : { TEAM_ALLIANCE, TEAM_HORDE })
            {
                if (ChannelMgr* mgr = ChannelMgr::forTeam(team))
                {
                    for (auto const& [key, channel] : mgr->GetChannels())
                    {
                        if (channel && EqualsNoCase(channel->GetName(), *allowed) && std::find(channels.begin(), channels.end(), channel) == channels.end())
                        {
                            channels.push_back(channel);
                        }
                    }
                }
            }
            if (channels.empty())
            {
                return;
            }

            std::string line = settings.relayPrefix + " [" + *allowed + "] " + author + ": " + text;
            for (auto const& [accountId, session] : sWorldSessionMgr->GetAllSessions())
            {
                Player* player = session ? session->GetPlayer() : nullptr;
                if (!player || !player->IsInWorld() || IsBot(player))
                {
                    continue;
                }
                for (Channel* channel : channels)
                {
                    if (PlayerChannelList::IsIn(player, channel))
                    {
                        ChatHandler(session).SendSysMessage(line);
                        break;
                    }
                }
            }
        }
    }

    // "/link <code>" on Discord: the code made with ".discord link" joins the Discord user to the game account
    void ChatTransmitter::HandleLink(nlohmann::json const& data)
    {
        std::string id = data.value("id", "");
        std::string code = data.value("code", "");
        std::string discordId = data.value("discordId", "");
        std::transform(code.begin(), code.end(), code.begin(), [](unsigned char c) { return std::toupper(c); });

        auto reply = [&](bool success, std::string const& error, std::string const& characterName)
        {
            QueueRequest(new Requests::Json("linkResult", {
                { "id", id }, { "success", success }, { "error", error }, { "characterName", characterName },
            }));
        };

        if (discordId.empty() || discordId.size() > 20 || !std::all_of(discordId.begin(), discordId.end(), [](unsigned char c) { return std::isdigit(c); }))
        {
            reply(false, "bad-user", "");
            return;
        }

        uint64 now = static_cast<uint64>(GameTime::GetGameTime().count());
        auto itr = linkCodes.find(code);
        if (itr == linkCodes.end() || itr->second.expiresAt <= now)
        {
            if (itr != linkCodes.end())
            {
                linkCodes.erase(itr);
            }
            reply(false, "bad-code", "");
            return;
        }

        LinkCode entry = itr->second;
        linkCodes.erase(itr);

        LoginDatabase.Execute("REPLACE INTO `chat_transmitter_discord_link` (`account_id`, `discord_id`, `linked_at`) VALUES ({}, '{}', {})",
            entry.accountId, discordId, now);

        if (WorldSession* session = sWorldSessionMgr->FindSession(entry.accountId))
        {
            ChatHandler(session).SendSysMessage("|cff7289daDiscord:|r your account is now linked. Use .discord unlink to undo it.");
        }

        reply(true, "", entry.characterName);
    }

    // "/link remove" on Discord
    void ChatTransmitter::HandleUnlink(nlohmann::json const& data)
    {
        std::string id = data.value("id", "");
        std::string discordId = data.value("discordId", "");
        bool valid = !discordId.empty() && discordId.size() <= 20 && std::all_of(discordId.begin(), discordId.end(), [](unsigned char c) { return std::isdigit(c); });
        if (valid)
        {
            LoginDatabase.Execute("DELETE FROM `chat_transmitter_discord_link` WHERE `discord_id` = '{}'", discordId);
        }
        QueueRequest(new Requests::Json("linkResult", { { "id", id }, { "success", valid }, { "error", valid ? "" : "bad-user" }, { "characterName", "" } }));
    }

    // Waits a little for queued messages (like "offline") to be written before the socket closes
    void ChatTransmitter::FlushWebSocket(uint32 maxWaitMs)
    {
        uint32 waited = 0;
        while (wsClient && wsClient->IsReady() && !wsClient->IsIdle() && waited < maxWaitMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            waited += 50;
        }
    }

    void ChatTransmitter::Stop(bool shutdown)
    {
        if (shutdown && settings.status && wsClient && wsClient->IsReady())
        {
            QueueEvent("serverStatus", { { "status", "offline" } });
            FlushWebSocket(3000);
        }

        if (wsClient)
        {
            wsClient->Close();
        }
        if (wsThread.joinable())
        {
            wsThread.join();
        }

        if (dbManager)
        {
            dbManager->Stop();
        }
        if (dbThread.joinable())
        {
            dbThread.join();
        }
    }

    void ChatTransmitter::AddScripts() const
    {
        ModChatTransmitter::AddPlayerScripts();
        ModChatTransmitter::AddWorldScripts();
        ModChatTransmitter::AddEventScripts();
        ModChatTransmitter::AddLinkCommandScripts();
    }

    void ChatTransmitter::Start(bool announce)
    {
        if (!IsEnabled() || wsClient || dbManager)
        {
            return;
        }

        LoadSettings();
        announceOnline = announce;
        StartEventPoller();

        wsThread = std::thread(&ChatTransmitter::WebSocketThread, this);
        dbThread = std::thread(&ChatTransmitter::DatabaseThread, this);

#ifdef sAnticheatMgr
        sAnticheatMgr->OnReport += [this](Player* player, uint16 reportType)
        {
            QueueRequest(new Requests::AnticheatReport(player, reportType));
        };
#endif

#if CHAT_TRANSMITTER_ELUNA && defined(sEluna)
        sEluna->OnError += [this](std::string trace)
        {
            QueueRequest(new Requests::ElunaError(trace));
        };
#endif
    }

    void ChatTransmitter::DatabaseThread()
    {
        dbManager = new DatabaseManager();
        dbManager->Start(); // This will block until stopped

        delete dbManager;
        dbManager = nullptr;
    }

    void ChatTransmitter::WebSocketThread()
    {
        // The io_context is required for all I/O
        net::io_context ioc;

        // Launch the asynchronous operation
        wsClient = new WebSocketClient(ioc);
        std::string key = GetBotWsKey();
        wsClient->Run(GetBotWsHost(), GetBotWsPort(), "/?key=" + key);

        // Run the I/O service. The call will return when
        // the socket is closed.
        ioc.run();

        delete wsClient;
        wsClient = nullptr;
    }

    void ChatTransmitter::QueueRequest(IRequest* req)
    {
        if (!req)
        {
            return;
        }

        if (IsEnabled() && wsClient)
        {
            wsClient->QueueRequest(req);
        }
        else
        {
            delete req;
        }
    }

    void ChatTransmitter::HandleCommand(const std::string& id, const std::string& command)
    {
        Command* cmdObj = new Command(id);
        sWorld->QueueCliCommand(new CliCommandHolder(cmdObj, command.c_str(), ChatTransmitter::OnCommandOutput, ChatTransmitter::OnCommandFinished));
    }

    void ChatTransmitter::OnCommandOutput(void* arg, std::string_view text)
    {
        if (!text.empty() && arg)
        {
            Command* command = static_cast<Command*>(arg);
            command->output.append(text);
        }
    }

    void ChatTransmitter::OnCommandFinished(void* arg, bool success)
    {
        if (!arg)
        {
            return;
        }

        Command* command = static_cast<Command*>(arg);
        Instance().QueueRequest(new Requests::CommandResult(command->id, command->output, success));
        delete command;
    }
}
