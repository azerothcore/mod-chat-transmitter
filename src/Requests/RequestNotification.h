#ifndef _MOD_CHAT_TRANSMITTER_REQUESTS_REQUEST_NOTIFICATION_H_
#define _MOD_CHAT_TRANSMITTER_REQUESTS_REQUEST_NOTIFICATION_H_

#include "../../libs/nlohmann/json.hpp"

#include "../IRequest.h"

namespace ModChatTransmitter::Requests
{
    // Generic server-to-Discord notification any module can emit (e.g. a log
    // event). The bot routes it to a channel keyed by the "source" string.
    class Notification : public IRequest
    {
    public:
        Notification(std::string const& source, std::string const& message);
        std::string GetContents() override;

    protected:
        std::string guildId;
        std::string source;
        std::string message;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Notification, guildId, source, message)
    };
}

#endif // _MOD_CHAT_TRANSMITTER_REQUESTS_REQUEST_NOTIFICATION_H_
