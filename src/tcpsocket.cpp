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
  mtx.lock();
  handle_ = epoll_create1(0);
  if (handle_ == -1) {
    cerr << "epoll_create1: " << strerror(errno) << endl;
  }
  mtx.unlock();
}

EPoll::~EPoll() 
{
  mtx.lock();
  sockets.clear();
  if (handle_ > 0) {
    ::close(handle_);
  }
  mtx.unlock();
}

bool EPoll::add(Socket& socket, int events) 
{
  struct epoll_event ev;
  mtx.lock();
  bool result = false;
  ev.events = events;
  ev.data.fd = socket.socket_;
  if (epoll_ctl(handle_,EPOLL_CTL_ADD,socket.socket_,&ev) != -1) {
    sockets[socket.socket_] = &socket;
    result = true;
  } 
  mtx.unlock();
  return result;
}

bool EPoll::update(Socket& socket, int events)
{
  bool result;
  struct epoll_event ev;
  mtx.lock();
  ev.events = events;
  ev.data.fd = socket.socket_;
  result = (epoll_ctl(handle_,EPOLL_CTL_MOD,socket.socket_,&ev) != -1);
  mtx.unlock();
  return result;
}

bool EPoll::remove(Socket& socket) 
{
  mtx.lock();
  bool result = false;
  if (epoll_ctl(handle_,EPOLL_CTL_DEL,socket.socket_,NULL) != -1) {
    mtx.lock();
    sockets.erase(socket.socket_);
    mtx.unlock();
    result = true;
  } 
  mtx.unlock();
  return result;
}

void EPoll::poll(int timeout) 
{  
  mtx.lock();
  int nfds = epoll_wait(handle_,events,MAX_EVENTS,timeout); 
  if (nfds == -1) {
    if (errno != EINTR) 
      cerr << "epoll_wait: " << strerror(errno) << endl;
  } else {
    vector<thread*> pool;
    for (int n = 0; n < nfds; ++n) {
      handleEvents(pool,events[n].events,events[n].data.fd);
    }
    for (auto it : pool) {
      it->join();
    }
  }
  mtx.unlock();
}

void EPoll::handleEvents(vector<thread*> pool, uint32_t events, int fd) 
{
  Socket* socket = sockets[fd];
  if (socket != nullptr) {
    thread *th = new thread(&Socket::threadHandleEvents,socket,events);
    pool.push_back(th);
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
  
  if (!epoll.add(*this,events)) {
    cerr << "Unable to add socket to epoll" << endl;
  }
  
  mtx.unlock();
}

Socket::~Socket() 
{
  if (socket_ > 0) {
    mtx.lock();
    epoll.remove(*this);
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
    if (epoll.update(*this,events)) { 
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
  if (state_ != SocketState::DISCONNECTED) {  
    ::shutdown(socket_,SHUT_RDWR);
    state_ = SocketState::DISCONNECTED;
    clog << "Disconnected" << endl;
    clog.flush(); 
  }
  mtx.unlock();
}

void Socket::disconnected()
{
  mtx.lock();
  if (state_ != SocketState::DISCONNECTED) {
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
  Socket::disconnect();
  mtx.unlock();
}

void DataSocket::disconnected()
{
  mtx.lock();
  if (ssl_) {
    delete ssl_;
    ssl_ = nullptr;
    printSSLErrors();
  }
  Socket::disconnected();
  mtx.unlock();
}

void DataSocket::readToInputBuffer()
{
  mtx.lock();
  uint8_t buffer[256];
  int size;
  do {
    size = read_(&buffer[0],256);
    for (int i=0;i<size;i++) {
      inputBuffer.push_back(buffer[i]);
    }
  } while (size > 0);
  mtx.unlock();
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
  if (value)
    setEvents(EPOLLIN | EPOLLOUT | EPOLLRDHUP);
  else
    setEvents(EPOLLIN | EPOLLRDHUP);  
}

void DataSocket::handleEvents(uint32_t events)
{
  if (state_ == SocketState::CONNECTED) {
    mtx.lock();
    if (events & EPOLLRDHUP) {
      disconnected();
    } else {
      if (events & EPOLLIN) {
        readToInputBuffer();
        dataAvailable();
        canSend(outputBuffer.size() > 0U);
      }
      if (events & EPOLLOUT) {
        sendOutputBuffer();
        canSend(outputBuffer.size() > 0U);
      }  
    }
    mtx.unlock();
  }
}

size_t DataSocket::read_(void *buffer, size_t size)
{
  if (state_ == SocketState::CONNECTED) {
    size_t result;
    mtx.lock();
    if (ssl_) {
      result = ssl_->read(buffer,size);
    } else {
      result = ::recv(socket(),buffer,size,0);
    }
    mtx.unlock();
    return result;
  } else {
    return 0;
  }
}

size_t DataSocket::write_(const void *buffer, size_t size)
{
  if (state_ == SocketState::CONNECTED) {
    size_t result;
    mtx.lock();
    if (ssl_) {
      result = ssl_->write(buffer,size);
    } else {
      result = ::send(socket(),buffer,size,0);
    }
    mtx.unlock();
    return result;
  } else {
    return 0;
  }
}

size_t DataSocket::read(void *buffer, size_t size)
{
  size_t result;
  mtx.lock();
  result = max<size_t>(size,inputBuffer.size());
  if (result > 0) {
    uint8_t *buf = (uint8_t*)buffer;
    for (size_t i=0;i<result;i++) {
      buf[i] = inputBuffer.at(0);
      inputBuffer.pop_front();
    }
  }
  mtx.unlock();
  return result;
}

size_t DataSocket::write(const void *buffer, size_t size)
{
  mtx.lock();
  uint8_t *buf = (uint8_t*)buffer;
  for (size_t i=0;i<size;i++) {
    outputBuffer.push_back(buf[i]);
  }
  canSend(outputBuffer.size() > 0U);
  mtx.unlock();
  return size;
}

} // namespace tcp