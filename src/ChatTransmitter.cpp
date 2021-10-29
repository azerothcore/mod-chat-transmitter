#include "Config.h"
#include "ChatTransmitter.h"
#include "ChatTransmitterScripts.h"
#include "Requests/Chat.h"
#include "Requests/ChatChannel.h"
#include "Requests/CommandResult.h"
#include "../libs/nlohmann/json.hpp"

namespace ModChatTransmitter
{
    Command::Command(std::string& id)
        : id(id),
        output("")
    { }

    ChatTransmitter& ChatTransmitter::Instance()
    {
        static ChatTransmitter instance;
        return instance;
    }

    ChatTransmitter::ChatTransmitter()
        : wsClient(nullptr)
    {
        if (!IsEnabled())
        {
            return;
        }

        AddScripts();
        Start();
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
        if (wsClient && IsEnabled())
        {
            QueueRequest(new Requests::Chat(player, type, msg));
        }
    }

    void ChatTransmitter::QueueChat(Player* player, uint32 type, std::string& msg, Channel* channel)
    {
        if (wsClient && IsEnabled())
        {
            QueueRequest(new Requests::ChatChannel(player, type, msg, channel));
        }
    }

    void ChatTransmitter::Update()
    {
        std::string json;
        while (wsClient && wsClient->GetReceivedMessage(json))
        {
            nlohmann::json item = nlohmann::json::parse(json);
            std::string message = item["message"].get<std::string>();
            nlohmann::json data = item["data"];

            if (message == "command")
            {
                std::string id = data["id"].get<std::string>();
                std::string command = data["command"].get<std::string>();
                Command* cmdObj = new Command(id);
                sWorld->QueueCliCommand(new CliCommandHolder(cmdObj, command.c_str(), ChatTransmitter::OnCommandOutput, ChatTransmitter::OnCommandFinished));
            }
        }
    }

    void ChatTransmitter::Stop()
    {
        if (wsClient)
        {
            wsClient->Close();
        }
        workerThread.join();
    }

    void ChatTransmitter::AddScripts() const
    {
        ModChatTransmitter::AddPlayerScripts();
        ModChatTransmitter::AddWorldScripts();
    }

    void ChatTransmitter::Start()
    {
        if (!IsEnabled() || wsClient)
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
            wsClient->QueueMessage(req->GetContents());
        }
        delete req;
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
