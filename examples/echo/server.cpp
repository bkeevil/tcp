#include <iostream>
#include "tcpssl.h"
#include "echoserver.h"

int main() { 
  initSSLLibrary();
  EchoServer server(AF_INET);
  server.bindaddress = "lo";
  server.port = 1234;
  server.cafile = "/home/bkeevil/projects/sslserver/testkeys/testca.crt";
  server.certfile = "/home/bkeevil/projects/sslserver/testkeys/mqtt-server-test.crt";
  server.keyfile = "/home/bkeevil/projects/sslserver/testkeys/mqtt-server-test.key";
  server.useSSL(true);
  server.start();
  //server.printifaddrs();
  if (!server.listening()) {
    cerr << "Failed to start server" << endl;
    return 1;
  }
  while (server.listening()) {
    epoll.poll(100);
  }
  freeSSLLibrary();
  return 0;
}