#include "ChatTransmitterDatabaseConnection.h"
#include <mysql.h>
#include <mysqld_error.h>

class MySQLPreparedStatement {};

ChatTransmitterDatabaseConnection::ChatTransmitterDatabaseConnection(MySQLConnectionInfo& connInfo)
    : MySQLConnection(connInfo, nullptr, false)
{
}

ChatTransmitterDatabaseConnection::ChatTransmitterDatabaseConnection(MySQLConnectionInfo& connInfo, ProducerConsumerQueue<AsyncOperation*>* dbQueue)
    : MySQLConnection(connInfo, dbQueue, false)
{
}

ChatTransmitterDatabaseConnection::~ChatTransmitterDatabaseConnection() = default;

bool ChatTransmitterDatabaseConnection::HandleMySQLError(uint32 errNo, uint8 attempts)
{
    switch (errNo)
    {
        case ER_BAD_FIELD_ERROR:
        case ER_NO_SUCH_TABLE:
        case ER_PARSE_ERROR:
            return false;
        default:
            return MySQLConnection::HandleMySQLError(errNo, attempts);
    }
}

std::string ChatTransmitterDatabaseConnection::GetLastErrorString()
{
    const char* err = mysql_error(reinterpret_cast<MYSQL*>(_mysqlHandle));
    return std::string(err);
}

uint64 ChatTransmitterDatabaseConnection::GetAffectedRows()
{
    return mysql_affected_rows(reinterpret_cast<MYSQL*>(_mysqlHandle));
}
