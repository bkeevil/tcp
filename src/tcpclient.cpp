#include "tcpclient.h"
#include <sys/ioctl.h>
#include <openssl/ssl.h>

namespace tcp {

using namespace std;

SSLContext Client::ctx_(SSLMode::CLIENT);

Client::~Client()
{
  if ((state_ == State::CONNECTED) || (state_ == State::CONNECTING)) {
    disconnect();
  }
  if (ssl_) {
    delete ssl_;
    ssl_ = nullptr;
  }  
}

bool Client::connect(const string &hostname, const in_port_t port) 
{
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int errorcode;
  
  if ( getSocket() == -1) {
    return false; 
  }
  memset(&result,0,sizeof(struct addrinfo));
  memset(&hints,0,sizeof(struct addrinfo));
  hints.ai_family = getDomain();
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags |= AI_CANONNAME;
  string service = to_string(port);
  errorcode = getaddrinfo(hostname.c_str(),service.c_str(),&hints,&result);
  if (errorcode != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errorcode));
    return false;
  }
  
  if (useSSL && !ssl_) {
    ssl_ = new tcp::SSL(ctx_);
/*     if (!keypass.empty()) {
      ssl_->keypass = (char*)malloc(keypass.length()+1);
      strncpy(ssl_->keypass, keypass.c_str(), keypass.length()+1);
    } */
    ssl_->setOptions(verifypeer);
    if (!ssl_->setCertificateAndKey(certfile.c_str(),keyfile.c_str()))
      return false;
    if (!ssl_->setfd(getSocket()))
      return false;
  }

  clog << "Connecting to " << result->ai_canonname << " on port " << service << endl;
  clog.flush();

  for (rp = result; rp != nullptr; rp = rp->ai_next) {
    if (::connect(getSocket(),rp->ai_addr,rp->ai_addrlen) == -1) {
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

  cerr << "Could not find host " << hostname << endl;
  freeaddrinfo(result);           /* No longer needed */  
  return false;
}

void Client::readToInputBuffer() {
  int size;
  do {
    uint8_t buffer[64];
    size = read(&buffer[0],64);
    for (int i=0;i<size;i++) {
      inputBuffer.push_back(buffer[i]);
    }
  } while (size > 0);
}

void Client::sendOutputBuffer() {
  size_t size = outputBuffer.size();
  uint8_t *buffer = (uint8_t*)malloc(size);
  for (size_t i=0;i<size;i++) {
    buffer[i] = outputBuffer.at(0);
    outputBuffer.pop_front();
  }
  write(buffer,size,false);
  free(buffer);
}

void Client::handleEvents(uint32_t events) {
  if (state_ == State::CONNECTED) {
    if (events & EPOLLRDHUP) {
      disconnected();
      return;
    } 
    if (events & EPOLLIN) {
      readToInputBuffer();
      dataAvailable();
      if (outputBuffer.size() > 0U) 
        setEvents(EPOLLIN | EPOLLOUT | EPOLLRDHUP);
      else
        setEvents(EPOLLIN | EPOLLRDHUP);
    }
    if (events & EPOLLOUT) {
      sendOutputBuffer();
      setEvents(EPOLLIN | EPOLLRDHUP);
    }
  } else if (state_ == State::CONNECTING) {
    if (events & EPOLLERR) {
      cerr << "handleEvents: " << strerror(errno) << endl;      
    }
    if (events & EPOLLRDHUP) {
      state_ = State::UNCONNECTED;
      return;
    }
    if (events & EPOLLOUT) {
      connected();
    }
  }
}

void Client::connected() {
  if (useSSL && ssl_) {
    ssl_->connect();
    printSSLErrors();
  }
  state_ = State::CONNECTED;
  clog << "Connected" << endl;
  clog.flush();  
}

int Client::available()
{
  int result;
  if (useSSL && ssl_) {
    result = ssl_->pending();
  } else {
    ioctl(getSocket(), FIONREAD, &result);
  }
  return result; 
}

int Client::read(void *buffer, const int size)
{
  if (useSSL) {
    return ssl_->read(buffer,size);
  } else {
    return ::recv(getSocket(),buffer,size,0);
  }
}

int Client::peek(void *buffer, const int size)
{
  if (useSSL) {
    return ssl_->peek(buffer,size);
  } else {
    return ::recv(getSocket(),buffer,size,MSG_PEEK);
  }
}

int Client::write(const void *buffer, const int size, const bool more)
{
  if (useSSL) {
    return ssl_->write(buffer,size);
  } else {
    if (more)
      return ::send(getSocket(),buffer,size,MSG_MORE);
    else  
      return ::send(getSocket(),buffer,size,0);
  }
}

void Client::disconnect() {
  if (useSSL && ssl_ && (state_ == State::CONNECTED)) {
    ssl_->shutdown();
    printSSLErrors();
  }
  disconnected();
}

void Client::disconnected() {
  if ((state_ == State::CONNECTING) || (state_ == State::CONNECTED)) {
    state_ = State::DISCONNECTED;
    if (ssl_) {
      delete ssl_;
      ssl_ = nullptr;
      printSSLErrors();
    }
    if (socket_ > 0) {
      ::close(socket_);
      socket_ = 0;
    }
    clog << "Disconnected" << endl;
    clog.flush();
  }
}

} // namespace mqtt