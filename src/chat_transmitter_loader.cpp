#include "ChatTransmitter.h"
#include <Logging/Log.h>

void Addmod_chat_transmitterScripts()
{
    LOG_INFO("server", "[ModChatTransmitter] Initializing...");
    ModChatTransmitter::ChatTransmitter::Instance();
}
