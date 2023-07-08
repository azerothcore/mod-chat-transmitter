#ifndef _MOD_CHAT_TRANSMITTER_DATABASE_CONNECTION_H_
#define _MOD_CHAT_TRANSMITTER_DATABASE_CONNECTION_H_

#include "Database/MySQLConnection.h"

class ChatTransmitterDatabaseConnection : public MySQLConnection
{
public:
    explicit ChatTransmitterDatabaseConnection(MySQLConnectionInfo& connInfo);
    ChatTransmitterDatabaseConnection(MySQLConnectionInfo& connInfo, ProducerConsumerQueue<AsyncOperation*>* dbQueue);

    ~ChatTransmitterDatabaseConnection() override;

    std::string GetLastErrorString();
    uint64 GetAffectedRows();

protected:
    bool HandleMySQLError(uint32 errNo, uint8 attempts = 5) override;
};

#endif
