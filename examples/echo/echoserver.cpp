#include "echoserver.h"

/* Echo Session */

void EchoSession::accepted() {
  tcp::Session::accepted();
}

/** @brief Read incoming data and send it back byte for byte */
void EchoSession::dataAvailable() {
  copy(inputBuffer.begin(),inputBuffer.end(),back_inserter(outputBuffer));
  int res = outputBuffer.size();
  (void)res;
  setEvents(EPOLLIN | EPOLLOUT | EPOLLRDHUP);
  inputBuffer.clear();
}

Session* EchoServer::createSession(const int socket, const sockaddr_in peer_address) {
  EchoSession* session = new EchoSession(*this,socket,peer_address);
  return dynamic_cast<Session*>(session); 
}
