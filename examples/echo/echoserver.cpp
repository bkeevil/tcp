#include "echoserver.h"

/* Echo Session */

/** @brief Read incoming data and send it back byte for byte */
void EchoSession::dataAvailable() {
  char c;
  c = streambuf_.sbumpc();
  while (c != traits_type::eof()) {
    streambuf_.sputc(c);
    c = streambuf_.sbumpc();
  } 
}

Session* EchoServer::createSession(const int socket, const sockaddr_in peer_address) {
  EchoSession* session = new EchoSession(*this,socket,peer_address);
  return dynamic_cast<Session*>(session); 
}
