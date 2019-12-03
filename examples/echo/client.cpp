#include <iostream>
#include <thread>
#include "tcpsocket.h"
#include "tcpclient.h"
#include "echoclient.h"
#include "arpa/inet.h"

using namespace std;

EchoClient *cl;

void threadfunc() {
  char c[255];
  while (true) {
    memset(c,0,255);
    cin.getline(c,255);
    cl->write(c,strlen(c),true);
    cl->write("\n",1);
  }
}

int main() { 
  EchoClient client(AF_INET,true);
  cl = &client;
  client.useSSL = true;
  client.cafile = "/home/bkeevil/projects/sslserver/testkeys/testca.crt";
  client.certfile = "/home/bkeevil/projects/sslserver/testkeys/mqtt-client-test.crt";
  client.keyfile = "/home/bkeevil/projects/sslserver/testkeys/mqtt-client-test.key";
  client.connect(string("localhost"),1234);
  //client.write("Echo Server Hello World\n",25);
  std::thread threadObj(&threadfunc);
  while (true) {
    epoll.poll(100);
  }
  return 0;
}