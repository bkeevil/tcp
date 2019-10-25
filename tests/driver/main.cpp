#include <iostream>
#include <cstdlib>
#include "echoserver.h"
#include "echoclient.h"
#include "tcpclient.h"

using namespace std;

EchoServer* server;
EchoClient* client;

int createServer() {
  server = new EchoServer(1200);
  if (server->listening()) {
    return EXIT_SUCCESS;
  } else {
    return EXIT_FAILURE;
  }
}

int destroyServer() {
  server = new EchoServer(1200);
  if (server != nullptr) {
    delete server;
    return EXIT_SUCCESS;
  } else {
    return EXIT_FAILURE;
  }
}

int createClient() {
  client = new EchoClient(false);
  if (client->state() == EchoClient::State::UNCONNECTED) {
    return EXIT_SUCCESS;
  } else {
    return EXIT_FAILURE;
  }
}

int destroyClient() {
  client = new EchoClient(false);
  if (client != nullptr) {
    delete client;
    return EXIT_SUCCESS;
  } else {
    return EXIT_FAILURE;
  }
}

int main(int argc, char** argv) {
  if (argc == 2) {
    if (strcmp(argv[1],"createServer") == 0) return createServer();
    if (strcmp(argv[1],"createClient") == 0) return createClient();
    if (strcmp(argv[1],"destroyServer") == 0) return destroyServer();
    if (strcmp(argv[1],"destroyClient") == 0) return destroyClient();
  } else {
    cerr << "Invalid number of arguments" << endl;
    return EXIT_FAILURE;
  }
}