#include "tcpsocket.h"
#include <algorithm>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

namespace tcp {

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
  bool result = false;
  ev.events = events;
  ev.data.fd = socket.socket_;
  if (epoll_ctl(handle_,EPOLL_CTL_ADD,socket.socket_,&ev) != -1) {
    sockets[socket.socket_] = &socket;
    result = true;
  } 
  return result;
}

bool EPoll::update(Socket& socket, int events)
{
  bool result;
  struct epoll_event ev;
  ev.events = events;
  ev.data.fd = socket.socket_;
  result = (epoll_ctl(handle_,EPOLL_CTL_MOD,socket.socket_,&ev) != -1);
  return result;
}

bool EPoll::remove(Socket& socket) 
{
  bool result = false;
  if (epoll_ctl(handle_,EPOLL_CTL_DEL,socket.socket_,NULL) != -1) {
    sockets.erase(socket.socket_);
    result = true;
  } 
  return result;
}

void EPoll::poll(int timeout) 
{  
  int nfds = epoll_wait(handle_,events,MAX_EVENTS,timeout); 
  if (nfds == -1) {
    if (errno != EINTR) 
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

Socket::Socket(EPoll &epoll, const int domain, const int socket, const bool blocking, const int events) : epoll_(epoll), events_(events), domain_(domain), socket_(socket) 
{ 
  if ((domain != AF_INET) && (domain != AF_INET6)) {
    cerr << "Socket: Only IPv4 and IPv6 are supported." << endl;
    return;
  }
  if (socket < 0) {
    cerr << "Socket: Socket parameter is < 0" << endl;;
    return;
  }
  mtx.lock();
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
  
  if (!epoll_.add(*this,events)) {
    cerr << "Unable to add socket to epoll" << endl;
  }
  
  mtx.unlock();
}

Socket::~Socket() 
{
  if (socket_ > 0) {
    mtx.lock();
    epoll_.remove(*this);
    if (::close(socket_) == -1) {
      cerr << "close: " << strerror(errno) << endl;
    } 
    socket_ = 0;
    mtx.unlock();
  }
}

bool Socket::setEvents(int events) 
{ 
  mtx.lock();
  bool result = false;
  if (events != events_) { 
    if (epoll_.update(*this,events)) { 
      events_ = events; 
      result = true;
    } 
  } else {
    result = true;
  }
  mtx.unlock();
  return result;
}

void Socket::disconnect() {
  mtx.lock();
  if (state_ == SocketState::CONNECTED) {  
    ::shutdown(socket_,SHUT_RDWR);
  }
  mtx.unlock();
  disconnected();
}

void Socket::disconnected()
{
  mtx.lock();
  if (state_ != SocketState::DISCONNECTED) {
    ::close(socket_);
    socket_ = 0;
    state_ = SocketState::DISCONNECTED;
    clog << "Disconnected" << endl;
    clog.flush(); 
  }
  mtx.unlock();
}

/* DataSocket */

void DataSocket::disconnect()
{ 
  mtx.lock();
  if (ssl_ && (state_ == SocketState::CONNECTED)) {
    ssl_->shutdown();
    delete ssl_;
    ssl_ = nullptr;
    printSSLErrors();
  }
  mtx.unlock();
  Socket::disconnect();
}

void DataSocket::disconnected()
{
  mtx.lock();  // Do I need to use lock here?
  if (ssl_) {
    delete ssl_;
    ssl_ = nullptr;
    printSSLErrors();
  }
  mtx.unlock();
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
  mtx.lock();
  size_t size = outputBuffer.size();
  if (size == 0) return;
  uint8_t *buffer = (uint8_t*)malloc(size);
  for (size_t i=0;i<size;i++) {
    buffer[i] = outputBuffer.at(0);
    outputBuffer.pop_front();
  }
  size_t res = write_(buffer,size);
  canSend(res!=size);
  if (res != size) {
    for (size_t i=res;i<size;i++) {
      outputBuffer.push_front(buffer[i]);
    }
  }
  free(buffer);
  mtx.unlock();
}

void DataSocket::canSend(bool value) 
{
  int events = EPOLLIN | EPOLLRDHUP;
  if (value)
    events |= EPOLLOUT;
  setEvents(events);
}

void DataSocket::handleEvents(uint32_t events)
{
  if (state_ == SocketState::CONNECTED) {
    if (events & EPOLLRDHUP) {
      disconnected();
    } else {
      if (events & EPOLLIN) {
        mtx.lock();
        readToInputBuffer();
        dataAvailable();
        if (outputBuffer.size() > 0U) {
          sendOutputBuffer();
          canSend(outputBuffer.size() > 0U);
        } else {
          canSend(false);  
        }
        mtx.unlock();
      }
      if (events & EPOLLOUT) {
        mtx.lock();
        sendOutputBuffer();
        canSend(outputBuffer.size() > 0U);
        mtx.unlock();
      }
    }
  }
}

size_t DataSocket::read_(void *buffer, size_t size)
{
  if (state_ == SocketState::CONNECTED) {
    size_t result;
    if (ssl_) {
      result = ssl_->read(buffer,size);
    } else {
      result = ::recv(socket(),buffer,size,0);
    }
    return result;
  } else {
    return 0;
  }
}

size_t DataSocket::write_(const void *buffer, size_t size)
{
  if (state_ == SocketState::CONNECTED) {
    size_t result;
    if (ssl_) {
      result = ssl_->write(buffer,size);
    } else {
      result = ::send(socket(),buffer,size,MSG_NOSIGNAL);
    }
    return result;
  } else {
    return 0;
  }
}

size_t DataSocket::read(void *buffer, size_t size)
{
  size_t result = 0;
  if (size) {
    mtx.lock();
    result = max<size_t>(size,inputBuffer.size());
    if (result > 0) {
      for (size_t i=0;i<result;++i) {
        ((uint8_t*)buffer)[i] = inputBuffer.at(0);
        inputBuffer.pop_front();
      }
    }
    mtx.unlock();
  }
  return result;
}

size_t DataSocket::write(const void *buffer, size_t size)
{
  size_t result = 0U;
  if (size) {
    mtx.lock();
    try {
      for (size_t i=0;i<size;++i) {
        outputBuffer.push_back(((uint8_t*)buffer)[i]);
        ++result;
      }
      canSend(true);
    } catch (const std::bad_alloc&) {
      mtx.unlock();
    }
  }
  return result;
}

SSL* DataSocket::createSSL(SSLContext &context)
{
  return new SSL(*this,context);
}

int getDomainFromHostAndPort(const char* host, const char* port, int def_domain)
{
  struct addrinfo hints;
  struct addrinfo *result;
  int errorcode;
  int domain;

  memset(&result,0,sizeof(struct addrinfo));
  memset(&hints,0,sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_NUMERICHOST;
  errorcode = getaddrinfo(host,nullptr,&hints,&result);
  if (errorcode == 0) {
    domain = result->ai_family;
  } else {
    domain = AF_UNSPEC;
  }
  if (domain == AF_UNSPEC) {
    hints.ai_flags = AI_CANONNAME;
    errorcode = getaddrinfo(host,port,&hints,&result);
    if (errorcode == 0) {
      domain = result->ai_family;
    }
  }
  if (domain == AF_UNSPEC) {
    return def_domain;
  } else {
    return domain;
  }
}

} // namespace tcp