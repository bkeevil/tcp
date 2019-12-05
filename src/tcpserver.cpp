#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/if_link.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <unistd.h>
#include "tcpserver.h"

namespace tcp {

using namespace std;

Server::~Server() {
  if (listening())
    stop();
}

void Server::start(in_port_t port, string bindaddress, bool useSSL, int backlog)
{
  useSSL_ = useSSL;
  memset(&addr_,0,sizeof(addr_));  
  if ((bindaddress == "") || (bindaddress == "0.0.0.0") || (bindaddress == "::")) {
    if (domain() == AF_INET) {
      reinterpret_cast<struct sockaddr_in*>(&addr_)->sin_addr.s_addr = INADDR_ANY;
    }
    if (domain() == AF_INET6) {
      reinterpret_cast<struct sockaddr_in6*>(&addr_)->sin6_addr = IN6ADDR_ANY_INIT;
    }
  } else {
    if (!findifaddr(bindaddress,reinterpret_cast<struct sockaddr*>(&addr_))) {
      cerr << "Interface " << bindaddress << " not found" << endl;
    }
  }
  if (domain() == AF_INET) {
    addr_.ss_family = AF_INET;
    reinterpret_cast<struct sockaddr_in*>(&addr_)->sin_port = htons(port);
  }
  if (domain() == AF_INET6) {
    addr_.ss_family = AF_INET6;
    reinterpret_cast<struct sockaddr_in6*>(&addr_)->sin6_port = htons(port);
  }  

  int enable = 1;
  if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
    cerr << "Server could not set socket option SO_REUSEADDR" << endl;  

  if (bindToAddress((struct sockaddr*)&addr_,(domain_ == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)))) {
    startListening(backlog);
  }
}

void Server::stop()
{ 
  if (listening()) {
    clog << "Sending disconnect to all sessions" << endl;
    clog.flush();
    std::map<int,Session*>::iterator it;
    for (it = sessions.begin();it != sessions.end();++it) {
      it->second->disconnect();
    }
  }
  ::close(socket_);
}

void Server::handleEvents(uint32_t events) {
  if (listening() && (events & EPOLLIN)) {
    acceptConnection();
  }
}

bool Server::printifaddrs() {
  struct ifaddrs *list, *item;
  string family;
  char host[NI_MAXHOST];

  if (getifaddrs(&list) == 0) {
    item = list;
    while (item != nullptr) {
      int f = item->ifa_addr->sa_family;
      if ((f == AF_INET) || (f == AF_INET6)) {
        switch (f) {
          case AF_INET: family.assign("AF_INET"); break;
          case AF_INET6: family.assign("AF_INET6"); break;
          default: family.assign("");
        };
        int res = getnameinfo(item->ifa_addr,(f == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        if (res != 0) {
          cerr << "printifaddrs: " << gai_strerror(res) << endl;
          return false;
        }
        cout << family << "    " << item->ifa_name << "   address: " << host << endl;
      }
      item = item->ifa_next;
    }
  } else {
    cerr << "printifaddrs:" << strerror(errno) << endl;
    return false;
  }
  freeifaddrs(list);
  return true;
}

bool Server::findifaddr(const string ifname, sockaddr *addr) {
  struct ifaddrs *list, *item;
  int f;
  string family;
  char host[NI_MAXHOST];
  bool result = false;

  if (getifaddrs(&list) == 0) {
    item = list;
    while (item != nullptr) {
      f = item->ifa_addr->sa_family;
      if (getnameinfo(item->ifa_addr,(f == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) != 0) {
        host[0] = 0;
      }
      if (f == domain_ && ((ifname.compare(item->ifa_name) == 0) || (ifname.compare(host) == 0))) {
        if (f == AF_INET) {
          *(sockaddr_in*)addr = *(sockaddr_in*)item->ifa_addr;
          result = true;
          break;
        }
        if (f == AF_INET6) {
          *(sockaddr_in6*)addr = *(sockaddr_in6*)item->ifa_addr;
          result = true;
          break;
        }
      }
      item = item->ifa_next;
    }
  } else {
    cerr << "findifaddrs:" << strerror(errno) << endl;
    return false;
  }
  freeifaddrs(list);
  return result;
}

bool Server::bindToAddress(sockaddr *addr, socklen_t len) {
  if (::bind(socket_,addr,len) == -1) {
    cerr << "bind: " << strerror(errno) << endl;
    cerr.flush();
    return false;
  } else {
    char ip[INET6_ADDRSTRLEN];
    memset(&ip,0,INET6_ADDRSTRLEN);      
    if (addr->sa_family == AF_INET) {
      struct sockaddr_in * a = reinterpret_cast<struct sockaddr_in*>(addr);
      if (a->sin_addr.s_addr == INADDR_ANY) {
        clog << "Server bound to any IP4";
      } else {
        inet_ntop(AF_INET,&a->sin_addr,ip,INET_ADDRSTRLEN);  
        clog << "Server bound to " << ip;
      }
      if (a->sin_port == 0) {
        clog << " on random port " << endl;
      } else {
        clog << " on port " << ntohs(a->sin_port) << endl;
      }
    } else {
      struct sockaddr_in6 * a = reinterpret_cast<struct sockaddr_in6*>(addr);
      inet_ntop(AF_INET6,&a->sin6_addr,ip,INET6_ADDRSTRLEN);  
      if (strcmp(ip,"::") == 0) {
        clog << "Server bound to any IP6";
      } else {
        clog << "Server bound to " << ip;
      }
      if (a->sin6_port == 0) {
        clog << " on random port " << endl;
      } else {
        clog << " on port " << ntohs(a->sin6_port) << endl;
      }
    }
    clog.flush();
    return true;
  }
}

bool Server::startListening(int backlog) {
  if (listen(socket_,backlog) == -1) {
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
  int conn_sock = ::accept(socket_,(struct sockaddr *) &peer_addr, &peer_addr_len);
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
    return session->connected();
  }
}

/* Session */

Session::~Session() { 
  server_.sessions.erase(socket_);
}

/** @brief Server calls this method to signal the start of the session 
 *  Override accepted() to perform initial actions when a session starts */ 
void Session::accepted() {
  char ip[INET6_ADDRSTRLEN];
  inet_ntop(domain_,&addr_,ip,INET_ADDRSTRLEN);
  clog << "Connection from " << ip << ":" << port_ << " accepted" << endl;
  clog.flush();
  if (server().useSSL_) {
    ssl_ = new tcp::SSL(*this,server().ctx());
    ssl_->setfd(socket_);
    if (ssl_->accept())
      state_ = SocketState::CONNECTED;
    else
      disconnected();
  } else {
    state_ = SocketState::CONNECTED;
  }
}

/** @brief   Starts a graceful shutdown of the session 
 *  Override disconnect() to send any last messages required before the session is terminated.
 *  Be sure to call flush() to ensure the data is actually written to the write buffer. */
void Session::disconnect() {
  if (ssl_) {
    ssl_->shutdown();
    printSSLErrors();    
  }
  disconnected();
}

/** @brief  Called in response to a disconnected TCP Connection
 *  Override disconnected() to perform cleanup operations when a connection is unexpectedly lost */
void Session::disconnected() {
  if (connected()) { 
    if (ssl_) {
      delete ssl_;
      ssl_ = nullptr;
      printSSLErrors();
    }
    state_ = SocketState::DISCONNECTED;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(domain_,&(addr_),ip,INET_ADDRSTRLEN);
    clog << ip << ":" << port_ << " disconnected" << endl;
    clog.flush();
    delete this;
  }  
}

}