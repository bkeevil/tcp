#include "tcpclient.h"
#include <sys/ioctl.h>
#include <openssl/ssl.h>

namespace tcp {

using namespace std;

Client::~Client()
{
  if (state_ == SocketState::CONNECTED) {
    disconnect();
  } 
}

bool Client::connect(const char *host, const char *service) 
{
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int errorcode;
  
  if ( socket() == -1) {
    return false; 
  }
  mtx.lock();
  memset(&result,0,sizeof(struct addrinfo));
  memset(&hints,0,sizeof(struct addrinfo));
  hints.ai_family = domain();
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags |= AI_CANONNAME;
  errorcode = getaddrinfo(host,service,&hints,&result);
  if (errorcode != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errorcode));
    mtx.unlock();
    return false;
  }

  if (ssl_) {
    delete ssl_;
    ssl_ = nullptr;
  }

  if (!certfile.empty() && !keyfile.empty()) {
    ssl_ = createSSL(ctx_);
    ssl_->setOptions(verifyPeer);
    if (verifyPeer && checkPeerSubjectName) {
      ssl_->requiresCertPostValidation = true;
      ssl_->setHostname(host);
    }
    if (!ssl_->setCertificateAndKey(certfile.c_str(),keyfile.c_str())) {
      mtx.unlock();
      return false;
    }
    if (!ssl_->setfd(socket())) {
      mtx.unlock();
      return false;
    }
  }
  
  log("Connecting to " + string(result->ai_canonname) + " on port " + string(service));

  for (rp = result; rp != nullptr; rp = rp->ai_next) {
    if (::connect(socket(),rp->ai_addr,rp->ai_addrlen) == -1) {
      if (errno == EINPROGRESS) {
        state_ = SocketState::CONNECTING;
        setEvents(EPOLLIN | EPOLLOUT | EPOLLRDHUP);
        mtx.unlock();
        return true;
      } else {
        setEvents(0);
        error("connect",strerror(errno));
        mtx.unlock();
        return false;
      }
    } else {
      connected();
      mtx.unlock();
      return true;
    }
  }
  error("Could not find host " + string(host));
  freeaddrinfo(result);
  mtx.unlock();
  return false;
}

void Client::connected() {
  if (ssl_) {
    mtx.lock();
    ssl_->connect();
    printSSLErrors(); 
    readToInputBuffer();
    mtx.unlock();
  }
    
  state_ = SocketState::CONNECTED;
  log("Connected");
}

void Client::handleEvents(uint32_t events) 
{
  if (state_ == SocketState::CONNECTING) {
    if (events & EPOLLERR) {
      error("handleEvents",strerror(errno));
    }
    if (events & EPOLLRDHUP) {
      state_ = SocketState::UNCONNECTED;
      return;
    }
    if (events & EPOLLOUT) {
      connected();
    }
  } else {
    DataSocket::handleEvents(events);
  }
}

} // namespace mqttS