#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <iostream>
#include <sstream>
#include <map>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "tcpsocket.h"

namespace tcp {

using namespace std;

class Server;   /**< Listens for TCP connections and establishes Sessions    */
class Session;  /**< Represents a TCP connection to a Server                 */

class Server : public SocketHandle {
  public:
    Server(const int port, const in_addr_t addr = INADDR_ANY);
    ~Server();
    bool listening() { return listening_; }   /**< Returns true if the server is listening */
    static const int LISTEN_BACKLOG = 50;     /**< Maximum listen backlog before connections are rejected */
  protected:
    void handleEvents(uint32_t events) override;
    virtual Session* createSession(const int socket, const sockaddr_in peer_address);
    std::map<int,tcp::Session*> sessions;
  private:
    bool bindToAddress();
    bool startListening();
    bool setNonBlocking(const int fd);
    bool acceptConnection();
    void closeConnection(Session* session);
    bool listening_;
    in_port_t port_        {0};
    in_addr_t addr_  {INADDR_ANY};
    friend class Session;
};

class Session : public SocketHandle, public iostream {
  public:
    enum class State { CONNECTED=0, DISCONNECTING };
    Session(Server& server, const int socket, const struct sockaddr_in peer_addr) 
      : SocketHandle(socket), iostream(socket), server_(server), port_(peer_addr.sin_port), addr_(peer_addr.sin_addr.s_addr) { }
    Server&   server() { return server_; }
    in_port_t peer_port() { return port_;   }
    in_addr_t peer_address() { return addr_;   }
    bool      connected() { return state_ == State::CONNECTED; }
    virtual void disconnect();
  protected:
    virtual ~Session();
    virtual void handleEvents(uint32_t events) override;
    virtual void dataAvailable() = 0;
    virtual void accepted();
    virtual void disconnected();
  private:
    Server& server_;
    in_port_t port_;
    in_addr_t addr_;
    State state_ {State::CONNECTED};
    friend class Server;
};

class LoopbackSession : public Session {
  public:
    LoopbackSession(Server& server, const int socket, const struct sockaddr_in peer_addr) : Session(server,socket,peer_addr) {}
    void dataAvailable() override;
};

}

#endif