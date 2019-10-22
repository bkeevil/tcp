#include "tcpserver.h"

namespace tcp {

using namespace std;

bool Server::start() {
  stop();
  terminated_ = false;
  if (!openSocket()) {
    return false;
  }
  if (!setNonBlocking(listen_socket)) { 
    stop(); 
    return false; 
  }
  if (!bindToAddress()) { 
    stop(); 
    return false; 
  }
  if (!startListening()) { 
    stop(); 
    return false; 
  }
  if (!initEPoll()) { 
    stop(); 
    return false; 
  }
  return true;
}

void Server::stop() {
  if (!terminated_) {
    cout << "Sending disconnect to all sessions" << endl;
    std::map<int,Session*>::iterator it;
    for (it = sessions.begin();it != sessions.end();++it) {
      it->second->disconnect();
    }

    terminated_ = true;
    if (listen_socket >= 0) {
      close(listen_socket);
      listen_socket = 0; 
    }
    cout << "Destroying sessions" << endl;
    for (it = sessions.begin();it != sessions.end();++it) {
      delete it->second;
    }
    if (epoll_fd >= 0) {
      close(epoll_fd);
      epoll_fd = 0;
    }
    sessions.clear(); // Should already be cleared: Session destructor calls Server::closeConnection()
  }
}

bool Server::poll(int timeout) {
  bool res = true;
  int nfds = epoll_wait(epoll_fd,events,MAX_EVENTS,timeout);
  if (nfds == -1) {
    perror("epoll_wait");
    return false;
  }

  for (int n = 0; n < nfds; ++n) {
    if (events[n].data.fd == listen_socket) {
      res &= acceptConnection();
    } else {
      handleEvent(events[n].events,events[n].data.fd);
    }
  }
  return res;
}

bool Server::openSocket() {
  listen_socket = socket(AF_INET,SOCK_STREAM,0);
  if ( listen_socket == -1) {
    perror("socket");
    return false;
  } else {
    cout << "Server socket handle opened" << endl;
    return true;
  }
}

bool Server::bindToAddress() {
  struct sockaddr_in server_addr;
  memset(&server_addr,0,sizeof(sockaddr_in));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port_);
  server_addr.sin_addr.s_addr = addr_;    
  if (bind(listen_socket,(struct sockaddr *)&server_addr,sizeof(server_addr)) == -1) {
    perror("bind");
    return false;
  } else {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&(server_addr.sin_addr),ip,INET_ADDRSTRLEN);
    cout << "Server bound to " << ip << " on port " << port_ << endl;
    return true;
  }
}

bool Server::startListening() {
  if (listen(listen_socket,LISTEN_BACKLOG) == -1) {
    perror("listen");
    return false;
  } else {
    cout << "Server started listening" << endl;
    return true;
  }
}

bool Server::setNonBlocking(const int fd) {
  int flags = fcntl(fd,F_GETFL,0);
  if (flags == -1) {
    perror("fnctl: get");
    return false;
  } else {
    flags = (flags & ~O_NONBLOCK);
    if (fcntl(fd,F_SETFL,flags) == -1) {
      perror("fnctl: set");
      return false;
    }
    return true;
  }
}

bool Server::initEPoll() {
  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    perror("epoll_create1");
    return false;
  }

  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = listen_socket;
  if (epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listen_socket,&ev) == -1) {
    perror("epoll_ctl: listen_sock");
    return false;
  }
  return true;
}

bool Server::acceptConnection() {
  struct sockaddr_in peer_addr;
  socklen_t peer_addr_len = sizeof(struct sockaddr_in);
  int conn_sock = ::accept(listen_socket,(struct sockaddr *) &peer_addr, &peer_addr_len);
  if (conn_sock == -1) {
    perror("accept");
    return false;
  } else {
    setNonBlocking(conn_sock);
    struct epoll_event ev;
    //ev.events = EPOLLIN;
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.fd = conn_sock;
    //cout << "epoll_fd=" << epoll_fd << endl;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
      perror("epoll_ctl_add: conn_sock");
      return false;
    }
    // Delete any existing sessions with the same socket handle
    Session* session = sessions[conn_sock];
    sessions.erase(conn_sock);
    if (session != nullptr) {
      cout << "WARNING: A session with socket handle " << conn_sock << " already exists. Deleting it." << endl;
      delete session;
    } 
    // Start a new session and accept it
    session = createSession(conn_sock,peer_addr);
    sessions[conn_sock] = session;
    session->accepted();
    return true;
  }
}

void Server::closeConnection(Session* session) {
  if (session != nullptr) {
    sessions.erase(session->socket()); 
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, session->socket(), 0) == -1) {
      perror("epoll_ctl_del: conn_sock");
    }
  }  
}

void Server::handleEvent(uint32_t events, int fd) {
  Session* session = sessions[fd];
  if (session != nullptr) {
    if ((events & EPOLLRDHUP) || (events & EPOLLHUP)) {
      char ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET,&(session->addr_),ip,INET_ADDRSTRLEN);
      cout << ip << ":" << session->port_ << " disconnected" << endl;
      cout.flush();
      session->disconnected();
      delete session;
    } else {
      if (events & EPOLLIN) {
        session->dataAvailable();
        session->flush();
      }
    }
  }
}

Session* Server::createSession(const int socket, const sockaddr_in peer_address) {
  LoopbackSession* session = new LoopbackSession(*this,socket,peer_address);
  return dynamic_cast<Session*>(session); 
}

/* Session */

Session::~Session() {
  if (!disconnected_) 
    disconnect(); 
  server_.closeConnection(this);
}

void Session::accepted() {
  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET,&(addr_),ip,INET_ADDRSTRLEN);
  cout << "Connection from " << ip << ":" << port_ << " accepted" << endl;
  cout.flush();
}

void Session::disconnect() {
  flush();
  disconnected_ = true;
  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET,&(addr_),ip,INET_ADDRSTRLEN);
  cout << ip << ":" << port_ << " disconnecting" << endl;
  cout.flush();
  ::shutdown(socket(),SHUT_RDWR);
}

/* Loopback Session */

void LoopbackSession::dataAvailable() {
  //char s[255];
  char c;
  std::streambuf* buf = rdbuf();
  c = buf->sbumpc();
  while (c != traits_type::eof()) {
    buf->sputc(c);
    c = buf->sbumpc();
  } 

  //*this >> s;
  //*this << s;
  //copy(istreambuf_iterator<char> {this->rdbuf()},istreambuf_iterator<char> {},ostreambuf_iterator<char> {this->rdbuf()});
  //*this << this->rdbuf();
}

}