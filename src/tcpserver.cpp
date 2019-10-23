#include "tcpserver.h"

namespace tcp {

using namespace std;

Server::Server(const int port, const in_addr_t addr) 
  : SocketHandle(0,false,EPOLLIN), port_(port), addr_(addr) 
{
  listening_ = (bindToAddress() && startListening());
}

Server::~Server() {
  if (listening_) {
    clog << "Sending disconnect to all sessions" << endl;
    clog.flush();
    std::map<int,Session*>::iterator it;
    for (it = sessions.begin();it != sessions.end();++it) {
      it->second->disconnect();
    }
  }
  ::close(socket());
}

void Server::handleEvents(uint32_t events) {
  if (listening_ && (events & EPOLLIN)) {
    acceptConnection();
  }
}

bool Server::bindToAddress() {
  struct sockaddr_in server_addr;
  memset(&server_addr,0,sizeof(sockaddr_in));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port_);
  server_addr.sin_addr.s_addr = addr_;    
  if (bind(socket(),(struct sockaddr *)&server_addr,sizeof(server_addr)) == -1) {
    cerr << "bind: " << strerror(errno) << endl;
    cerr.flush();
    return false;
  } else {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&(server_addr.sin_addr),ip,INET_ADDRSTRLEN);
    clog << "Server bound to " << ip << " on port " << port_ << endl;
    clog.flush();
    return true;
  }
}

bool Server::startListening() {
  if (listen(socket(),listenBacklog_) == -1) {
    cerr << "listen: " << strerror(errno) << endl;
    cerr.flush();
    return false;
  } else {
    clog << "Server started listening" << endl;
    clog.flush();
    return true;
  }
}

bool Server::acceptConnection() {
  struct sockaddr_in peer_addr;
  socklen_t peer_addr_len = sizeof(struct sockaddr_in);
  int conn_sock = ::accept(socket(),(struct sockaddr *) &peer_addr, &peer_addr_len);
  if (conn_sock == -1) {
    cerr << "accept: " << strerror(errno) << endl;
    cerr.flush();
    return false;
  } else {
    // Delete any existing sessions with the same socket handle
    Session* session = sessions[conn_sock];
    sessions.erase(conn_sock);
    if (session != nullptr) {
      clog << "WARNING: A session with socket handle " << conn_sock << " already exists. Deleting it." << endl;
      clog.flush();
      delete session;
    } 
    // Start a new session and accept it
    session = createSession(conn_sock,peer_addr);
    sessions[conn_sock] = session;
    session->accepted();
    return true;
  }
}

Session* Server::createSession(const int socket, const sockaddr_in peer_address) {
  LoopbackSession* session = new LoopbackSession(*this,socket,peer_address);
  return dynamic_cast<Session*>(session); 
}

/* Session */

Session::~Session() { 
  server_.sessions.erase(socket());
}

void Session::handleEvents(uint32_t events) {
  if (connected_) {
    if (events & EPOLLRDHUP) {
      disconnected();
      return;
    } 
    if (events & EPOLLIN) {
      dataAvailable();
      flush();
    }
  }
}

/** @brief Server calls this method to signal the start of the session 
 *  Override accepted() to perform initial actions when a session starts */ 
void Session::accepted() {
  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET,&(addr_),ip,INET_ADDRSTRLEN);
  clog << "Connection from " << ip << ":" << port_ << " accepted" << endl;
  clog.flush();
}

/** @brief   Starts a graceful shutdown of the session 
 *  Override disconnect() to send any last messages required before the session is terminated.
 *  Be sure to call flush() to ensure the data is actually written to the write buffer. */
void Session::disconnect() {
  disconnected();
}

/** @brief  Called in response to a disconnected TCP Connection
 *  Override disconnected() to perform cleanup operations when a connection is unexpectedly lost */
void Session::disconnected() {
  if (connected_) { 
    connected_ = false;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&(addr_),ip,INET_ADDRSTRLEN);
    clog << ip << ":" << port_ << " disconnected" << endl;
    clog.flush();
    ::shutdown(socket(),SHUT_RDWR); 
    delete this;
  }  
}

/* Loopback Session */

/** @brief Read incoming data and send it back byte for byte */
void LoopbackSession::dataAvailable() {
  //char s[255];
  char c;
  c = streambuf_.sbumpc();
  while (c != traits_type::eof()) {
    streambuf_.sputc(c);
    c = streambuf_.sbumpc();
  } 

  //*this >> s;
  //*this << s;
  //copy(istreambuf_iterator<char> {this->rdbuf()},istreambuf_iterator<char> {},ostreambuf_iterator<char> {this->rdbuf()});
  //*this << this->rdbuf();
}

}