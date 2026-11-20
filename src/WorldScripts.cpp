#include "ScriptMgr.h"
#include "ChatTransmitter.h"

namespace ModChatTransmitter
{
    class WorldScripts : public WorldScript
    {
    public:
        WorldScripts() : WorldScript("ModChatTransmitterWorldScripts", {
            WORLDHOOK_ON_STARTUP,
            WORLDHOOK_ON_SHUTDOWN,
            WORLDHOOK_ON_UPDATE,
            WORLDHOOK_ON_AFTER_CONFIG_LOAD
        })
        { }

        void OnStartup() override
        {
            ChatTransmitter::Instance().Start();
        }

        void OnShutdown() override
        {
            ChatTransmitter::Instance().Stop(true);
        }

        void OnUpdate(uint32 diff) override
        {
            ChatTransmitter::Instance().Update(diff);
        }

        void OnAfterConfigLoad(bool reload) override
        {
            if (!reload)
            {
                return;
            }

            // A config reload reconnects without telling Discord that the server went off and on
            ChatTransmitter::Instance().Stop(false);
            ChatTransmitter::Instance().Start(false);
        }
    };

    void AddWorldScripts()
    {
        new WorldScripts();
    }
}
