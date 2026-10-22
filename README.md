# Your LAN Party

A LAN chat application written in C++17 with a Winsock server and a Qt 6 client.

## Features
- Multithreaded TCP server, one thread per client
- Length-prefixed message protocol
- Public chat, private messages (`/w`), online users list (`/users`)
- SQLite message history (last 20 messages sent to anyone who joins)
- File transfer between users in 32 KB chunks, saved to `received_files/`
- Console client and Qt GUI client

## Requirements
- Windows with MSYS2 UCRT64 (g++) and CMake 3.16+
- Qt 6.11.1 MinGW 64-bit (only needed for the GUI client)

## Build the server and console client
```
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_MAKE_PROGRAM=mingw32-make.exe
cmake --build .
```

## Run
```
server.exe
client.exe [server-ip]
```

Client commands:
- `/users` shows who is online
- `/w name message` sends a private message
- `/send name filepath` sends a file
- `/quit` leaves the chat

## GUI client
Open `qt_client/CMakeLists.txt` in Qt Creator, choose the Desktop Qt MinGW 64-bit kit, then build and run.

## Project structure
- `common/` shared message protocol
- `server/` server code
- `client/` console client
- `qt_client/` Qt GUI client
- `db/` SQLite wrapper class
- `third_party/sqlite/` SQLite source

## Author
Himal Thapa
