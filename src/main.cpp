#include <iostream>
#include "tcpserver.h"

using namespace std;
using namespace tcp;

//int main(int argc, char** argv) {  
int main() { 
    Server server(1883);
  if (!server.start()) {
    cerr << "Failed to start server" << endl;
    return 1;
  }
  while (!server.terminated()) {
    if (!server.poll(100)) break;
  }
  server.stop();

  return 0;
}