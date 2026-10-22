//TCP is a stream, so two messages can arrive glued together, or one can arrive in pieces. 
//To avoid that, every message gets a 4-byte length in front of it. 
//This also makes file transfer possible later.

#pragma once
#include <winsock2.h>
#include <string>
#include <cstdint>

//send exactly 'len' bytes (send() may send fewer, so we loop until all are out)
inline bool sendAll(SOCKET s, const char* data, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(s, data + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

//receive exactly 'len' bytes (recv() may return fewer, so we loop until we have all)
inline bool recvAll(SOCKET s, char* data, int len) {
    int got = 0;
    while (got < len) {
        int n = recv(s, data + got, len - got, 0);
        if (n <= 0) return false;
        got += n;
    }
    return true;
}

//one message on the wire = [4 byte length][text]
inline bool sendMessage(SOCKET s, const std::string& msg) {
    uint32_t len = htonl((uint32_t)msg.size());//htonl = host to network (long)
    if (!sendAll(s, (const char*)&len, 4)) return false;
    return sendAll(s, msg.data(), (int)msg.size());
}

inline bool recvMessage(SOCKET s, std::string& out) {
    uint32_t len;
    if (!recvAll(s, (char*)&len, 4)) return false;
    len = ntohl(len);
    if (len > (1u << 20)) return false;//reject anything over 1 MB, protects against garbage
    out.resize(len);
    if (len == 0) return true;
    return recvAll(s, &out[0], (int)len);
}

//Message format: client sends LOGIN|name or CHAT|text. Server sends CHAT|sender|text, SYSTEM|text, OK|text or ERROR|text.