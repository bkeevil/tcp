#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <iostream>
#include <sstream>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "tcpsocket.h"

namespace tcp {

using namespace std;

class Client : public Socket {
  public:
    Client() = default;
    ~Client() { if (connected_) disconnect(); };
    bool socket() { return socket_; }
    bool isConnected() { return connected_; }
    in_port_t port() { return addr_; }
    in_addr_t address() { return port_; }
    virtual bool connect(in_addr_t addr, in_port_t port);
    virtual void disconnect();
  protected:
    int send(const void* buf, const size_t buf_size, bool more = true);
    virtual int receive(const void* buf, const size_t buf_size);
    virtual void connected();
    virtual void disconnected();
    void dataAvailable();  
    int availableForRead();
  private:
    int socket_ {0};
    bool connected_ {false};
    in_port_t port_ {0};
    in_addr_t addr_ {0};    
};

}

#endif