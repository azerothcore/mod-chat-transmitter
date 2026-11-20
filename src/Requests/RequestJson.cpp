#include "RequestJson.h"
#include "../ChatTransmitter.h"

namespace ModChatTransmitter::Requests
{
    Json::Json(std::string const& message, nlohmann::json data)
        : message(message),
        data(std::move(data))
    {
        if (!this->data.contains("guildId"))
        {
            this->data["guildId"] = ChatTransmitter::Instance().GetDiscordGuildId();
        }
    }

    std::string Json::GetContents()
    {
        nlohmann::json jsonObj;
        jsonObj["message"] = message;
        jsonObj["data"] = data;
        // Replace bad UTF-8 instead of throwing: names and chat come from players.
        return jsonObj.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    }
}
