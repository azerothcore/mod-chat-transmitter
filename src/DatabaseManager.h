#ifndef _MOD_CHAT_TRANSMITTER_DATABASE_MANAGER_H_
#define _MOD_CHAT_TRANSMITTER_DATABASE_MANAGER_H_

#include "PCQueue.h"
#include "Requests/RequestQueryResult.h"
#include "ChatTransmitterDatabaseConnection.h"
#include <utility>

class AsyncOperation;

namespace ModChatTransmitter
{
    enum QueryDatabase
    {
        Auth = 0,
        Characters = 1,
        World = 2,
        Eluna = 3,
    };

    class DatabaseManager
    {
    public:
        DatabaseManager();
        void Start();
        void QueueQuery(const std::string& id, const std::string& query, QueryDatabase dbType);
        bool GetResult(Requests::QueryResult*& outResult);
        void Stop();

    private:
        void HandleQuery(const std::string& id, const std::string& query, QueryDatabase dbType);
        void InitializeDatabase(const std::string& connectionString, MySQLConnectionInfo** connInfo, ProducerConsumerQueue<AsyncOperation*>** sqlQueue, ChatTransmitterDatabaseConnection** database);
        void InitializeDatabase(const MySQLConnectionInfo* srcConnInfo, MySQLConnectionInfo** connInfo, ProducerConsumerQueue<AsyncOperation*>** sqlQueue, ChatTransmitterDatabaseConnection** database);
        void CleanupDatabase(MySQLConnectionInfo* connInfo, ProducerConsumerQueue<AsyncOperation*>* sqlQueue, ChatTransmitterDatabaseConnection** database);
        bool IsValidUtf8(const Binary& data);
        void Cleanup();

        struct WorkItem
        {
            std::string id;
            std::string query;
            QueryDatabase dbType;

            WorkItem(std::string id, std::string query, QueryDatabase dbType)
                : id(std::move(id)),
                query(std::move(query)),
                dbType(dbType)
            { }
        };

        ProducerConsumerQueue<WorkItem*> workQueue;
        ProducerConsumerQueue<Requests::QueryResult*> results;

        uint32 dbPingInterval;
        long long nextDbPingTime;

        MySQLConnectionInfo* authConnInfo;
        ProducerConsumerQueue<AsyncOperation*>* authSqlQueue;
        ChatTransmitterDatabaseConnection* authDatabase;

        MySQLConnectionInfo* charsConnInfo;
        ProducerConsumerQueue<AsyncOperation*>* charsSqlQueue;
        ChatTransmitterDatabaseConnection* charsDatabase;

        MySQLConnectionInfo* worldConnInfo;
        ProducerConsumerQueue<AsyncOperation*>* worldSqlQueue;
        ChatTransmitterDatabaseConnection* worldDatabase;

        MySQLConnectionInfo* elunaConnInfo;
        ProducerConsumerQueue<AsyncOperation*>* elunaSqlQueue;
        ChatTransmitterDatabaseConnection* elunaDatabase;

        std::atomic<bool> stop;
    };
}

#endif // _MOD_CHAT_TRANSMITTER_DATABASE_MANAGER_H_
