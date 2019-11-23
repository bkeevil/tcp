#include "tcpclient.h"

namespace tcp {

using namespace std;

bool Client::connect(string &hostname, in_port_t port) 
{
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int sfd, errorcode;
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
  for (rp = result; rp != NULL; rp = rp->ai_next) {
      sfd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (sfd == -1)
          continue;

      if (::bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
          break;                

      ::close(sfd); 
  }

  if (sfd > 0) 
    ::close(sfd);
    
  if (rp == NULL) {               /* No address succeeded */
    cerr << "Could not find host " << hostname << endl;
    return false;
  }

  struct sockaddr_in *sa = (sockaddr_in*)rp->ai_addr;
  bool cr = connect(sa->sin_addr.s_addr,port);
  freeaddrinfo(result);           /* No longer needed */  
  return cr;
}

bool Client::connect(in_addr_t addr, in_port_t port) {
  sockaddr_in a;
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
    if (::connect(socket(),(const sockaddr*)(&a),sizeof(sockaddr_in)) == -1) {
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