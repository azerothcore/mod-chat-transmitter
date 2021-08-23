#include "Config.h"
#include "ChatTransmitter.h"
#include "ChatTransmitterScripts.h"
#include "Requests/Chat.h"
#include "Requests/ChatChannel.h"

namespace ModChatTransmitter
{
    ChatTransmitter& ChatTransmitter::Instance()
    {
        static ChatTransmitter instance;
        return instance;
    }

    ChatTransmitter::ChatTransmitter()
    {
        if (!IsEnabled())
        {
            return;
        }

        AddScripts();
        StartWebSocketClient();
    }

    bool ChatTransmitter::IsEnabled() const
    {
        return ConfigMgr::instance()->GetBoolDefault("ChatTransmitter.Enabled", false);
    }

    std::string ChatTransmitter::GetDiscordGuildId() const
    {
        return ConfigMgr::instance()->GetStringDefault("ChatTransmitter.DiscordGuildId", "");
    }

    std::string ChatTransmitter::GetBotWsHost() const
    {
        return ConfigMgr::instance()->GetStringDefault("ChatTransmitter.BotWsHost", "127.0.0.1");
    }

    std::string ChatTransmitter::GetBotWsKey() const
    {
        return ConfigMgr::instance()->GetStringDefault("ChatTransmitter.BotWsKey", "");
    }

    int ChatTransmitter::GetBotWsPort() const
    {
        return ConfigMgr::instance()->GetIntDefault("ChatTransmitter.BotWsPort", 22141);
    }

    void ChatTransmitter::QueueChat(Player* player, uint32 type, std::string& msg)
    {
        if (IsEnabled())
        {
            QueueRequest(new Requests::Chat(player, type, msg));
        }
    }

    void ChatTransmitter::QueueChat(Player* player, uint32 type, std::string& msg, Channel* channel)
    {
        if (IsEnabled())
        {
            QueueRequest(new Requests::ChatChannel(player, type, msg, channel));
        }
    }

    void ChatTransmitter::Stop()
    {
        wsClient->Close();
        workerThread.join();
    }

    void ChatTransmitter::AddScripts() const
    {
        ModChatTransmitter::AddPlayerScripts();
        ModChatTransmitter::AddWorldScripts();
    }

    void ChatTransmitter::StartWebSocketClient()
    {
        if (!IsEnabled())
        {
            return;
        }

        workerThread = std::thread(&ChatTransmitter::WorkerThread, this);
    }

    void ChatTransmitter::WorkerThread()
    {
        // The io_context is required for all I/O
        net::io_context ioc;

        // Launch the asynchronous operation
        wsClient = std::make_shared<WebSocketClient>(ioc);
        std::string key = GetBotWsKey();
        wsClient->Run(GetBotWsHost(), GetBotWsPort(), "/?key=" + key);

        // Run the I/O service. The call will return when
        // the socket is closed.
        ioc.run();
    }

    void ChatTransmitter::QueueRequest(IRequest* req)
    {
        if (req && IsEnabled())
        {
            wsClient->QueueMessage(req->GetContents());
        }
    }
}
