#include <iostream>
#include <cstdlib>
#include "echoserver.h"
#include "echoclient.h"
#include "tcpclient.h"

using namespace std;

int createServer() {
  EchoServer server(1200);
  if (server.listening()) {
    return EXIT_SUCCESS;
  } else {
    return EXIT_FAILURE;
  }
}

int createClient() {
  EchoClient client(false);
  if (client.state() == EchoClient::State::UNCONNECTED) {
    return EXIT_SUCCESS;
  } else {
    return EXIT_FAILURE;
  }
}

int main(int argc, char** argv) {
  if (argc == 2) {
    if (strcmp(argv[1],"createServer") == 0) return createServer();
    if (strcmp(argv[1],"createClient") == 0) return createClient();
  } else {
    cerr << "Invalid number of arguments" << endl;
    return EXIT_FAILURE;
  }
}