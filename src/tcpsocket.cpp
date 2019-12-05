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
  ev.data.fd = socket.socket_;
  if (epoll_ctl(handle_,EPOLL_CTL_ADD,socket.socket_,&ev) != -1) {
    sockets[socket.socket_] = &socket;
    return true;
  } else {
    return false;
  }
}

bool EPoll::update(Socket& socket, int events)
{
  struct epoll_event ev;
  ev.events = events;
  ev.data.fd = socket.socket_;
  return (epoll_ctl(handle_,EPOLL_CTL_MOD,socket.socket_,&ev) != -1);
}

bool EPoll::remove(Socket& socket) 
{
  if (epoll_ctl(handle_,EPOLL_CTL_DEL,socket.socket_,NULL) != -1) {
    sockets.erase(socket.socket_);
    return true;
  } else {
    return false;
  }
}

void EPoll::poll(int timeout) 
{  
  int nfds = epoll_wait(handle_,events,MAX_EVENTS,timeout); 
  if (nfds == -1) {
    if (errno == EINTR) 
      return;
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
  if (socket_ > 0) {
    epoll.remove(*this);
    if (::close(socket_) == -1) {
      cerr << "close: " << strerror(errno) << endl;
    } 
    socket_ = 0;
  }
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

void Socket::disconnected()
{
  if (state_ != SocketState::DISCONNECTED) {
    state_ = SocketState::DISCONNECTED;
    clog << "Disconnected" << endl;
    clog.flush();  
  }
}

/* DataSocket */

void DataSocket::disconnected()
{
  if (ssl_) {
    delete ssl_;
    ssl_ = nullptr;
    printSSLErrors();
  }
  Socket::disconnected();
}

void DataSocket::readToInputBuffer()
{
  uint8_t buffer[256];
  int size;
  do {
    size = read_(&buffer[0],256);
    for (int i=0;i<size;i++) {
      inputBuffer.push_back(buffer[i]);
    }
  } while (size > 0);
}

void DataSocket::sendOutputBuffer()
{
  size_t size = outputBuffer.size();
  if (size == 0) return;
  uint8_t *buffer = (uint8_t*)malloc(size);
  for (size_t i=0;i<size;i++) {
    buffer[i] = outputBuffer.at(0);
    outputBuffer.pop_front();
  }
  size_t res = write_(buffer,size);
  if (res == size) {
    setEvents(EPOLLIN | EPOLLRDHUP);
  } else {
    for (size_t i=res;i<size;i++) {
      outputBuffer.push_front(buffer[i]);
    }
    setEvents(EPOLLIN | EPOLLOUT | EPOLLRDHUP);
  }
  free(buffer);
}

void DataSocket::updateEvents()
{
  if (outputBuffer.size() > 0U) 
    setEvents(EPOLLIN | EPOLLOUT | EPOLLRDHUP);
  else
    setEvents(EPOLLIN | EPOLLRDHUP);  
}

void DataSocket::handleEvents(uint32_t events)
{
  if (state_ == SocketState::CONNECTED) {
    if (events & EPOLLRDHUP) {
      disconnected();
      return;
    } 
    if (events & EPOLLIN) {
      readToInputBuffer();
      dataAvailable();
      updateEvents();
    }
    if (events & EPOLLOUT) {
      sendOutputBuffer();
      updateEvents();
    }  
  }
}

size_t DataSocket::read_(void *buffer, size_t size)
{
  if (state_ == SocketState::CONNECTED) {
    if (ssl_) {
      return ssl_->read(buffer,size);
    } else {
      return ::recv(socket(),buffer,size,0);
    }
  } else {
    return 0;
  }
}

size_t DataSocket::write_(const void *buffer, size_t size)
{
  if (state_ == SocketState::CONNECTED) {
    if (ssl_) {
      return ssl_->write(buffer,size);
    } else {
      return ::send(socket(),buffer,size,0);
    }
  } else {
    return 0;
  }
}

size_t DataSocket::read(void *buffer, size_t size)
{
  size_t s = max<size_t>(size,inputBuffer.size());
  if (s == 0)
    return 0;
  uint8_t *buf = (uint8_t*)buffer;
  for (size_t i=0;i<s;i++) {
    buf[i] = inputBuffer.at(0);
    inputBuffer.pop_front();
  }
  return s;
}

size_t DataSocket::write(const void *buffer, size_t size)
{
  uint8_t *buf = (uint8_t*)buffer;
  for (size_t i=0;i<size;i++) {
    outputBuffer.push_back(buf[i]);
  }
  updateEvents();
  return size;
}

} // namespace tcp