#include "echoserver.h"

/* Echo Session */

void EchoSession::accepted() {
  tcp::Session::accepted();
}

/** @brief Read incoming data and send it back byte for byte */
void EchoSession::dataAvailable() {
  mtx.lock();
  int da = available();
  if (da > 0) {
    void *buf = malloc(da);
    int dr = read(buf,da);
    int dw = write(buf,dr);
    (void)dw;
    free(buf);
  }
  mtx.unlock();
}

Session* EchoServer::createSession(const int socket, const sockaddr_in peer_address) {
  EchoSession* session = new EchoSession(epoll(),*this,socket,peer_address);
  return dynamic_cast<Session*>(session); 
}
