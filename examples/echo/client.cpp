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
  client.verifyPeer = options.vm.count("verifypeer");
  const po::variable_value &cafile = options.vm["cafile"];
  const po::variable_value &capath = options.vm["capath"];
  client.ctx().setVerifyPaths(cafile.as<string>().c_str(),capath.as<string>().c_str());
  const po::variable_value &certfile = options.vm["certfile"];
  const po::variable_value &keyfile  = options.vm["keyfile"];
  const po::variable_value &keypass  = options.vm["keypass"];
  client.certfile = certfile.as<string>().c_str();
  client.keyfile = keyfile.as<string>().c_str();
  client.keypass = keypass.as<string>().c_str();
}

int main(int argc, char** argv) 
{
  if (options.parseOptions(argc,argv)) {
    options.dump();
  } else {
    return EXIT_FAILURE;
  }

  string host;
  string port;
  const bool blocking = options.vm.count("blocking");
  const bool ip6 = options.vm.count("ip6");
  const po::variable_value &vhost = options.vm["host"];
  const po::variable_value &vport = options.vm["port"];
  
  if (vport.empty()) {
    cerr << "No port or service name to connect to" << endl;
    return EXIT_FAILURE;
  } else {
    port = vport.as<string>();
  }
  
  if (!vhost.empty()) {
    host = vhost.as<string>();
  } else {
    if (ip6) {
      host.assign("::");
    } else {
      host.assign("localhost");
    }
  }

  int domain = getDomainFromHostAndPort(host.c_str(),port.c_str(),ip6 ? AF_INET6 : AF_INET);

  EchoClient client(epoll,domain,blocking);

  //initClientFromOptions(client,options);

  initSSLLibrary();

  client.connect(host.c_str(),port.c_str());
  
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