#pragma once
#include <string>
#include <vector>
#include <mutex>
#include "sqlite3.h"

//one row from the messages table
struct StoredMessage {
    std::string sender;
    std::string content;
};

class Database {
public:
    Database(const std::string& path);  //opens (or creates) the database file
    ~Database();                        //closes it

    void logMessage(const std::string& sender, const std::string& content);  //INSERT one chat message
    std::vector<StoredMessage> getRecentMessages(int limit);                 //SELECT the last 'limit' messages

private:
    sqlite3* db;        //the open database connection
    std::mutex dbMutex; //many client threads use the database, so every access is locked
};