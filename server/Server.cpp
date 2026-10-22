#include "Server.h"
#include "Protocol.h"   //our own sendMessage() / recvMessage() helpers (length-prefixed messages)
#include <iostream>
#pragma comment(lib, "ws2_32.lib")

//CONSTRUCTOR: runs once, automatically, when we do "Server server(5000);" in main()
//sets up the socket and binds it, but does NOT start listening yet (that's run()'s job)
Server::Server(int port) : port(port), db("chat.db") {
    //": port(port)" -> this is called an initializer list
    //it sets the class's OWN "port" member variable using the "port" argument passed in
    //(same name used deliberately, this is a common C++ convention for constructors)

    //1. STARTING THE WINSOCK
    WSADATA wsaData;//Create variable of WSADATA datatype
    WSAStartup(MAKEWORD(2, 2), &wsaData);//Initializing Winsock, version 2.2 of API
    //Waking up the OS's Networking Department for the program to use.

    // 2. CREATING A SOCKET:
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);//Create a socket for:
    /*
    => AF_INET -> Use IPv4 (most common IP)
    => SOCK_STREAM -> Use TCP style stream socket (more reliable)
    => 0 -> Use appropriate protocol automatically, don't need to specify explicitly (TCP already locked in)

    => Protocol Numbers:
        6 -> TCP, 17 -> UDP
    */
    //NOTE: this is "serverSocket", not "SOCKET serverSocket" -> we're not creating a NEW variable here,
    //we're filling in the class's OWN member variable (declared in Server.h)

    // 3: BIND it to port 5000 (or whatever port was passed in) on all local interfaces
    sockaddr_in serverAddr{};//Initialize serverAddr, a struct type (sockaddr_in) variable, to all zero
    //this avoids garbage values sitting in serverAddr

    serverAddr.sin_family = AF_INET;//set socket internet family (sin_family) to IPv4.
    serverAddr.sin_addr.s_addr = INADDR_ANY;  // listen on all IPs of this machine.
        //.sin_addr -> a sockaddr_in member.
        //.s_addr -> the actual address value inside sin_addr.
        //INADDR_ANY -> listen on any IPv4 address this machine has.
    serverAddr.sin_port = htons(port);        // htons = convert port to network byte order
        //htons = "host to network short" -> converts port number into network byte order
        //NOTE: using "port" (the member variable) here, not hardcoded 5000 anymore -> more reusable

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));//bind() attaches the socket to a specific IP+port
        /*
        serverSocket -> the socket we created above
        (sockaddr*)&serverAddr -> pointer to serverAddr, cast to the generic type bind() expects
        sizeof(serverAddr) -> size of the struct in bytes, so bind() knows how much to read
        */
        //NOW server has an address
}

//BROADCAST: sends one message to EVERY connected user, except the one named in exceptUser
//(used so the sender doesn't get their own message echoed back)
void Server::broadcast(const std::string& msg, const std::string& exceptUser) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    //lock_guard locks clientsMutex right here, and unlocks it automatically when this function ends
    //while it's locked, no other thread can touch 'clients' -> prevents 2 threads changing the map at once

    for (auto& [name, sock] : clients) {
        //loops over every entry in the hashmap; "[name, sock]" unpacks each entry into its username + socket
        if (name != exceptUser) sendMessage(sock, msg);//skip the excepted user, send to everyone else
    }
}

