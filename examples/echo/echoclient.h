#ifndef ECHOCLIENT_H
#define ECHOCLIENT_H

#include "tcpsocket.h"
#include "tcpclient.h"

using namespace std;
using namespace tcp;

class EchoClient : public tcp::Client {
  public:
    EchoClient(EPoll &epoll, SSLContext *ctx, const int domain = AF_INET, bool blocking = false) : Client(epoll,ctx,domain,blocking) {}
  protected:
    void dataAvailable() override;
};

#endif