#include <iostream>
#include <thread>
#include "tcpsocket.h"
#include "tcpclient.h"
#include "tcpssl.h"
#include "echoclient.h"
#include "arpa/inet.h"

using namespace std;

mutex mtx;
string cmd;
EPoll epoll;

void threadfunc() {
  char c[255];
  while (true) {
    memset(c,0,256);
    cin.getline(c,255);
    c[strlen(c)] = '\n';
    mtx.lock();
    cmd.assign(c);
    mtx.unlock();
  }
}

int main() { 
  initSSLLibrary();
  EchoClient client(epoll,AF_INET,false);
  client.ctx().setVerifyPaths("/home/bkeevil/projects/sslserver/testkeys/testca.crt",NULL);
  client.certfile = "/home/bkeevil/projects/sslserver/testkeys/mqtt-client-test.crt";
  client.keyfile = "/home/bkeevil/projects/sslserver/testkeys/mqtt-client-test.key";
  //client.write("Echo Server Hello World\n",25);
  client.connect("localhost",1234,true);
  std::thread threadObj(&threadfunc);
  while (true) {
    epoll.poll(100);
    mtx.lock();
    if (!cmd.empty()) {
      client.write(cmd.c_str(),cmd.length());
      cmd.clear();
    }
    mtx.unlock();
  }
  freeSSLLibrary();
  threadObj.join();
  return 0;
}