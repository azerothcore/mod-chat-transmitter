#ifndef _MOD_CHAT_TRANSMITTER_H_
#define _MOD_CHAT_TRANSMITTER_H_

#include <atomic>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "Player.h"
#include "Channel.h"
#include "IRequest.h"
#include "WebSocketClient.h"
#include "DatabaseManager.h"

// Set by mod-chat-transmitter.cmake. 1 when an Eluna module (mod-ale or mod-eluna) is in the build.
#ifndef CHAT_TRANSMITTER_ELUNA
#define CHAT_TRANSMITTER_ELUNA 0
#endif

class Guild;

namespace ModChatTransmitter
{
    struct Command
    {
    public:
        Command(const std::string& id);

        std::string id;
        std::string output;
    };

    // A code made in game with ".discord link" that the bot's /link command confirms.
    struct LinkCode
    {
        uint32 accountId = 0;
        std::string characterName;
        uint64 expiresAt = 0; // game time in seconds
    };

    class ChatTransmitter
    {
    public:
        static ChatTransmitter& Instance();

        // Config methods
        bool IsEnabled() const;
        std::string GetDiscordGuildId() const;
        std::string GetBotWsHost() const;
        std::string GetBotWsKey() const;
        int GetBotWsPort() const;
        std::string GetElunaDatabaseInfo() const;
        bool SendAccountInfo() const;
        bool IgnoreBots() const;

        // True for playerbots and other players without a game client.
        static bool IsBot(Player* player);

        void QueueChat(Player* player, uint32 type, std::string& msg);
        void QueueChat(Player* player, uint32 type, std::string& msg, Channel* channel);
        void QueueGuildChat(Player* player, std::string& msg, Guild* guild);

        // Generic entry point for other modules to push a Discord notification.
        void QueueNotification(std::string const& source, std::string const& message);

        // Sends a game event (server status, death, level up, boss kill, expedition...) to the bot.
        // "type" names the event, "payload" holds its fields. See README.md for the list.
        void QueueEvent(std::string const& type, nlohmann::json payload);

        // Account linking (".discord link" in game, "/link <code>" on Discord).
        // Returns "" when the account asked for a code less than 30 seconds ago.
        std::string CreateLinkCode(Player* player);

        void Update(uint32 diff);
        void Stop(bool shutdown = false);
        void Start(bool announce = true);

        // Settings that the event scripts read, loaded in Start()
        struct EventSettings
        {
            bool status = true;
            uint32 deaths = 1;          // 0 off, 1 only deaths to a boss, 2 every death
            uint32 deathMinLevel = 1;
            std::set<uint32> levelMilestones;
            uint32 bossKills = 1;       // 0 off, 1 realm first kills only, 2 every boss kill
            bool expeditions = true;
            uint32 pollInterval = 30;   // seconds
            bool eventQueue = true;
            std::set<uint32> relayGuilds;
            std::vector<std::string> relayChannels;
            std::string relayPrefix = "|cff7289da[Discord]|r";
            uint32 linkCodeMinutes = 10;
        };
        EventSettings const& Settings() const { return settings; }

    protected:
        ChatTransmitter();

        void AddScripts() const;
        void LoadSettings();
        void WebSocketThread();
        void DatabaseThread();
        void QueueRequest(IRequest* req);
        void HandleMessage(std::string const& json);
        void HandleCommand(const std::string& id, const std::string& command);
        void HandleQuery(const std::string& id, const std::string& query, QueryDatabase dbType);
        void HandleRelay(nlohmann::json const& data);
        void HandleLink(nlohmann::json const& data);
        void HandleUnlink(nlohmann::json const& data);
        void FlushWebSocket(uint32 maxWaitMs);

        static void OnCommandOutput(void* arg, std::string_view text);
        static void OnCommandFinished(void* arg, bool success);

        std::thread wsThread;
        WebSocketClient* wsClient;

        std::thread dbThread;
        DatabaseManager* dbManager;

        EventSettings settings;
        bool announceOnline;
        std::unordered_map<std::string, LinkCode> linkCodes;
        std::unordered_map<uint32, uint64> lastLinkCodeAt; // account -> game time
    };

    // Strips WoW escape codes and line breaks from text that goes into the game.
    std::string CleanForGame(std::string text, std::size_t maxLength);
    // Splits "1, 2,3" into numbers.
    std::set<uint32> ParseNumberList(std::string const& text);
    // Splits "World, LFG" into names.
    std::vector<std::string> ParseNameList(std::string const& text);
}

#define sChatTransmitter (&ModChatTransmitter::ChatTransmitter::Instance())

#endif // _MOD_CHAT_TRANSMITTER_H_
