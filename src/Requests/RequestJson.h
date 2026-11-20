#ifndef _MOD_CHAT_TRANSMITTER_REQUESTS_REQUEST_JSON_H_
#define _MOD_CHAT_TRANSMITTER_REQUESTS_REQUEST_JSON_H_

#include <nlohmann/json.hpp>

#include "../IRequest.h"

namespace ModChatTransmitter::Requests
{
    // A message to the bot built from any JSON object: {"message": <name>, "data": <data>}.
    // Used by the newer messages (gameEvent, guildChat, linkResult) so each one does not need its own class.
    class Json : public IRequest
    {
    public:
        Json(std::string const& message, nlohmann::json data);
        std::string GetContents() override;

    protected:
        std::string message;
        nlohmann::json data;
    };
}

#endif // _MOD_CHAT_TRANSMITTER_REQUESTS_REQUEST_JSON_H_
