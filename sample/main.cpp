#include "tcpserver.h"
#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[]) {
  int port = 8080;
  if (argc >= 2) {
    port = std::from_chars(argv[1], argv[1] + strlen(argv[1]), port).ptr ==
                   argv[1] + strlen(argv[1])
               ? port
               : 8080;
    if (port <= 0 || port > 65535) {
      std::cerr << "Invalid port number. Port must be between 1 and 65535. use "
                   "8080 as default"
                << std::endl;
      port = 8080;
    }
  }

  TcpServer server(port);
  server.run();
  return 0;
}