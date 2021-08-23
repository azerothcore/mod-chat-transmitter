#include "ScriptMgr.h"
#include "ChatTransmitter.h"

namespace ModChatTransmitter
{
    class WorldScripts : public WorldScript
    {
    public:
        WorldScripts() : WorldScript("ModChatTransmitterWorldScripts")
        { }

        void OnShutdown() override
        {
            ChatTransmitter::Instance().Stop();
        }
    };

    void AddWorldScripts()
    {
        new WorldScripts();
    }
}
