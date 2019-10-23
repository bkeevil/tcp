#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include <iostream>
#include <streambuf>
#include <vector>
#include <map>
#include <sys/epoll.h>

namespace tcp {

using namespace std;

class SocketHandle;

/** @brief Provides access to unix socket handle using iostream streambuf interface */
class streambuf : public std::streambuf {
  public:
    explicit streambuf(int socket, size_t rx_buff_sz = 256, size_t tx_buff_sz = 256, size_t put_back = 8);
    // moving and copying not allowed :
    streambuf(const streambuf &) = delete;
    streambuf &operator= (const streambuf &) = delete;
    streambuf(streambuf &&) = delete;
    streambuf &operator= (streambuf &&) = delete;
    //
    int available() { return showmanyc() + (egptr() - gptr()); }
  protected:
    int_type underflow() override;
    streamsize showmanyc() override;
    streamsize xsgetn (char* s, streamsize n) override;
    int_type overflow(int_type ch) override;
    int sync() override;
    streamsize xsputn (const char* s, streamsize n) override;
  private:
    int internalflush(bool more);
    int socket_;
    const size_t put_back_;
    vector<char> recvbuffer_;
    vector<char> sendbuffer_;
};

class iostream : public std::iostream {
  public:
    iostream(int socket) : std::iostream(&streambuf_), streambuf_(socket) { }
  protected:
    tcp::streambuf streambuf_;  
};

/** @brief Encapsulates the EPoll interface */
class EPoll {
  public:
    EPoll();
    ~EPoll();
    void poll(int timeout); 
  private:
    static const int MAX_EVENTS = 10; /**< Maximum number of epoll events to handle per poll() call                     */
    bool add(SocketHandle& socket, int events);
    bool update(SocketHandle& socket, int events);
    bool remove(SocketHandle& socket);    
    void handleEvents(uint32_t events, int fd);
    int handle_;
    epoll_event events[MAX_EVENTS];
    std::map<int,tcp::SocketHandle*> sockets;
    friend class SocketHandle;
};

class SocketHandle {
  public:
    SocketHandle(const int socket = 0, const bool blocking = false, const int events = (EPOLLIN | EPOLLRDHUP));
    ~SocketHandle();
    int socket() const { return socket_; }
  protected:
    bool setEvents(int events);
    virtual void handleEvents(uint32_t events) = 0;
  private:
    int socket_;
    int events_;
    friend class EPoll;
};

extern EPoll epoll;

} // namespace tcp

#endif // include guard