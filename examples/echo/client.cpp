#include <iostream>
#include <fstream>
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
streambuf *oldclog;
streambuf *oldcerr;

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

void initSSLFromOptions(EchoClient &client, ProgramOptions &options)
{
  if (client.ctx()) {
    client.ctx()->setOptions(options.verifypeer,!options.nocompression,options.tlsonly);
    client.ctx()->setVerifyPaths(options.cafile,options.capath);
  }
  client.verifyPeer = options.verifypeer;
  client.checkPeerSubjectName = options.checkhostname;
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

void closeSSL(SSLContext **ctx)
{
  if (*ctx) {
    delete *ctx;
    *ctx = nullptr;
  }
  freeSSLLibrary();
}

int main(int argc, char** argv) 
{
  initSignals();
  
  ProgramOptions::statusReturn_e res = options.parseOptions(argc,argv);
  
  if (options.verbose) {
//    options.dump();
  }

  if (res == ProgramOptions::OPTS_FAILURE) {
    return EXIT_FAILURE;
  }
  
  if (res != ProgramOptions::OPTS_SUCCESS) {
    return EXIT_SUCCESS;
  }

  ofstream *os = nullptr ;
  if (!options.log.empty()) {
    os = new ofstream(options.log.c_str(),ios_base::app);
    setLogStream(os);
  }  

  int domain = getDomainFromHostAndPort(options.host.c_str(),options.port.c_str(),options.ip6 ? AF_INET6 : AF_INET);

  SSLContext *ctx;
  
  if (options.useSSL) {
    initSSLLibrary();
    ctx = new SSLContext(SSLMode::CLIENT);
  }
  
  EchoClient client(epoll,ctx,domain,options.blocking);
  
  if (options.useSSL) {
    initSSLFromOptions(client,options);
  }
  
  if (client.connect(options.host.c_str(),options.port.c_str())) {
    run(client);
  } else {
    cerr << "ERROR: Could not connect to " << options.host << " on port " << options.port << endl;
    if (os) {
      delete os;
    }
    if (options.useSSL) {
      closeSSL(&ctx);
    }
    return EXIT_FAILURE;
  }

  if (options.useSSL) {
    closeSSL(&ctx);
  }
    
  if (os) {
    delete(os);
  }
  return EXIT_SUCCESS;
}