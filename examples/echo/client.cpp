#include <iostream>
#include <thread>
#include "arpa/inet.h"
#include "tcpsocket.h"
#include "tcpclient.h"
#include "tcpssl.h"
#include "echoclient.h"
#include "clientoptions.h"

using namespace std;

mutex mtx;
string cmd;
EPoll epoll;
ProgramOptions options;

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

void initClientFromOptions(EchoClient &client, ProgramOptions &options)
{
  client.verifyPeer = options.verifypeer;
  client.ctx().setVerifyPaths(options.cafile.c_str(),options.capath.c_str());
  client.certfile = options.certfile;
  client.keyfile = options.keyfile;
  client.keypass = options.keypass;
}

int main(int argc, char** argv) 
{
  ProgramOptions::statusReturn_e res = options.parseOptions(argc,argv);
  options.dump();

  if (res == ProgramOptions::OPTS_FAILURE) {
    return EXIT_FAILURE;
  }
  
  if (res == ProgramOptions::OPTS_SUCCESS) {
    return EXIT_SUCCESS;
  }

  int domain = getDomainFromHostAndPort(options.host.c_str(),options.port.c_str(),options.ip6 ? AF_INET6 : AF_INET);

  SSLContext ctx(SSLMode::CLIENT);

  EchoClient client(epoll,ctx,domain,options.blocking);

  //initClientFromOptions(client,options);

  initSSLLibrary();

  client.connect(options.host.c_str(),options.port.c_str());
  
  std::thread threadObj(&threadfunc);

  cout << "Type 'quit' to exit" << endl;
  while (client.state() != SocketState::DISCONNECTED) {
    epoll.poll(100);
    mtx.lock();
    if (!cmd.empty()) {
      if (cmd.compare("quit") == 0) {
        client.disconnect();
      } else {
        client.write(cmd.c_str(),cmd.length());
      }
      cmd.clear();
    }
    mtx.unlock();
  }
  
  threadObj.join();
  
  freeSSLLibrary();

  return 0;
}