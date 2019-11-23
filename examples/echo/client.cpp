#include <iostream>
#include "tcpsocket.h"
#include "tcpclient.h"
#include "echoclient.h"
#include "arpa/inet.h"

int main() { 
  EchoClient client(true);
  client.connect(string("127.0.0.1"),1200);
  while (true) {
    if (!cin.eof()) {
      cin >> client.rdbuf();
    }
    epoll.poll(100);
  }
  return 0;
}