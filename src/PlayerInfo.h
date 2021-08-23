#ifndef _MOD_CHAT_TRANSMITTER_PLAYER_INFO_H_
#define _MOD_CHAT_TRANSMITTER_PLAYER_INFO_H_

#include <string>

namespace ModChatTransmitter
{
    struct PlayerInfo
    {
    public:
        PlayerInfo(Player* player)
          : name(player->GetName()),
            guid(player->GetGUID().GetCounter()),
            level(player->getLevel()),
            raceId(player->getRace()),
            classId(player->getClass()),
            accountGuid(player->GetSession()->GetAccountId()),
            lastIpAddr(player->GetSession()->GetRemoteAddress())
        {
            AccountMgr::GetName(accountGuid, accountName);
        }

        PlayerInfo& operator=(const PlayerInfo& other)
        {
            name = other.name;
            guid = other.guid;
            level = other.level;
            raceId = other.raceId;
            classId = other.classId;
            accountName = other.accountName;
            accountGuid = other.accountGuid;

            return *this;
        }

        std::string name;
        uint32 guid;
        uint8 level;
        uint8 raceId;
        uint8 classId;
        std::string accountName;
        uint32 accountGuid;
        std::string lastIpAddr;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(PlayerInfo, name, guid, level, raceId, classId, accountName, accountGuid, lastIpAddr)
    };
}

#endif // _MOD_CHAT_TRANSMITTER_PLAYER_INFO_H_
