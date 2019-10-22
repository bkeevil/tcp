#include <cstring>
#include <cassert>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include "tcpsocket.h"

namespace tcp {

/* tcpstreambuf */

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

    char *base = &recvbuffer_.front();
    char *start = base;

    if (eback() == base) { // true when this isn't the first fill
        // Make arrangements for putback characters
        ::memmove(base, egptr() - put_back_, put_back_);
        start += put_back_;
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
}

/** @brief Get number of characters available
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

/** @brief Writes the data in the send buffer to the socket 
 *  Set more to true if there is more data to be written
 *  Setting more to false causes the underlying network to send the buffered contents */
int streambuf::internalflush(bool more) {
  char *base = &sendbuffer_.front();
  ptrdiff_t size = pptr() - base;
  if (size == 0) 
    return 0;
  ssize_t actualsize;
  if (more) {
    actualsize = ::send(socket_,(void*)pptr(),size,MSG_MORE);
  } else {
    actualsize = ::send(socket_,(void*)pptr(),size,0);
  }
  if (actualsize > 0) {
    if (actualsize < size) {
      ::memmove(base,base+actualsize,size-actualsize);
      setp(base+actualsize,base+sendbuffer_.size());
    }
  }
  return actualsize;
}

streambuf::int_type streambuf::overflow(int_type ch) {
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

int streambuf::sync() {
  return internalflush(false);
}

streamsize streambuf::xsputn(const char* s, streamsize n) {
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

/* Socket */

/*void Socket::sync() {
  int i;
  int size;
  int actualSize;

  if ((::ioctl(socket_,FIONREAD,&size) == 0) && (size > 0)) {
    char* buf = (char*)malloc(size);
    actualSize = ::recv(socket_,buf,size,0);
    for (i = actualSize - 1; i>=0; i--) {
      recvBuffer.push_front(buf[i]);
    }
    free(buf);
    dataAvailable();
  }

  size = sendBuffer.size();
  if (size > 0) {
    char* buf = (char*)malloc(size);
    for (i=0;i<size;i++) {
      buf[i] = sendBuffer.front();
      sendBuffer.pop_front();
    }
    actualSize = send(socket_,buf,size,0);
    if (actualSize < size) {
      for (i=size - 1; i>actualSize; i--) {
        sendBuffer.push_front(buf[i]);
      }
    }
    free(buf);
  }
}*/

} // namespace tcp