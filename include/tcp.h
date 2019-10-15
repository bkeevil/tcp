#ifndef TCP_H
#define TCP_H

#include <iostream>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>


namespace tcp {

using namespace std;

class Session;

class Server {
  public:
    Server(const int port, const in_addr_t addr = INADDR_ANY) : port_(port), addr_(addr) { cout << "Creating server" << endl; }
    ~Server() { stop(); clearSessions(); }
    bool start();
    void stop();
    bool poll();
    bool terminated() { return terminated_; }
    static const int LISTEN_BACKLOG = 50;
    static const int MAX_EVENTS = 10;
  protected:
    //virtual void accepted(Session& session);
  private:
    bool openSocket();
    bool bindToAddress();
    bool startListening();
    bool setNonBlocking(const int fd);
    bool initEPoll();
    bool acceptConnection();
    void handleEvent(uint32_t events, int fd);
    void addSession(Session* session);
    bool removeSession(Session* session);
    Session* removeSession(int fd);
    Session* findSession(int fd);
    void disconnectSessions();
    void clearSessions();
    bool terminated_ {true};
    int port_        {0};
    in_addr_t addr_  {INADDR_ANY};
    int listen_fd    {0};
    int epoll_fd     ;
    struct epoll_event listen_event;
    struct epoll_event events[MAX_EVENTS];
    Session* top_ {nullptr};
    friend class Session;
};

class Session {
  public:
    Session(Server* server, int fd, struct sockaddr_in peer_addr) : server_(server), fd_(fd), peer_addr_(peer_addr) {}
    ~Session() { server_->removeSession(this); }
  protected:
    virtual void accepted();
    virtual void dataAvailable();
    virtual void disconnect();
    virtual void canSend();
  private:
    Server* server_;
    Session* next_;
    int fd_;
    struct sockaddr_in peer_addr_;
    friend class Server;
};

}

#endif