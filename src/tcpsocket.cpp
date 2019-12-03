#include "tcpsocket.h"
#include <algorithm>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

namespace tcp {

EPoll epoll;

EPoll::EPoll() 
{
  handle_ = epoll_create1(0);
  if (handle_ == -1) {
    cerr << "epoll_create1: " << strerror(errno) << endl;
  }
}

EPoll::~EPoll() 
{
  sockets.clear();
  if (handle_ > 0) {
    ::close(handle_);
  }
}

bool EPoll::add(Socket& socket, int events) 
{
  struct epoll_event ev;
  ev.events = events;
  ev.data.fd = socket.getSocket();
  if (epoll_ctl(handle_,EPOLL_CTL_ADD,socket.getSocket(),&ev) != -1) {
    sockets[socket.getSocket()] = &socket;
    return true;
  } else {
    return false;
  }
}

bool EPoll::update(Socket& socket, int events)
{
  struct epoll_event ev;
  ev.events = events;
  ev.data.fd = socket.getSocket();
  return (epoll_ctl(handle_,EPOLL_CTL_MOD,socket.getSocket(),&ev) != -1);
}

bool EPoll::remove(Socket& socket) 
{
  if (epoll_ctl(handle_,EPOLL_CTL_DEL,socket.getSocket(),NULL) != -1) {
    sockets.erase(socket.getSocket());
    return true;
  } else {
    return false;
  }
}

void EPoll::poll(int timeout) 
{  
  int nfds = epoll_wait(handle_,events,MAX_EVENTS,timeout); 
  if (nfds == -1) {
    cerr << "epoll_wait: " << strerror(errno) << endl;
  } else {
    for (int n = 0; n < nfds; ++n) {
      handleEvents(events[n].events,events[n].data.fd);
    }
  }
}

void EPoll::handleEvents(uint32_t events, int fd) 
{
  Socket* socket = sockets[fd];
  if (socket != nullptr) {
    socket->handleEvents(events);
  }
}

/* Socket */

Socket::Socket(const int domain, const int socket, const bool blocking, const int events) : socket_(socket), events_(events) 
{ 
  if ((domain != AF_INET) && (domain != AF_INET6)) {
    cerr << "Socket: Only IPv4 and IPv6 are supported." << endl;
    return;
  }
  domain_ = domain;
  if (socket < 0) {
    cerr << "Socket: Socket parameter is < 0" << endl;;
    return;
  }
  if (socket == 0) {
    socket_ = ::socket(domain,SOCK_STREAM,0);
    if (socket_ == -1) {
      cerr << "socket: " << strerror(errno) << endl;
    }
  }
  int flags = fcntl(socket_,F_GETFL,0);
  if (flags == -1) {
    cerr << "fcntl (get): " << strerror(errno) << endl;
  } else {
    if (!blocking) {
      flags |= O_NONBLOCK;
    } else {
      flags = flags & ~O_NONBLOCK;
    }
    if (fcntl(socket_,F_SETFL,flags) == -1) {
      cerr << "fcntl (set): " << strerror(errno) << endl;
    }
  }
  
  if (!epoll.add(*this,events)) {
    cerr << "Unable to add socket to epoll" << endl;
  }
}

Socket::~Socket() 
{
  epoll.remove(*this);
  if (socket_ > 0) {
    if (::close(socket_) == -1) {
      cerr << "close: " << strerror(errno) << endl;
    } 
  }
  socket_ = 0;
}

bool Socket::setEvents(int events) 
{ 
  if (events != events_) { 
    if (epoll.update(*this,events)) { 
      events_ = events; 
      return true;
    } else { 
      return false; 
    } 
  }
  return true;
}

} // namespace tcp