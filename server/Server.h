#pragma once 
/*#pragma once prevents this header from being 
included twice by accident (a standard header guard)*/
#include <winsock2.h>
#include <thread>
#include <vector>
#include <string>
#include <unordered_map>    //hashmap: lets us find a user by username in O(1) average time
#include <mutex>            //std::mutex, stops 2 threads from touching the same data at the same time
#include "Database.h"       //our SQLite wrapper
//A Server "knows how" to listen on a port and handle multiple clients.
//All the data it needs, and all the actions it can do, live together in this class.

/*The class just groups together what a server 
needs to remember (port, serverSocket, clientThreads)*/
class Server{

    public:
    Server(int port);   //Constructor: SET UP SERVER ON THIS PORT
    void run();         //starts listening and accepting clients forever

    private:
    void handleClient(SOCKET clientSocket); //handles 1 client's messages
    /*it's an internal implementation detail, only run() uses it.*/

    void broadcast(const std::string& msg, const std::string& exceptUser = "");
    //sends msg to every connected user EXCEPT exceptUser ("" means nobody is skipped)
    //const std::string& -> passed by reference (no copy made) and const (function can't change it)
    
    int port;   //which port this server listens on
    SOCKET serverSocket;    //the main listening socket (not tied to any one client)
    std::vector<std::thread> clientThreads; //keeps track of every client's thread, so we could join() them later

    std::unordered_map<std::string, SOCKET> clients;    //username -> that user's socket (our hashmap)
    std::mutex clientsMutex;    //every client thread shares 'clients', so each access is locked with this

    Database db;    //the message history database (chat.db)
};