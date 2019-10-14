#include <iostream>
#include <string>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

using namespace std;

#define LISTEN_BACKLOG 50

int openSocket() {
  int res = socket(AF_INET,SOCK_STREAM,0);
  if (res == -1) {
    perror("socket");
  }
  return res;
}

int bindToSocket(int tcp_socket) {
  struct sockaddr_in my_addr;
  memset(&my_addr,0,sizeof(sockaddr_in));
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(1883);
  my_addr.sin_addr.s_addr = INADDR_ANY;    
  
  int res = bind(tcp_socket,(struct sockaddr *)&my_addr,sizeof(my_addr));
  if (res == -1) {
    perror("bind");
  } 
  return res;
}

int startListening(int tcp_socket) {
  int res = listen(tcp_socket,LISTEN_BACKLOG);
  if (res == -1) {
    perror("listen");
  }
  return res;
}

int setNonBlocking(int tcp_socket) {
  int flags = fcntl(tcp_socket,F_GETFL,0);
  if (flags == -1) {
    perror("fnctl");
    return -1;
  } else {
    flags = (flags & ~O_NONBLOCK);
    int res = fcntl(tcp_socket,F_SETFL,flags);
    if (res == -1) {
      perror("fnctl");
    }
    return res;
  }
}

int acceptClient(int tcp_socket) {
  struct sockaddr_in peer_addr;
  socklen_t peer_addr_size;
  int res = accept(tcp_socket,(struct sockaddr*)&peer_addr,&peer_addr_size);
  if (res == -1) {
    perror("accept");
  }
  return res;
}

void waitForConnections() {
  cout << "Waiting for incomming connections" << endl;
  cout.flush();
}

int main(int argc, char** argv) {  
  int tcp_socket = openSocket();
  if (tcp_socket == -1) {
    return 1;
  } else {
    if (bindToSocket(tcp_socket) == -1) {
      close(tcp_socket);
      return 1;
    } else {
      if (setNonBlocking(tcp_socket) == -1) {
        close(tcp_socket);
        return 1;
      } else {
        if (startListening(tcp_socket) == -1) {
          close(tcp_socket);
          return 1;
        } else {
          waitForConnections(); 
          close(tcp_socket);
          return 0;
        }
      }
    }
  }
}
