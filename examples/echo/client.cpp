#include <iostream>
#include <thread>
#include <signal.h>
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
bool terminated {false};

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
  char *cafile {nullptr};
  char *capath {nullptr};
  client.verifyPeer = options.verifypeer;
  if (!options.cafile.empty()) {
    cafile = (char*)malloc(options.cafile.length()+1);
    strcpy(cafile,options.cafile.c_str());
  }
  if (!options.capath.empty()) {
    cafile = (char*)malloc(options.capath.length()+1);
    strcpy(capath,options.capath.c_str());
  }
  client.ctx().setVerifyPaths(cafile,capath);
  if (cafile)
    free(cafile);
  if (capath)
    free(capath);
  client.certfile = options.certfile;
  client.keyfile = options.keyfile;
  client.keypass = options.keypass;
}

void handle_signal(int signal) 
{
  if (signal == SIGHUP) {
    clog << "Caught SIGHUP. Shutting down" << endl;
    terminated = true;
  }
}

void initSignals()
{
  struct sigaction sa;
  sa.sa_handler = &handle_signal;
  sa.sa_flags = SA_RESTART;
  sigfillset(&sa.sa_mask);
  if (sigaction(SIGHUP, &sa, NULL) == -1) {
    cerr << "ERROR: cannot handle SIGHUP" << endl;
  }
  signal(SIGPIPE, SIG_IGN);
}

void run(EchoClient &client)
{
  std::thread threadObj(&threadfunc);

  cout << "Type 'quit' to exit" << endl;
  while (!terminated) {
    
    mtx.lock();
    if (!cmd.empty()) {
      if (cmd.compare("quit\n") == 0) {
        terminated = true;
      } else {
        client.write(cmd.c_str(),cmd.length());
      }
      cmd.clear();
    }
    mtx.unlock();

    if (client.state() == SocketState::DISCONNECTED) {
      terminated = true;
    }

    epoll.poll(100);
  }
  
  if (client.state() == SocketState::CONNECTED) {
    client.disconnect();
  }

  threadObj.detach();  
}

int main(int argc, char** argv) 
{
  initSignals();
  
  ProgramOptions::statusReturn_e res = options.parseOptions(argc,argv);
  
  if (options.verbose) {
    options.dump();
  }

  if (res == ProgramOptions::OPTS_FAILURE) {
    return EXIT_FAILURE;
  }
  
  if (res != ProgramOptions::OPTS_SUCCESS) {
    return EXIT_SUCCESS;
  }
  
  int domain = getDomainFromHostAndPort(options.host.c_str(),options.port.c_str(),options.ip6 ? AF_INET6 : AF_INET);

  initSSLLibrary();
  SSLContext ctx(SSLMode::CLIENT);
  EchoClient client(epoll,ctx,domain,options.blocking);
  initClientFromOptions(client,options);
  if (client.connect(options.host.c_str(),options.port.c_str())) {
    run(client);
  } else {
    cerr << "ERROR: Could not connect to " << options.host << " on port " << options.port << endl;
    freeSSLLibrary();
    return EXIT_FAILURE;
  }
  freeSSLLibrary();

  return EXIT_SUCCESS;
}