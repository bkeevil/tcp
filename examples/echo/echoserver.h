#ifndef ECHOSERVER_H
#define ECHOSERVER_H

#include "tcpsocket.h"
#include "tcpserver.h"

using namespace std;
using namespace tcp;

class EchoServer : public tcp::Server {
  public:
    EchoServer(EPoll &epoll, SSLContext *ctx, const int domain = AF_INET) : Server(epoll,ctx,domain) {}
  protected:
    Session* createSession(const int socket, const sockaddr_in peer_address) override;
};

/** @brief   A Session that echos back whatever data it recieves
 *  @details The EchoSession class is used for testing purposes. 
 */
class EchoSession : public tcp::Session {
  public:
    EchoSession(EPoll &epoll, Server& server, const int socket, const struct sockaddr_in peer_addr) : Session(epoll,server,socket,peer_addr) {}
    void dataAvailable() override;
  protected:
    void accepted() override;
};

#endif