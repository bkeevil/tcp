#include "tcpserver.h"
#include <fcntl.h>
#include <sys/types.h>
#include <linux/if_link.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <unistd.h>

namespace tcp {

using namespace std;

Server::Server(const int domain, const int port, const string bindaddr) 
  : Socket(domain,0,false,EPOLLIN), port_(port)
{
  memset(&addr_,0,sizeof(addr_));  
  if ((bindaddr == "") || (bindaddr == "0.0.0.0") || (bindaddr == "any")) {
    if (domain == AF_INET) {
      reinterpret_cast<struct sockaddr_in*>(&addr_)->sin_addr.s_addr = INADDR_ANY;
    }
    if (domain == AF_INET6) {
      reinterpret_cast<struct sockaddr_in6*>(&addr_)->sin6_addr = IN6ADDR_ANY_INIT;
    }
  } else {
    if (!findifaddr(bindaddr,reinterpret_cast<struct sockaddr*>(&addr_))) {
      cerr << "Interface " << bindaddr << " not found" << endl;
    }
  }
  if (domain == AF_INET) {
    addr_.ss_family = AF_INET;
    reinterpret_cast<struct sockaddr_in*>(&addr_)->sin_port = htons(port);
  }
  if (domain == AF_INET6) {
    addr_.ss_family = AF_INET6;
    reinterpret_cast<struct sockaddr_in6*>(&addr_)->sin6_port = htons(port);
  }  
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
  ::close(getSocket());
}

void Server::handleEvents(uint32_t events) {
  if (listening_ && (events & EPOLLIN)) {
    acceptConnection();
  }
}

void Server::printifaddrs() {
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
          exit(EXIT_FAILURE);
        }
        cout << family << "    " << item->ifa_name << "   address: " << host << endl;
      }
      item = item->ifa_next;
    }
  } else {
    cerr << "printifaddrs:" << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
  freeifaddrs(list);
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
      if ((f == getDomain()) && ((ifname.compare(item->ifa_name) == 0) || (ifname.compare(host) == 0))) {
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
    exit(EXIT_FAILURE);
  }
  freeifaddrs(list);
  return result;
}

bool Server::bindToAddress() {
  if (::bind(getSocket(),addr(),addrlen()) == -1) {
    cerr << "bind: " << strerror(errno) << endl;
    cerr.flush();
    return false;
  } else {
    char ip[INET6_ADDRSTRLEN];
    memset(&ip,0,INET6_ADDRSTRLEN);      
    if (addr_.ss_family == AF_INET) {
      struct sockaddr_in * a = reinterpret_cast<struct sockaddr_in*>(addr());
      if (a->sin_addr.s_addr == INADDR_ANY) {
        clog << "Server bound to any IP4";
      } else {
        inet_ntop(AF_INET,&a->sin_addr,ip,INET_ADDRSTRLEN);  
        clog << "Server bound to " << ip;
      }
      clog << " on port " << ntohs(a->sin_port) << endl;
    } else {
      struct sockaddr_in6 * a = reinterpret_cast<struct sockaddr_in6*>(addr());
      inet_ntop(AF_INET6,&a->sin6_addr,ip,INET6_ADDRSTRLEN);  
      clog << "Server bound to " << ip << " on port " << ntohs(a->sin6_port) << endl;
    }
    clog.flush();
    return true;
  }
}

bool Server::startListening() {
  if (listen(getSocket(),listenBacklog_) == -1) {
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
  int conn_sock = ::accept(getSocket(),(struct sockaddr *) &peer_addr, &peer_addr_len);
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

/* Session */

Session::~Session() { 
  server_.sessions.erase(getSocket());
}

void Session::handleEvents(uint32_t events) {
  if (connected_) {
    if (events & EPOLLRDHUP) {
      disconnected();
      clear();
      return;
    } 
    if (events & EPOLLIN) {
      clear();
      dataAvailable();
      flush();
    }
  }
}

/** @brief Server calls this method to signal the start of the session 
 *  Override accepted() to perform initial actions when a session starts */ 
void Session::accepted() {
  char ip[INET_ADDRSTRLEN];
  inet_ntop(getDomain(),&(addr_),ip,INET_ADDRSTRLEN);
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
    inet_ntop(getDomain(),&(addr_),ip,INET_ADDRSTRLEN);
    clog << ip << ":" << port_ << " disconnected" << endl;
    clog.flush();
    close(getSocket());
    delete this;
  }  
}

}