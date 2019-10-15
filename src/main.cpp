#include <iostream>
#include "tcp.h"

using namespace std;
using namespace tcp;

int main(int argc, char** argv) {  
  Server server(1883);
  if (!server.start()) {
    cerr << "Failed to start server" << endl;
    return 1;
  }
  while (!server.terminated()) {
    if (!server.poll()) break;
  }
  server.stop();
  return 0;
}