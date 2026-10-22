#include "Server.h"

//main() is now just 3 lines: create a Server, run it.
//All the actual logic lives inside the Server class (Server.h / Server.cpp)
int main() {
    Server server(5000);//create a Server object, listening on port 5000
    server.run();       //start listening and accepting clients forever
    return 0;
}