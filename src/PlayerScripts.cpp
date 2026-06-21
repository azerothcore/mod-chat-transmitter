#include "ChatTransmitter.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"

namespace ModChatTransmitter
{
    class PlayerScripts : public PlayerScript
    {
    public:
        PlayerScripts() : PlayerScript("ModChatTransmitterPlayerScripts", {
            PLAYERHOOK_CAN_PLAYER_USE_CHAT,
            PLAYERHOOK_CAN_PLAYER_USE_CHANNEL_CHAT
        })
        { }

        bool OnPlayerCanUseChat(Player* player, uint32 type, uint32/* lang*/, std::string& msg) override
        {
            if (type == ChatMsg::CHAT_MSG_SAY || type == CHAT_MSG_YELL || type == CHAT_MSG_EMOTE)
                ChatTransmitter::Instance().QueueChat(player, type, msg);

            return true;
        }

        bool OnPlayerCanUseChat(Player* player, uint32 type, uint32/* lang*/, std::string& msg, Channel* channel) override
        {
            std::string addonChannels[] = { "Crb", "LFGForwarder", "TCForwarder", "LFGShout", "xtensionxtooltip2", "QuickHealMod" };
            for (const std::string& addonChannel : addonChannels)
            {
                if (channel->GetName().find(addonChannel) != std::string::npos)
                    return true;
            }
            ChatTransmitter::Instance().QueueChat(player, type, msg, channel);

            return true;
        }
    };

    void AddPlayerScripts()
    {
        new PlayerScripts();
    }
}
