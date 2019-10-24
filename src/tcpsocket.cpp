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

/* tcp::streambuf */

streambuf::streambuf(int socket, size_t rx_buff_sz, size_t tx_buff_sz, size_t put_back) :
    socket_(socket),
    put_back_(std::max(put_back, size_t(1))),
    recvbuffer_(std::max(rx_buff_sz, put_back_) + put_back_),
    sendbuffer_(tx_buff_sz+1)
{
    char *end = &recvbuffer_.front() + recvbuffer_.size();
    setg(end, end, end);

    char *base = &sendbuffer_.front();
    setp(base, base + sendbuffer_.size() - 1); // -1 to make overflow() easier
}

streambuf::int_type streambuf::underflow() {
    if (gptr() < egptr()) // buffer not exhausted
        return traits_type::to_int_type(*gptr());

    if (in_avail() > 0) {

      char *base = &recvbuffer_.front();
      char *start = base;

      if (eback() == base) { // true when this isn't the first fill
          // Make arrangements for putback characters
          ptrdiff_t pbsize = std::min<ptrdiff_t>(put_back_,egptr() - eback());
          void* pboffset = egptr() - pbsize;
          if (pboffset > base) {
            ::memmove(base, pboffset, pbsize);
          }
          start += pbsize;
      }

      // start is now the start of the buffer, proper.
      // Read from socket into the provided buffer
      ssize_t n = ::recv(socket_,start,recvbuffer_.size() - (start - base),0);
      if (n == 0) {
          return traits_type::eof();
      }

      // Set buffer pointers
      setg(base, start, start + n);

      return traits_type::to_int_type(*gptr());
    } else {
      return traits_type::eof();
    }
}

/** @brief Get number of characters available in the socket buffers
 *  Virtual function (to be read s-how-many-c) called by other member functions to get an estimate on 
 *  the number of characters available in the associated input sequence. */
streamsize streambuf::showmanyc() {
  int size;
  if ((::ioctl(socket_,FIONREAD,&size) == 0) && (size > 0)) {
    return size;
  } else {
    return 0;
  }
}

/** @brief Get sequence of characters
 *  Retrieves characters from the controlled input sequence and stores them in the array pointed by s, 
 *  until either n characters have been extracted or the end of the sequence is reached. */
streamsize streambuf::xsgetn(char* s, streamsize n) {
  char* sptr = s;
  streamsize remainingsize = n;
  ptrdiff_t bufferedsize;

  while ((remainingsize > 0) && (in_avail() > 0)) {
    bufferedsize = min(egptr() - gptr(),remainingsize);
    if (bufferedsize > 0) {
      memmove(sptr,gptr(),bufferedsize);
      sptr += bufferedsize;
      gbump(bufferedsize);
      remainingsize -= bufferedsize;
    }
    underflow();
  }
  return n - remainingsize;
}

int streambuf::internalflush(bool more) 
{
  char *base = &sendbuffer_.front();
  ptrdiff_t size = pptr() - base;
  if (size == 0) 
    return 0;
  ssize_t actualsize;
  if (more) {
    actualsize = ::send(socket_,(void*)base,size,MSG_MORE);
  } else {
    actualsize = ::send(socket_,(void*)base,size,0);
  }
  if (actualsize > 0) {
    if (actualsize < size) {
      ::memmove(base,base+actualsize,size-actualsize);
      setp(base+actualsize,base+sendbuffer_.size());
    } else {
      setp(base,base+sendbuffer_.size());
    }
  }
  return actualsize;
}

streambuf::int_type streambuf::overflow(int_type ch) 
{
  if (ch == traits_type::eof()) {
    internalflush(false);
  } else {
    if (pptr() == epptr()) {
      internalflush(true);
    }
    if (pptr() <= epptr()) {
      *pptr() = ch;
      pbump(1);
      return ch;
    }
  }
  return traits_type::eof();
}

int streambuf::sync() 
{
  return internalflush(false);
}

streamsize streambuf::xsputn(const char* s, streamsize n) 
{
  const char* sptr = s;
  streamsize remainingsize = n;
  ptrdiff_t bufferedsize;

  while (remainingsize > 0) {
    bufferedsize = min(epptr() - pptr(),remainingsize);
    if (bufferedsize > 0) {
      memmove(pptr(),s,bufferedsize);
      sptr += bufferedsize;
      pbump(bufferedsize);
      remainingsize -= bufferedsize;
      if (epptr() == pptr()) {
        internalflush(true);
      }
    }
  }
  return n - remainingsize;
}

/* EPoll */

EPoll::EPoll() 
{
  handle_ = epoll_create1(0);
  if (handle_ == -1) {
    cerr << "epoll_create1: " << strerror(errno) << endl;
    cerr.flush();
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
  ev.data.fd = socket.socket();
  if (epoll_ctl(handle_,EPOLL_CTL_ADD,socket.socket(),&ev) != -1) {
    sockets[socket.socket()] = &socket;
    return true;
  } else {
    return false;
  }
}

bool EPoll::update(Socket& socket, int events)
{
  struct epoll_event ev;
  ev.events = events;
  ev.data.fd = socket.socket();
  return (epoll_ctl(handle_,EPOLL_CTL_MOD,socket.socket(),&ev) != -1);
}

bool EPoll::remove(Socket& socket) 
{
  if (epoll_ctl(handle_,EPOLL_CTL_DEL,socket.socket(),NULL) != -1) {
    sockets.erase(socket.socket());
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

Socket::Socket(const int socket, const bool blocking, const int events) : socket_(socket), events_(events) 
{ 
  if (socket < 0) {
    cerr << "Socket: socket parameter should not be < 0";
  }
  if (socket <= 0) {
    socket_ = ::socket(AF_INET,SOCK_STREAM,0);
    if (socket_ == -1) {
      cerr << "socket: " << strerror(errno) << endl;
      cerr.flush();
    }
  }
  if (!blocking) {
    int flags = fcntl(socket_,F_GETFL,0);
    if (flags == -1) {
      cerr << "fcntl (get): " << strerror(errno) << endl;
      cerr.flush();
    } else {
      flags = (flags & ~O_NONBLOCK);
      if (fcntl(socket_,F_SETFL,flags) == -1) {
        cerr << "fcntl (set): " << strerror(errno) << endl;
        cerr.flush();
      }
    }
  }
  epoll.add(*this,events); 
}

Socket::~Socket() 
{
  epoll.remove(*this);
  if (socket_ > 0) {
    if (::close(socket_) == -1) {
      cerr << "close: " << strerror(errno) << endl;
      cerr.flush();
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