#include "tcpsocket.h"

#include <cstring>
//#include <strings.h>
//#include <errno.h>
//#include <string.h>
//#include <unistd.h>
//#include <fcntl.h>
//#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
//#include <arpa/inet.h>
//#include <netinet/ip.h>
//#include <netinet/tcp.h>

namespace tcp {

/* tcpstreambuf */

streambuf::streambuf(int socket, size_t buff_sz, size_t put_back) :
    socket_(socket),
    put_back_(std::max(put_back, size_t(1))),
    buffer_(std::max(buff_sz, put_back_) + put_back_)
{
    char *end = &buffer_.front() + buffer_.size();
    setg(end, end, end);
}

streambuf::int_type streambuf::underflow()
{
    if (gptr() < egptr()) // buffer not exhausted
        return traits_type::to_int_type(*gptr());

    char *base = &buffer_.front();
    char *start = base;

    if (eback() == base) // true when this isn't the first fill
    {
        // Make arrangements for putback characters
        ::memmove(base, egptr() - put_back_, put_back_);
        start += put_back_;
    }

    // start is now the start of the buffer, proper.
    // Read from socket into the provided buffer
    size_t n = ::recv(socket_,start,buffer_.size() - (start - base),0);
    if (n == 0)
        return traits_type::eof();

    // Set buffer pointers
    setg(base, start, start + n);

    return traits_type::to_int_type(*gptr());
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