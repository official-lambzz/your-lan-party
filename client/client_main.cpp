#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>   //used to get a safe file name and create the received_files folder
#include <atomic>
#include <cstdlib>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Protocol.h"

std::atomic<bool> sendFailed{false};//set to true if the server says the target isn't online

//state of the file currently being RECEIVED (only the receiver thread touches these)
std::ofstream incomingFile;
unsigned long long incomingSize = 0;
unsigned long long incomingGot = 0;
std::string incomingName;

//turns "CHAT|bob|hi" into "bob: hi", and other types into "[text]"
std::string display(const std::string& msg) {
    size_t a = msg.find('|');
    if (a == std::string::npos) return msg;
    std::string type = msg.substr(0, a), rest = msg.substr(a + 1);
    if (type == "CHAT") {
        size_t b = rest.find('|');
        return rest.substr(0, b) + ": " + rest.substr(b + 1);
    }
    if (type == "PM") {
        size_t b = rest.find('|');
        return "(private) " + rest.substr(0, b) + ": " + rest.substr(b + 1);
    }
    if (type == "USERS") return "Online: " + rest;
    return "[" + rest + "]";
}

//handles the 3 file message types, returns true if msg was a file message
bool handleIncomingFile(const std::string& msg) {
    if (msg.rfind("FILE_START|", 0) == 0) {
        //FILE_START|sender|filename|size
        std::string rest = msg.substr(11);
        size_t a = rest.find('|');
        size_t b = (a == std::string::npos) ? a : rest.find('|', a + 1);
        if (a == std::string::npos || b == std::string::npos) return true;//malformed, ignore

        std::string sender = rest.substr(0, a);
        std::string fname = std::filesystem::path(rest.substr(a + 1, b - a - 1)).filename().string();
        //.filename() keeps only the file name, so a sender can't write outside received_files
        incomingSize = std::stoull(rest.substr(b + 1));
        incomingGot = 0;

        std::filesystem::create_directories("received_files");
        incomingName = "received_files/" + fname;
        incomingFile.open(incomingName, std::ios::binary);
        std::cout << "\r" << sender << " is sending you " << fname << " (" << incomingSize << " bytes)\n> " << std::flush;
        return true;
    }
    if (msg.rfind("FILE_DATA|", 0) == 0) {
        //FILE_DATA|sender|<raw bytes>
        size_t a = msg.find('|', 10);//the '|' right after the sender name
        if (a == std::string::npos) return true;
        incomingFile.write(msg.data() + a + 1, msg.size() - a - 1);
        incomingGot += msg.size() - a - 1;
        unsigned long long pct = incomingSize ? incomingGot * 100 / incomingSize : 100;
        std::cout << "\rReceiving: " << pct << "%   " << std::flush;
        return true;
    }
    if (msg.rfind("FILE_END|", 0) == 0) {
        incomingFile.close();
        std::cout << "\rSaved to " << incomingName << "          \n> " << std::flush;
        return true;
    }
    return false;
}

//runs on its own thread so incoming messages show up while you're typing
void receiveLoop(SOCKET s) {
    std::string msg;
    while (recvMessage(s, msg)) {
        if (handleIncomingFile(msg)) continue;//file messages are not printed as chat
        if (msg.rfind("ERROR|", 0) == 0) sendFailed = true;
        std::cout << "\r" << display(msg) << "\n> " << std::flush;
    }
    std::cout << "\nDisconnected from server.\n";
    std::exit(0);
}

//reads a file from disk and sends it to 'target' in 32 KB chunks
void sendFile(SOCKET s, const std::string& target, const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);//ate -> start at the END, so tellg() gives the file size
    if (!in) {
        std::cout << "Could not open file: " << path << "\n";
        return;
    }
    unsigned long long size = (unsigned long long)in.tellg();
    in.seekg(0);//go back to the start before reading

    std::string fname = std::filesystem::path(path).filename().string();
    sendFailed = false;
    sendMessage(s, "FILE_START|" + target + "|" + fname + "|" + std::to_string(size));

    const size_t CHUNK = 32 * 1024;
    std::string header = "FILE_DATA|" + target + "|";
    std::vector<char> buf(CHUNK);
    unsigned long long sent = 0;

    while (in && !sendFailed) {
        in.read(buf.data(), CHUNK);
        std::streamsize n = in.gcount();//how many bytes were actually read this time
        if (n <= 0) break;
        if (!sendMessage(s, header + std::string(buf.data(), (size_t)n))) {
            std::cout << "\nConnection lost.\n";
            return;
        }
        sent += (unsigned long long)n;
        std::cout << "\rSending: " << (size ? sent * 100 / size : 100) << "%   " << std::flush;
    }
    sendMessage(s, "FILE_END|" + target);
    std::cout << (sendFailed ? "\nTransfer failed.\n" : "\nDone.\n");
}

int main(int argc, char* argv[]) {
    std::string ip = (argc > 1) ? argv[1] : "127.0.0.1";//optional: client.exe 192.168.x.x

    std::string username;
    std::cout << "Username: ";
    std::getline(std::cin, username);

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(5000);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    if (connect(s, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        std::cout << "Could not connect to server.\n";
        return 1;
    }

    sendMessage(s, "LOGIN|" + username);
    std::thread(receiveLoop, s).detach();

    std::string line;
    std::cout << "> ";
    while (std::getline(std::cin, line)) {
        if (line == "/quit") break;
        if (line == "/users") {
            sendMessage(s, "USERS|");
        } else if (line.rfind("/w ", 0) == 0) {
            std::string rest = line.substr(3);
            size_t sp = rest.find(' ');
            if (sp == std::string::npos) std::cout << "Usage: /w name message\n";
            else sendMessage(s, "PM|" + rest.substr(0, sp) + "|" + rest.substr(sp + 1));
        } else if (line.rfind("/send ", 0) == 0) {
            std::string rest = line.substr(6);
            size_t sp = rest.find(' ');
            if (sp == std::string::npos) {
                std::cout << "Usage: /send name filepath\n";
            } else {
                std::string path = rest.substr(sp + 1);
                if (path.size() >= 2 && path.front() == '"' && path.back() == '"')
                    path = path.substr(1, path.size() - 2);//remove quotes if the path was pasted with them
                sendFile(s, rest.substr(0, sp), path);
            }
        } else if (!line.empty()) {
            sendMessage(s, "CHAT|" + line);
        }
        std::cout << "> ";
    }

    closesocket(s);
    WSACleanup();
    return 0;
}