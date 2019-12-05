#include <iostream>
#include "tcpserver.h"
#include "tcpssl.h"
#include "echoserver.h"

int main() { 
  initSSLLibrary();
  EchoServer server(AF_INET);
  server.ctx().setVerifyPaths("/home/bkeevil/projects/sslserver/testkeys/testca.crt",NULL);
  server.ctx().setCertificateAndKey(
    "/home/bkeevil/projects/sslserver/testkeys/mqtt-server-test.crt",
    "/home/bkeevil/projects/sslserver/testkeys/mqtt-server-test.key");
  //server.printifaddrs();
  server.start(1234,string("lo"),true);
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