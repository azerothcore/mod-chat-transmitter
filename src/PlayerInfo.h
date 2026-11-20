#ifndef _MOD_CHAT_TRANSMITTER_PLAYER_INFO_H_
#define _MOD_CHAT_TRANSMITTER_PLAYER_INFO_H_

#include <string>

#include <Player.h>
#include "AccountMgr.h"
#include <nlohmann/json.hpp>

namespace ModChatTransmitter
{
    struct PlayerInfo
    {
    public:
        PlayerInfo(Player* player)
          : name(player->GetName()),
            guid(player->GetGUID().GetCounter()),
            level(player->GetLevel()),
            raceId(player->getRace()),
            classId(player->getClass()),
            gender(player->getGender()),
            accountGuid(0),
            lastIpAddr("")
        {
            // Account names, account ids and IP addresses stay on the server unless
            // ChatTransmitter.SendAccountInfo is 1 (only for a bot that only staff can see).
            if (SendPrivateAccountInfo())
            {
                accountGuid = player->GetSession()->GetAccountId();
                lastIpAddr = player->GetSession()->GetRemoteAddress();
                AccountMgr::GetName(accountGuid, accountName);
            }
        }

        static bool SendPrivateAccountInfo();

        PlayerInfo& operator=(const PlayerInfo& other)
        {
            name = other.name;
            guid = other.guid;
            level = other.level;
            raceId = other.raceId;
            classId = other.classId;
            gender = other.gender;
            accountName = other.accountName;
            accountGuid = other.accountGuid;
            lastIpAddr = other.lastIpAddr;

            return *this;
        }

        std::string name;
        uint32 guid;
        uint8 level;
        uint8 raceId;
        uint8 classId;
        uint8 gender;
        std::string accountName;
        uint32 accountGuid;
        std::string lastIpAddr;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(PlayerInfo, name, guid, level, raceId, classId, gender, accountName, accountGuid, lastIpAddr)
    };
}

#endif // _MOD_CHAT_TRANSMITTER_PLAYER_INFO_H_