//HANDLES ONE CLIENT'S ENTIRE CONVERSATION
//this runs on its OWN thread, so multiple clients can be handled "at the same time"
void Server::handleClient(SOCKET clientSocket) {
    //"Server::" prefix means this function belongs to the Server class
    //(needed in the .cpp file so C++ knows this handleClient is THIS class's version)

    std::string msg;    //holds each message we receive from this client

    // STEP 1: the very first message from a client MUST be "LOGIN|username"
    if (!recvMessage(clientSocket, msg) || msg.rfind("LOGIN|", 0) != 0) {
        //recvMessage() returns false if the client disconnected or something went wrong
        //msg.rfind("LOGIN|", 0) != 0 -> a trick for "does msg NOT start with LOGIN|"
        //(rfind with position 0 only checks the very start of the string)
        closesocket(clientSocket);//didn't log in properly, so drop them
        return;//ends this thread
    }
    std::string username = msg.substr(6);//"LOGIN|" is 6 characters, so everything after it is the username

        if (username.empty() || username.find('|') != std::string::npos || username.find(' ') != std::string::npos) {
        //spaces are blocked too now, because "/w name message" uses the first space to split name from message
        //'|' is our separator in messages, so a username containing it would break parsing
        //std::string::npos -> the value find() returns when it finds NOTHING
        sendMessage(clientSocket, "ERROR|Invalid username");
        closesocket(clientSocket);
        return;
    }

    // STEP 2: add this user to the hashmap (reject duplicate usernames)
    {   //these extra { } make the lock last ONLY for this block, not the whole function
        std::lock_guard<std::mutex> lock(clientsMutex);
        if (clients.count(username)) {
            //count() returns 1 if the username already exists in the map, 0 if not
            sendMessage(clientSocket, "ERROR|Username already taken");
            closesocket(clientSocket);
            return;//lock_guard still unlocks automatically, even on an early return
        }
        clients[username] = clientSocket;//store username -> socket in the hashmap
        sendMessage(clientSocket, "OK|Welcome, " + username);//tell the client login worked
         //send this new user the last 20 saved messages, so they can catch up
        std::vector<StoredMessage> history = db.getRecentMessages(20);
        if (!history.empty()) {
            sendMessage(clientSocket, "SYSTEM|--- last messages ---");
            for (auto& m : history) {
                sendMessage(clientSocket, "CHAT|" + m.sender + "|" + m.content);
            }
            sendMessage(clientSocket, "SYSTEM|--- end of history ---");
        }
    }

    std::cout << username << " joined\n";
    broadcast("SYSTEM|" + username + " joined the chat", username);//tell everyone ELSE someone joined

    // STEP 3: keep receiving messages from THIS client until they disconnect.
    while (recvMessage(clientSocket, msg)) {
        //recvMessage() blocks until a full message arrives, returns false when client disconnects

        size_t sep = msg.find('|');//position of the first '|' separating TYPE from the rest
        std::string type = (sep == std::string::npos) ? msg : msg.substr(0, sep);
        //everything BEFORE the first '|' is the message type (e.g. CHAT)
        std::string payload = (sep == std::string::npos) ? "" : msg.substr(sep + 1);
        //everything AFTER the first '|' is the actual content (e.g. the chat text)

        if (type == "CHAT") {
            std::cout << username << ": " << payload << "\n";
            db.logMessage(username, payload);//save it to the database
            broadcast("CHAT|" + username + "|" + payload, username);
            //forward to everyone else, stamped with WHO sent it
        }
        else if (type == "PM") {
            //payload looks like "targetName|message text"
            size_t sep2 = payload.find('|');
            if (sep2 != std::string::npos) {
                std::string target = payload.substr(0, sep2);
                std::string text = payload.substr(sep2 + 1);

                std::lock_guard<std::mutex> lock(clientsMutex);
                auto it = clients.find(target);//hashmap lookup by username, O(1) on average
                if (it == clients.end()) {
                    sendMessage(clientSocket, "ERROR|" + target + " is not online");
                } else {
                    sendMessage(it->second, "PM|" + username + "|" + text);//deliver to the target only
                    sendMessage(clientSocket, "SYSTEM|(to " + target + ") " + text);//confirm to the sender
                }
            }
            //private messages are deliberately NOT saved to the database
        }
        else if (type == "USERS") {
            std::string list;
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (auto& [name, sock] : clients) list += name + ",";
            if (!list.empty()) list.pop_back();//remove the trailing comma
            sendMessage(clientSocket, "USERS|" + list);
        }
        else if (type == "FILE_START" || type == "FILE_DATA" || type == "FILE_END") {
            //payload looks like "targetName|rest"
            //we only split at the FIRST '|', so any raw file bytes after it are never touched
            size_t sep2 = payload.find('|');
            std::string target = (sep2 == std::string::npos) ? payload : payload.substr(0, sep2);
            std::string rest = (sep2 == std::string::npos) ? "" : payload.substr(sep2 + 1);

            std::lock_guard<std::mutex> lock(clientsMutex);
            auto it = clients.find(target);//hashmap lookup, same as private messages
            if (it == clients.end()) {
                if (type == "FILE_START") sendMessage(clientSocket, "ERROR|" + target + " is not online");
            } else {
                sendMessage(it->second, type + "|" + username + "|" + rest);//forward, stamped with the sender
            }
        }
    }

    // STEP 4: the loop ended, so this client disconnected -> clean up
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(username);//remove them from the hashmap so nobody tries to send to a dead socket
    }
    closesocket(clientSocket);//this client is done talking, close their dedicated socket
    std::cout << username << " left\n";
    broadcast("SYSTEM|" + username + " left the chat");//tell everyone remaining (nobody skipped)
}

//STARTS the server: puts the socket into listening mode, then loops forever accepting clients
void Server::run() {
    listen(serverSocket, SOMAXCONN);//socket is now "on" and can receive incoming connection attempts
        //SOMAXCONN -> max number of connections allowed to WAIT IN QUEUE before being accept()-ed
        //(not a limit on total clients overall)
    std::cout << "Server waiting for connections on port " << port << "...\n";

    // keep accepting NEW clients forever, instead of just once
    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
            //accept() blocks until a client connects, then gives us a NEW socket dedicated to that client
            //serverSocket itself stays free to accept the NEXT client
        std::cout << "Client connected!\n";

            //hand this client off to its own thread running handleClient(),
            //so this while loop can immediately go back to accept() for the next client
            //instead of waiting for this conversation to finish first
        clientThreads.push_back(std::thread(&Server::handleClient, this, clientSocket));
            //&Server::handleClient -> "the handleClient function that belongs to the Server class"
            //this -> WHICH Server object it belongs to (needed since handleClient uses class members)
            //clientSocket -> the actual argument passed into handleClient
    }

    //unreachable in this simple version (while(true) never exits), but good practice to have:
    for (auto& t : clientThreads) {
        if (t.joinable()) t.join();//wait for each client thread to finish before the program truly exits
    }
}