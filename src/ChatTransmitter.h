#ifndef _MOD_CHAT_TRANSMITTER_H_
#define _MOD_CHAT_TRANSMITTER_H_

#include <thread>

#include "Player.h"
#include "Channel.h"
#include "IRequest.h"
#include "WebSocketClient.h"

namespace ModChatTransmitter
{
    struct Command
    {
    public:
        Command(std::string& id);

        std::string id;
        std::string output;
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

        void QueueChat(Player* player, uint32 type, std::string& msg);
        void QueueChat(Player* player, uint32 type, std::string& msg, Channel* channel);
        void Update();
        void Stop();
        void Start();

    protected:
        ChatTransmitter();

        void AddScripts() const;
        void WorkerThread();
        void QueueRequest(IRequest* req);

        static void OnCommandOutput(void* arg, const char* text);
        static void OnCommandFinished(void* arg, bool success);

        std::thread workerThread;
        WebSocketClient* wsClient;
    };
}

#endif // _MOD_CHAT_TRANSMITTER_H_
