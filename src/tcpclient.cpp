#include "tcpclient.h"

namespace tcp {

using namespace std;

Client::~Client()
{
  if ((state_ == State::CONNECTED) || (state_ == State::CONNECTING)) {
    disconnect();
  }
  freeSSL();
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
    exit(EXIT_FAILURE);
  }
  
  if (useSSL) {
    initSSL(
      certfile.empty() ? nullptr : certfile.c_str(),
      keyfile.empty()  ? nullptr : keyfile.c_str(),
      cafile.empty()   ? nullptr : cafile.c_str(),
      capath.empty()   ? nullptr : capath.c_str()
    );
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
    }
  } else if (state_ == State::CONNECTING) {
    //setEvents(EPOLLIN | EPOLLRDHUP);
    if (events & EPOLLERR) {
      cerr << "connect: " << strerror(errno) << endl;      
    }
    if (events & EPOLLRDHUP) {
      state_ = State::UNCONNECTED;
      disconnected();
      return;
    }
    if (events & EPOLLOUT) {
      //state_ = State::CONNECTED;
      connected();
    }
  }
}

void Client::connected() {
  if (useSSL) {
    rbio = BIO_new(BIO_s_mem());
    wbio = BIO_new(BIO_s_mem());
    ssl_ = SSL_new(ctx());
    SSL_set_connect_state(ssl_);
    SSL_set_bio(ssl_,rbio,wbio);
    printSSLErrors();
  }
  state_ = State::CONNECTED;
  clog << "Connected" << endl;
  clog.flush();  
}

void Client::disconnected() {
  if ((state_ == State::CONNECTING) || (state_ == State::CONNECTED)) {
    if (ssl_ != nullptr) {
      SSL_shutdown(ssl_);
      printSSLErrors();
    }    
    state_ = State::DISCONNECTED;
    close(getSocket());
    clog << "Disconnected" << endl;
    clog.flush();
    if (ssl_ != nullptr) {
      SSL_free(ssl_);
      ssl_ = nullptr;
    }
    if (rbio != nullptr) {
      BIO_free_all(rbio);
      rbio = nullptr;
    }
    if (wbio != nullptr) {
      BIO_free_all(wbio);
      wbio = nullptr;
    }
    if (useSSL)
      printSSLErrors();    
  }
}

} // namespace mqtt