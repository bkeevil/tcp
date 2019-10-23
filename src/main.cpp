#include <iostream>
#include "tcpsocket.h"
#include "tcpserver.h"

using namespace std;
using namespace tcp;

//int main(int argc, char** argv) {  
int main() { 
  Server server(1883);
  if (!server.listening()) {
    cerr << "Failed to start server" << endl;
    return 1;
  }
  while (server.listening()) {
    epoll.poll(100);
  }

  return 0;
}