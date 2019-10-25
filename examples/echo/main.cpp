#include <iostream>
#include "tcpsocket.h"
#include "tcpserver.h"

using namespace std;
using namespace tcp;

class EchoServer : public tcp::Server {
  public:
    EchoServer(const int port = 0, const in_addr_t addr = INADDR_ANY) : Server(port,addr) {}
  protected:
    Session* createSession(const int socket, const sockaddr_in peer_address) override;
};

/** @brief   A Session that echos back whatever data it recieves
 *  @details The EchoSession class is used for testing purposes. 
 */
class EchoSession : public tcp::Session {
  public:
    EchoSession(Server& server, const int socket, const struct sockaddr_in peer_addr) : Session(server,socket,peer_addr) {}
    void dataAvailable() override;
};

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

int main() { 
  EchoServer server(1200);
  if (!server.listening()) {
    cerr << "Failed to start server" << endl;
    return 1;
  }
  while (server.listening()) {
    epoll.poll(100);
  }

  return 0;
}