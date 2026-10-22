#include "Database.h"
#include <iostream>

//CONSTRUCTOR: opens chat.db (creates it if it doesn't exist) and makes sure the table exists
Database::Database(const std::string& path) : db(nullptr) {
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Could not open database: " << sqlite3_errmsg(db) << "\n";
        return;
    }

    //"IF NOT EXISTS" -> safe to run every time the server starts
    const char* sql =
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"      //unique number, goes up by itself
        "sender TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "sent_at TEXT DEFAULT (datetime('now','localtime')));";  //timestamp filled in automatically

    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "Could not create table: " << err << "\n";
        sqlite3_free(err);
    }
}

Database::~Database() {
    if (db) sqlite3_close(db);
}

//saves one chat message
void Database::logMessage(const std::string& sender, const std::string& content) {
    std::lock_guard<std::mutex> lock(dbMutex);//only one thread in the database at a time
    if (!db) return;

    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO messages (sender, content) VALUES (?, ?);";
    //the two ? are placeholders, filled in safely below
    //(a "prepared statement" -> stops SQL injection, the text is never pasted into the SQL itself)

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, sender.c_str(), -1, SQLITE_TRANSIENT);   //1st ? = sender
    sqlite3_bind_text(stmt, 2, content.c_str(), -1, SQLITE_TRANSIENT);  //2nd ? = content
    sqlite3_step(stmt);        //actually runs the INSERT
    sqlite3_finalize(stmt);    //frees the statement
}

//returns the last 'limit' messages, oldest first
std::vector<StoredMessage> Database::getRecentMessages(int limit) {
    std::lock_guard<std::mutex> lock(dbMutex);
    std::vector<StoredMessage> result;
    if (!db) return result;

    //inner query grabs the newest 'limit' rows, outer query flips them back into oldest-first order
    const char* sql =
        "SELECT sender, content FROM "
        "(SELECT * FROM messages ORDER BY id DESC LIMIT ?) ORDER BY id ASC;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;
    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {  //each step() moves to the next row
        StoredMessage m;
        m.sender = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));   //column 0 = sender
        m.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));  //column 1 = content
        result.push_back(m);
    }
    sqlite3_finalize(stmt);
    return result;
}