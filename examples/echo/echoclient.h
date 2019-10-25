#ifndef ECHOCLIENT_H
#define ECHOCLIENT_H

#include "tcpsocket.h"
#include "tcpclient.h"

using namespace std;
using namespace tcp;

class EchoClient : public tcp::Client {
  public:
    EchoClient(bool blocking = false) : Client(blocking) {}
  protected:
    void dataAvailable() override;
};

#endif