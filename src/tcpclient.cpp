#include "tcpclient.h"
#include <sys/ioctl.h>
#include <openssl/ssl.h>

namespace tcp {

using namespace std;

SSLContext Client::ctx_(SSLMode::CLIENT);

Client::~Client()
{
  if (state_ == SocketState::CONNECTED) {
    disconnect();
  } 
}

bool Client::connect(const string &hostname, const in_port_t port, bool useSSL) 
{
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int errorcode;
  
  if ( socket() == -1) {
    return false; 
  }
  memset(&result,0,sizeof(struct addrinfo));
  memset(&hints,0,sizeof(struct addrinfo));
  hints.ai_family = domain();
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags |= AI_CANONNAME;
  string service = to_string(port);
  errorcode = getaddrinfo(hostname.c_str(),service.c_str(),&hints,&result);
  if (errorcode != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errorcode));
    return false;
  }
  
  if (ssl_) {
    delete ssl_;
    ssl_ = nullptr;
  }

  if (useSSL) {
    ssl_ = createSSL(ctx_);
    ssl_->setOptions(verifyPeer);
    if (verifyPeer && checkPeerSubjectName) {
      ssl_->requiresCertPostValidation = true;
      ssl_->setHostname(hostname);
    }
    if (!ssl_->setCertificateAndKey(certfile.c_str(),keyfile.c_str()))
      return false;
    if (!ssl_->setfd(socket()))
      return false;
  }

  clog << "Connecting to " << result->ai_canonname << " on port " << service << endl;
  clog.flush();

  for (rp = result; rp != nullptr; rp = rp->ai_next) {
    if (::connect(socket(),rp->ai_addr,rp->ai_addrlen) == -1) {
      if (errno == EINPROGRESS) {
        state_ = SocketState::CONNECTING;
        setEvents(EPOLLIN | EPOLLOUT | EPOLLRDHUP);
        return true;
      } else {
        setEvents(0);
        cerr << "connect: " << strerror(errno) << endl;
        return false;
      }
    } else {
      connected();
      return true;
    }
  }

  cerr << "Could not find host " << hostname << endl;
  freeaddrinfo(result);           /* No longer needed */  
  return false;
}

void Client::connected() {
  if (ssl_) {
    ssl_->connect();
    printSSLErrors(); 
    readToInputBuffer();
  }
    
  state_ = SocketState::CONNECTED;
  clog << "Connected" << endl;

  clog.flush();  
}

void Client::handleEvents(uint32_t events) 
{
  if (state_ == SocketState::CONNECTING) {
    if (events & EPOLLERR) {
      cerr << "handleEvents: " << strerror(errno) << endl;      
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