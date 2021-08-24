#include "ChatTransmitter.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"

namespace ModChatTransmitter
{
    class PlayerScripts : public PlayerScript
    {
    public:
        PlayerScripts() : PlayerScript("ModChatTransmitterPlayerScripts")
        { }

        void OnLogin(Player*/* player*/)
        { }

        void OnChat(Player* player, uint32 type, uint32/* lang*/, std::string& msg)
        {
            if (type == ChatMsg::CHAT_MSG_SAY || type == CHAT_MSG_YELL || type == CHAT_MSG_EMOTE)
            {
                ChatTransmitter::Instance().QueueChat(player, type, msg);
            }
        }

        void OnChat(Player* player, uint32 type, uint32/* lang*/, std::string& msg, Channel* channel)
        {
            if (channel->GetName().find("Crb") != std::string::npos || channel->GetName().find("LFGForwarder") != std::string::npos)
            {
                return;
            }
            ChatTransmitter::Instance().QueueChat(player, type, msg, channel);
        }
    };

    void AddPlayerScripts()
    {
        new PlayerScripts();
    }
}
