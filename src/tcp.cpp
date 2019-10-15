#include "tcp.h"

namespace tcp {

using namespace std;

bool Server::start() {
  clearSessions();
  if (!openSocket()) return false;
  if (!bindToAddress()) { stop(); return false; }
  if (!setNonBlocking(listen_fd)) { stop(); return false; }
  if (!startListening()) { stop(); return false; }
  if (!initEPoll()) { stop(); return false; }
  terminated_ = false;
  return true;
}

void Server::stop() {
  cout << "Stopping server" << endl;
  if (!terminated_) {
    disconnectSessions();
    terminated_ = true;
    if (listen_fd >= 0) {
      close(listen_fd);
      listen_fd = 0; 
    }
    if (epoll_fd >= 0) {
      close(epoll_fd);
      epoll_fd = 0;
    }
    clearSessions();
  }
}

bool Server::poll() {
  bool res = true;
  int nfds = epoll_wait(epoll_fd,events,MAX_EVENTS,-1);
  if (nfds == -1) {
    perror("epoll_wait");
    return false;
  }

  for (int n = 0; n < nfds; ++n) {
    if (events[n].data.fd == listen_fd) {
      res &= acceptConnection();
    } else {
      handleEvent(events[n].events,events[n].data.fd);
    }
  }
  return res;
}

bool Server::openSocket() {
  listen_fd = socket(AF_INET,SOCK_STREAM,0);
  if ( listen_fd == -1) {
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
  if (bind(listen_fd,(struct sockaddr *)&server_addr,sizeof(server_addr)) == -1) {
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
  if (listen(listen_fd,LISTEN_BACKLOG) == -1) {
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
  ev.data.fd = listen_fd;
  if (epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listen_fd,&ev) == -1) {
    perror("epoll_ctl: listen_sock");
    return false;
  }
  return true;
}

bool Server::acceptConnection() {
  struct sockaddr_in peer_addr;
  socklen_t peer_addr_len = sizeof(struct sockaddr_in);
  int conn_sock = accept(listen_fd,(struct sockaddr *) &peer_addr, &peer_addr_len);
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
      perror("epoll_ctl: conn_sock");
      return false;
    }
    // Delete any existing sessions with the same fd handle
    Session* session = removeSession(conn_sock);
    if (session != nullptr) {
      cout << "WARNING: A session with socket handle " << conn_sock << " already exists. Deleting it." << endl;
      delete session;
    } 
    // Start a new session and accept it
    session = new Session(this,conn_sock,peer_addr);
    addSession(session);
    session->accepted();
    return true;
  }
}

void Server::handleEvent(uint32_t events, int fd) {
  Session* session = findSession(fd);
  if (session != nullptr) {
    if (events & EPOLLRDHUP) {
      cout << "EPOLLRDHUB received" << endl;
      delete session;
    } else if (events & EPOLLHUP) {
      cout << "EPOLLHUB received" << endl;
      delete session;
    } else {
      /*if (events & EPOLLOUT) {
        cout << "EPOLLOUT received" << endl;
        session->canSend();
      }*/
      if (events & EPOLLIN) {
        //cout << "EPOLLIN received" << endl;
        session->dataAvailable();
      }
    }
  }
}

/** @brief Adds session to the list of sessions
 *  @param session   The session to add
 */
void Server::addSession(Session* session) {
  if (top_ != nullptr) {
    session->next_ = top_;
  }
  top_ = session;
}

/** @brief Finds and removes the session from the session list
 *  @param session   The session to remove
 *  @return true if a matching session was found and removed from the list
 *  @remark The session object is not destroyed
 */
bool Server::removeSession(Session* session) {
  Session* ptr = top_;
  Session* prev = nullptr;
  if (session != nullptr) {
    while (ptr != nullptr) {
      if (ptr == session) {
        if (prev == nullptr) {
          top_ = ptr->next_;
        } else {
          prev->next_ = ptr->next_;
        }
        return true;
      }
      prev = ptr;
      ptr = ptr->next_;
    }
  }
  return false;
}

/** @brief Finds and removes the session with a matching socket file descriptor
 *  @param fd   The socket file descriptor to remove for
 *  @return A pointer to the Session object or nullptr if not found
 *  @remark The session object is not destroyed
 */
Session* Server::removeSession(int fd) {
  Session* ptr = top_;
  Session* prev = nullptr;
  
  while (ptr != nullptr) {
    if (ptr->fd_ == fd) {
      if (prev == nullptr) {
        top_ = ptr->next_;
      } else {
        prev->next_ = ptr->next_;
      }
      return ptr;
    }
    prev = ptr;
    ptr = ptr->next_;
  }
  return nullptr;
}

/** @brief Finds a session with a matching socket file descriptor
 *  @param fd   The socket file descriptor to search for
 *  @return A pointer to the Session object or nullptr if not found
 */
Session* Server::findSession(int fd) {
  Session* ptr = top_;
  while (ptr != nullptr) {
    if (ptr->fd_ == fd) {
      return ptr;
    }
    ptr = ptr->next_;
  }
  return nullptr;
}

/** @brief Calls disconnect() on all sessions 
 *  @details Called by stop() to signal a graceful shutdown of the server
 */
void Server::disconnectSessions() {
  Session* ptr = top_;
  while (ptr != nullptr) {
    ptr->disconnect();
    ptr = ptr->next_;
  }
}

/** @brief Clears the session list
 *  @details Calls the destructor for each session, removing it from the list
 */
void Server::clearSessions() {
  while (top_ != nullptr) {
    delete top_;
  }
  top_ = nullptr;
}

/* Session */

void Session::accepted() {
  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET,&(peer_addr_.sin_addr),ip,INET_ADDRSTRLEN);
  cout << "Connection from " << ip << ":" << peer_addr_.sin_port << " accepted" << endl;
}

void Session::dataAvailable() {
  cout << "Data available. Socket Handle = " << fd_ << endl;
}

void Session::canSend() {
  cout << "Can Send. Socket Handle = " << fd_ << endl;
}

void Session::disconnect() {
  cout << "Session accepted. Socket Handle = " << fd_ << endl;
}

}