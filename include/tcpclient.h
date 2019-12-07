/** @file    tcpclient.h
 *  @brief   Provides basic tcp client functionality
 *  @details Create descendants of tcp::Client to implement a custom tcp client
 *  @author  Bond Keevil
 *  @version 1.0
 *  @date    2019
 *  @copyright GPLv3.0
 */

#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <iostream>
#include <map>
#include <string.h>
#include <vector>
#include <deque>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netdb.h>
#include "tcpsocket.h"
#include "tcpssl.h"

namespace tcp {

using namespace std;
using namespace tcp;

/** @brief  A blocking or non-blocking TCP client connection */
class Client : public DataSocket {
  public:
    
    /** @brief  Creates a blocking or a non-blocking client
     *  @param doain    Either AF_INET or AF_INET6
     *  @param blocking If true, a blocking socket will be created. Otherwise a non-blocking socket is created 
     */
    Client(EPoll &epoll, const int domain = AF_INET, bool blocking = false) : DataSocket(epoll,domain,0,blocking) {}
    
    /** @brief   Destroys the client 
      * @details Will call disconnect() first if necessary. This allows clients to be disconnected 
      *          by destroying them. 
      */
    virtual ~Client();
    
    /** @brief Returns the ssl context object for all client connections */
    static SSLContext &ctx() { return ctx_; }
   
    /** @brief Return the client state. See the State enum for possible values */
    SocketState state() { return state_; }

    /** @brief Returns the port number used for the last call to connect() */
    in_port_t port() { return port_; }
    
    /** @brief Returns the ip address number used for the last call to connect() */
    in_addr_t addr() { return addr_; }
   
    /** @brief   Initiates a connection to a server
     *  @details If the client is a blocking client, the call blocks until a connection is established. 
     *  @remark  Check the value of state() to determine if a non-blocking socket CONNECTED or is CONNECTING
     *  @return  True if the connection was initiated
     */
    virtual bool connect(const string &hostname, const in_port_t port, bool useSSL = false);

    /** @brief   Disconnects the session gracefully 
     *  @details Override disconnect() to perform additional cleanup operations before a session is
     *           intentionally closed.
     */
    virtual void disconnect();

  protected:
    
    /** @brief   Called by the EPoll object to process OS events sent to this handle
     *  @details Extends the behavior of DataSocket::handleEvents to monitors for an initial 
     *           EPOLLIN signal that a new non-blocking connection has been established.
     */
    void handleEvents(uint32_t events) override;

    /** @brief   An event handler called when the client connects to the server
     *  @details Override connected() to perform operations when a connection is first established 
     */ 
    virtual void connected();
    
    friend class SSL;
  
  private:
    in_port_t port_ {0};
    in_addr_t addr_ {0};
    static SSLContext ctx_; 
};

}

#endif