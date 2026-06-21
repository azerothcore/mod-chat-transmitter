#include "RequestNotification.h"
#include "../ChatTransmitter.h"

namespace ModChatTransmitter::Requests
{
    Notification::Notification(std::string const& source, std::string const& message)
        : guildId(ChatTransmitter::Instance().GetDiscordGuildId()),
        source(source),
        message(message)
    { }

    std::string Notification::GetContents()
    {
        nlohmann::json jsonObj;
        jsonObj["message"] = "notification";
        nlohmann::to_json(jsonObj["data"], *this);
        return nlohmann::to_string(jsonObj);
    }
}
