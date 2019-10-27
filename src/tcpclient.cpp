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
  if ( socket() == -1) {
    return false;
  } else {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&(a.sin_addr.s_addr),ip,INET_ADDRSTRLEN);
    clog << "Connecting to to " << ip << " on port " << port_ << endl;
    clog.flush();
    if (::connect(socket(),(const sockaddr*)(&a),sizeof(sockaddr_in)) == -1) {
      if (errno == EINPROGRESS) {
        state_ = State::CONNECTING;
        setEvents(EPOLLOUT | EPOLLRDHUP);
        return true;
      } else {
        cerr << "connect: " << strerror(errno) << endl;
        cerr.flush();
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
      dataAvailable();
      flush();
    }
  } else if (state_ == State::CONNECTING) {
    setEvents(EPOLLIN | EPOLLRDHUP);
    if (events & EPOLLRDHUP) {
      state_ = State::UNCONNECTED;
      disconnected();
      return;
    } 
    if (events & EPOLLOUT) {
      state_ = State::CONNECTED;
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