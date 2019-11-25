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
    cl->write(c,255);
    *cl << endl;
    cl->flush();
    //cl->clear();
  }
}

int main() { 
  EchoClient client(AF_INET6,true);
  cl = &client;
  client.connect(string("::1/128"),1200);
  client << "Echo Server Hello World" << endl;
  client.flush();
  std::thread threadObj(&threadfunc);
  while (true) {
    epoll.poll(100);
  }
  return 0;
}