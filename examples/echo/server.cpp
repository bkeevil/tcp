#include <iostream>
#include "echoserver.h"

int main() { 
  EchoServer server(AF_INET6,0,"::");
  server.printifaddrs();
  if (!server.listening()) {
    cerr << "Failed to start server" << endl;
    return 1;
  }
  while (server.listening()) {
    epoll.poll(100);
  }

  return 0;
}