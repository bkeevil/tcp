#include "tcpclient.h"

namespace tcp {

using namespace std;

bool Client::connect(in_addr_t addr, in_port_t port) {
  sockaddr_in a;
  addr_ = addr;
  port_ = port;
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = addr;
  a.sin_port = port;
  socket_ = ::socket(AF_INET,SOCK_STREAM,0);
  if ( socket_ == -1) {
    perror("socket");
    return false;
  } else {
    if (::connect(socket_,(const sockaddr*)(&a),sizeof(sockaddr_in)) == -1) {
      perror("connect");
      return false;
    }
    connected_ = true;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&(a.sin_addr.s_addr),ip,INET_ADDRSTRLEN);
    cout << "Connecting to to " << ip << " on port " << port_ << endl;
    return true;
  }

}

void Client::disconnect() {
  connected_ = false;
  ::close(socket_);
}

int Client::send(const void* buf, const size_t buf_size, const bool more) {
  if (connected_) {
    ssize_t sz;
    if (more)
      sz = ::send(socket_,buf,buf_size,MSG_MORE);
    else
      sz = ::send(socket_,buf,buf_size,0);
    
    if (sz == -1) {
      perror("Session::send");
    }
    return sz;
  } else {
    return 0;
  }

}

int Client::receive(const void* buf, const size_t buf_size) {
  return 0;
}

void Client::connected() {
  connected_ = true;
  cout << "Connected" << endl;
}

void Client::disconnected() {
  connected_ = false;
  cout << "Disconnected" << endl;
}

int Client::availableForRead() {
  int bytes_to_read;
  if ((ioctl(socket_,FIONREAD,&bytes_to_read) == 0) && (bytes_to_read > 0)) {
    return bytes_to_read;
  } else {
    return 0;
  }
}

void Client::dataAvailable() {
  int bytes_to_read = availableForRead();;
  if (bytes_to_read > 0) {
    void* buf = malloc(bytes_to_read);
    int sz = recv(socket_,buf,bytes_to_read,0);
    if (sz > 0) receive(buf,bytes_to_read);    
    free(buf);
  } else {
    perror("Session::dataAvailable");
  }
}

}