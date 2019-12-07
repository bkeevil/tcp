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
    
    /** @brief   Returns the ssl context object
     *  @details A single SSLContext object is shared by all client connections in the library
     *           this allows default options, like a CA certificate, to be applied to all new 
     *           client connections. */
    static SSLContext &ctx() { return ctx_; }
   
    /** @brief   Return the client state. See the SocketState enum for possible values */
    SocketState state() { return state_; }

    /** @brief   Returns the port number used for the last call to connect() */
    in_port_t port() { return port_; }
    
    /** @brief   Returns the ip address number used for the last call to connect() */
    in_addr_t addr() { return addr_; }
   
    /** @brief   Check if the peer certificate subject name matches the host name 
     *  @details If true and verifyPeer is true then this flag will cause an additional
     *           check that the server certificates subject name == the hostname used in
     *           the connect string. If it does not match the connection will not complete. */
    bool checkPeerSubjectName { false };

    /** @brief   Initiates a connection to a server
     *  @details If the client is a blocking client, the call blocks until a connection is established. 
     *  @param   hostName [in]  The hostname or ip address of the server to connect to
     *  @param   port     [in]  The port number to connect to
     *  @param   useSSL   [in]  If true, this connection will be an SSL connection
     *  @remark  Check the value of state() after a call to connect() to determine if the socket is CONNECTED or is CONNECTING
     *  @return  True if the connection was initiated
     */
    virtual bool connect(const string &hostName, in_port_t port, bool useSSL = false);

    bool validatePeerCertificate();
    
  protected:
    
    /** @brief   Receive epoll events sent to this socket
     *  @details Extends the behavior of DataSocket::handleEvents to monitors for an initial 
     *           EPOLLIN signal when a new non-blocking connection has been established. */
    void handleEvents(uint32_t events) override; 

    /** @brief   Called when the client connects to the server
     *  @details Override connected() to perform operations when a connection is established
     *           The default implementation initiates the SSL_connect handshake.
     *  @returns false if there is a peer certificate validation failure */ 
    virtual void connected();

    friend class SSL;
  
  private:
    in_port_t port_ {0};
    in_addr_t addr_ {0};
    static SSLContext ctx_; 
};

} // namespace mqtt

#endif