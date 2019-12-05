#include <iostream>
#include <thread>
#include "tcpsocket.h"
#include "tcpclient.h"
#include "tcpssl.h"
#include "echoclient.h"
#include "arpa/inet.h"

using namespace std;

EchoClient *cl;

void threadfunc() {
  char c[255];
  while (true) {
    memset(c,0,256);
    cin.getline(c,255);
    c[strlen(c)] = '\n';
    cl->write(c,strlen(c));
  }
}

int main() { 
  initSSLLibrary();
  EchoClient client(AF_INET,false);
  cl = &client;
  client.ctx().setVerifyPaths("/home/bkeevil/projects/sslserver/testkeys/testca.crt",NULL);
  client.certfile = "/home/bkeevil/projects/sslserver/testkeys/mqtt-client-test.crt";
  client.keyfile = "/home/bkeevil/projects/sslserver/testkeys/mqtt-client-test.key";
  //client.write("Echo Server Hello World\n",25);
  client.connect("localhost",1234,true);
  std::thread threadObj(&threadfunc);
  while (true) {
    epoll.poll(100);
  }
  freeSSLLibrary();
  threadObj.join();
  return 0;
}