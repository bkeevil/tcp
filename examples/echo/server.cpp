#include <iostream>
#include "tcpserver.h"
#include "tcpssl.h"
#include "echoserver.h"

int main() { 
  initSSLLibrary();
  EchoServer server(AF_INET);
  server.bindaddress = "lo";
  server.port = 1234;
  server.ctx().setVerifyPaths("/home/bkeevil/projects/sslserver/testkeys/testca.crt",NULL);
  server.ctx().setCertificateAndKey(
    "/home/bkeevil/projects/sslserver/testkeys/mqtt-server-test.crt",
    "/home/bkeevil/projects/sslserver/testkeys/mqtt-server-test.key");
  server.useSSL = true;
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