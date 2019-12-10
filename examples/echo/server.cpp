#include <iostream>
#include <signal.h>
#include "tcpserver.h"
#include "tcpssl.h"
#include "echoserver.h"
#include "serveroptions.h"

EPoll epoll;
ProgramOptions options;
bool terminated {false};

void handle_signal(int signal) 
{
  if (signal == SIGHUP) {
    clog << "Caught SIGHUP. Shutting down" << endl;
    terminated = true;
  }
}

void initSSLFromOptions(EchoServer &server, ProgramOptions &options)
{
  if (server.ctx()) {
    server.ctx()->setOptions(options.verifypeer,!options.nocompression,options.tlsonly);
    server.ctx()->setVerifyPaths(options.cafile,options.capath);
    server.ctx()->setCertificateAndKey(options.certfile.c_str(),options.keyfile.c_str());
    server.ctx()->setPrivateKeyPassword(options.keypass);
  }
}

void closeSSL(SSLContext **ctx)
{
  if (*ctx) {
    delete *ctx;
    *ctx = nullptr;
  }
  freeSSLLibrary();
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

int main(int argc, char** argv) { 
  initSignals();
  
  ProgramOptions::statusReturn_e res = options.parseOptions(argc,argv);
  
  if (options.verbose) {
 //   options.dump();
  }

  if (res == ProgramOptions::OPTS_FAILURE) {
    return EXIT_FAILURE;
  }
  
  if (res == ProgramOptions::OPTS_INTERFACES) {
    EchoServer::printifaddrs();
    return EXIT_SUCCESS;
  }

  if (res != ProgramOptions::OPTS_SUCCESS) {
    return EXIT_SUCCESS;
  }
  
  SSLContext *ctx = nullptr;

  bool useSSL = (!options.certfile.empty() && !options.keyfile.empty());
  
  if (useSSL) {
    initSSLLibrary();
    ctx = new SSLContext(SSLMode::SERVER);
  }
  int domain = options.ip6 ? AF_INET6 : AF_INET;
  EchoServer server(epoll,ctx,domain);
  if (useSSL) {
    initSSLFromOptions(server,options);
  }
  server.start(options.port,options.interface.c_str(),useSSL);
  if (!server.listening()) {
    cerr << "Failed to start server" << endl;
    closeSSL(&ctx);
    return EXIT_FAILURE;
  }
  while (server.listening() && !terminated) {
    epoll.poll(100);
  }
  closeSSL(&ctx);
  return EXIT_SUCCESS;
}