#ifndef _MOD_CHAT_TRANSMITTER_H_
#define _MOD_CHAT_TRANSMITTER_H_

#include <thread>

#include "Player.h"
#include "Channel.h"
#include "IRequest.h"
#include "WebSocketClient.h"

namespace ModChatTransmitter
{
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
        void Stop();

    protected:
        ChatTransmitter();

        void AddScripts() const;
        void StartWebSocketClient();
        void WorkerThread();
        void QueueRequest(IRequest* req);

        std::thread workerThread;
        std::shared_ptr<WebSocketClient> wsClient;
    };
}

#endif // _MOD_CHAT_TRANSMITTER_H_
