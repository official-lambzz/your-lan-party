#include "Database.h"
#include <iostream>

int main() {
    Database db("test_chat.db");
    db.logMessage("aryan", "hello everyone");
    db.logMessage("rahul", "hi aryan");
    db.logMessage("aryan", "send me the file");

    for (auto& m : db.getRecentMessages(2))
        std::cout << m.sender << ": " << m.content << "\n";
}