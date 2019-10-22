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

class Server {
  public:
    Server(const int port, const in_addr_t addr = INADDR_ANY) : port_(port), addr_(addr) { }
    ~Server() { stop(); }
    bool start();                             /**< Start the server                                                             */
    void stop();                              /**< Stop the server from listening and free all resources                        */
    bool poll(int timeout);                   /**< Call poll() frequently from the main program loop to process network traffic */
    bool terminated() { return terminated_; } /**< Returns true if the server has been terminated                               */
    static const int LISTEN_BACKLOG = 50;     /**< Maximum listen backlog before connections are rejected                       */
    static const int MAX_EVENTS = 10;         /**< Maximum number of epoll events to handle per poll() call                     */
  protected:
    virtual Session* createSession(const int socket, const sockaddr_in peer_address);
    std::map<int,tcp::Session*> sessions;
  private:
    bool openSocket();
    bool bindToAddress();
    bool startListening();
    bool setNonBlocking(const int fd);
    bool initEPoll();
    bool acceptConnection();
    void closeConnection(Session* session);
    void handleEvent(uint32_t events, int fd);
    bool terminated_ {true};
    in_port_t port_        {0};
    in_addr_t addr_  {INADDR_ANY};
    int listen_socket;
    int epoll_fd;
    //struct epoll_event listen_event;
    struct epoll_event events[MAX_EVENTS];
    friend class Session;
};

class Session : public Socket {
  public:
    Session(Server& server, const int socket, const struct sockaddr_in peer_addr) : Socket(socket), server_(server), port_(peer_addr.sin_port), addr_(peer_addr.sin_addr.s_addr) {}
    virtual ~Session() { server_.sessions.erase(socket()); server_.closeConnection(this); }
   
    Server& server() { return server_; }
    in_port_t port() { return port_; }
    in_addr_t address() { return addr_; }
  protected:
    virtual void accepted();
    virtual void disconnected() { if (!disconnected_) { close(socket()); disconnected_ = true; } }
    virtual void disconnect();
  private:
    Server& server_;
    in_port_t port_;
    in_addr_t addr_;
    bool disconnected_ {false};
    friend class Server;
};

class LoopbackSession : public Session {
  public:
    LoopbackSession(Server& server, const int socket, const struct sockaddr_in peer_addr) : Session(server,socket,peer_addr) {}
    virtual ~LoopbackSession() = default;
    virtual void dataAvailable();
};

}

#endif