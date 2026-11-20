// .discord link   - shows a code to type into the bot's /link command on Discord
// .discord status - tells whether this account is linked
// .discord unlink - removes the link

#include "Chat.h"
#include "CommandScript.h"
#include "DatabaseEnv.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

#include "ChatTransmitter.h"
#include "ChatTransmitterScripts.h"

using namespace Acore::ChatCommands;

namespace ModChatTransmitter
{
    class LinkCommandScripts : public CommandScript
    {
    public:
        LinkCommandScripts() : CommandScript("ModChatTransmitterLinkCommands") { }

        ChatCommandTable GetCommands() const override
        {
            static ChatCommandTable sub =
            {
                { "link",   HandleLink,   SEC_PLAYER, Console::No },
                { "status", HandleStatus, SEC_PLAYER, Console::No },
                { "unlink", HandleUnlink, SEC_PLAYER, Console::No },
            };
            static ChatCommandTable root = { { "discord", sub } };
            return root;
        }

        static bool HandleLink(ChatHandler* handler)
        {
            Player* player = handler->GetPlayer();
            if (!player)
            {
                return true;
            }

            std::string code = ChatTransmitter::Instance().CreateLinkCode(player);
            if (code.empty())
            {
                handler->SendSysMessage("|cff7289daDiscord:|r please wait a few seconds before asking for a new code.");
                return true;
            }

            handler->PSendSysMessage("|cff7289daDiscord:|r your code is |cffffff00{}|r. On our Discord server type |cffffff00/link {}|r within {} minutes.",
                code, code, ChatTransmitter::Instance().Settings().linkCodeMinutes);
            handler->SendSysMessage("|cff7289daDiscord:|r never give this code to anyone else.");
            return true;
        }

        static bool HandleStatus(ChatHandler* handler)
        {
            WorldSession* session = handler->GetSession();
            if (!session)
            {
                return true;
            }

            QueryResult result = LoginDatabase.Query("SELECT 1 FROM `chat_transmitter_discord_link` WHERE `account_id` = {}", session->GetAccountId());
            handler->SendSysMessage(result
                ? "|cff7289daDiscord:|r your account is linked to Discord. Use .discord unlink to undo it."
                : "|cff7289daDiscord:|r your account is not linked. Use .discord link to get a code.");
            return true;
        }

        static bool HandleUnlink(ChatHandler* handler)
        {
            WorldSession* session = handler->GetSession();
            if (!session)
            {
                return true;
            }

            LoginDatabase.Execute("DELETE FROM `chat_transmitter_discord_link` WHERE `account_id` = {}", session->GetAccountId());
            handler->SendSysMessage("|cff7289daDiscord:|r your account is no longer linked to Discord.");
            return true;
        }
    };

    void AddLinkCommandScripts()
    {
        new LinkCommandScripts();
    }
}
