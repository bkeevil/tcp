#include "tcpclient.h"

namespace tcp {

using namespace std;

bool Client::connect(string &hostname, in_port_t port) 
{
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int errorcode;
  memset(&result,0,sizeof(struct addrinfo));
  memset(&hints,0,sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  string service = to_string(port);
  errorcode = getaddrinfo(hostname.c_str(),service.c_str(),&hints,&result);
  if (errorcode != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errorcode));
    exit(EXIT_FAILURE);
  }
  
  struct sockaddr_in *sa;
  bool cr;
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sa = (sockaddr_in*)rp->ai_addr;
    cr = connect(sa->sin_addr.s_addr,port);
    if (cr)
      return true;
  }

  cerr << "Could not find host " << hostname << endl;
  freeaddrinfo(result);           /* No longer needed */  
  return false;
}

bool Client::connect(in_addr_t addr, in_port_t port) {
  struct sockaddr_in a;
  addr_ = addr;
  port_ = port;
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = addr;
  a.sin_port = port;
  if ( socket() == -1) {
    return false;
  } else {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&(a.sin_addr.s_addr),ip,INET_ADDRSTRLEN);
    clog << "Connecting to " << ip << " on port " << port_ << endl;
    clog.flush();
    int s = socket();
    if (::connect(s,(const sockaddr*)(&a),sizeof(sockaddr_in)) == -1) {
      if (errno == EINPROGRESS) {
        state_ = State::CONNECTING;
        setEvents(EPOLLIN | EPOLLOUT | EPOLLRDHUP);
        return true;
      } else {
        cerr << "connect: " << strerror(errno) << endl;
        return false;
      }
    } else {
      connected();
      return true;
    }
  }
}

void Client::disconnect() {
  disconnected();
}

void Client::handleEvents(uint32_t events) {
  if (state_ == State::CONNECTED) {
    if (events & EPOLLRDHUP) {
      disconnected();
      return;
    } 
    if (events & EPOLLIN) {
      clear();
      dataAvailable();
      flush();
    }
  } else if (state_ == State::CONNECTING) {
    //setEvents(EPOLLIN | EPOLLRDHUP);
    if (events & EPOLLERR) {
      cerr << "connect: " << strerror(errno) << endl;      
    }
    if (events & EPOLLRDHUP) {
      state_ = State::UNCONNECTED;
      disconnected();
      clear();
      return;
    }
    if (events & EPOLLOUT) {
      //state_ = State::CONNECTED;
      clear();
      connected();
      flush();
    }
  }
}

void Client::connected() {
  state_ = State::CONNECTED;
  clog << "Connected" << endl;
  clog.flush();
}

void Client::disconnected() {
  if ((state_ == State::CONNECTING) || (state_ == State::CONNECTED)) {
    state_ = State::DISCONNECTED;
    close(socket());
    clog << "Disconnected" << endl;
    clog.flush();
  }
}

}