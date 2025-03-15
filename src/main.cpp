#include "tcpserver.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        std::cerr << "Invalid port number. Port must be between 1 and 65535. use 8080 as default" << std::endl;
        port = 8080;
    }

    TcpServer server(port);
    server.run();
    return 0;
}