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
            ChatTransmitter::Instance().Stop();
        }

        void OnUpdate(uint32) override
        {
            ChatTransmitter::Instance().Update();
        }

        void OnAfterConfigLoad(bool reload) override
        {
            if (!reload)
            {
                return;
            }

            ChatTransmitter::Instance().Stop();
            ChatTransmitter::Instance().Start();
        }
    };

    void AddWorldScripts()
    {
        new WorldScripts();
    }
}
