/** @file    tcpclient.h
 *  @brief   Provides basic tcp client functionality
 *  @details Create descendants of tcp::Client to implement a custom tcp client
 *  @author  Bond Keevil
 *  @version 1.0
 *  @date    2019
 *  @license GPLv3.0
 */

#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <iostream>
#include <map>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include "tcpsocket.h"

namespace tcp {

using namespace std;
using namespace tcp;

/** @brief  A blocking or non-blocking TCP client connection */
class Client : public Socket, public iostream {
  public:
    
    /** @brief  Determines the state of a Client connection */
    enum class State { UNCONNECTED=0, CONNECTING, CONNECTED, DISCONNECTED };
    
    /** @brief  Creates a blocking or a non-blocking client */
    /** @param blocking If true, a blocking socket will be created. Otherwise a non-blocking socket is created */
    Client(bool blocking = false): Socket(0,blocking), iostream(socket()) {}
    
    /** @brief   Destroys the client 
      * @details Will call disconnect() first if necessary. This allows clients to be disconnected 
      *          by destroying them. 
      */
    ~Client() { if ((state_ == State::CONNECTED) || (state_ == State::CONNECTING)) disconnect(); };
    
    /** @brief Return the client state. See the State enum class for possible values */
    State state() { return state_; }

    /** @brief Returns the port number used for the last call to connect() */
    in_port_t port() { return addr_; }
    
    /** @brief Returns the ip address number used for the last call to connect() */
    in_addr_t addr() { return port_; }

    /** @brief   Initiates a connection to a server
     *  @details If the client is a blocking client, the call blocks until a connection is established. 
     *  @remark  Check the value of state() to determine if a non-blocking socket CONNECTED or is CONNECTING
     *  @return  True if the connection was initiated
     */
    virtual bool connect(in_addr_t addr, in_port_t port);

    /** @brief   Shutdown the socket and closes the connection 
     *  @details Internally calls disconnect()
     *  @details Override disconnect() to perform additional cleanup operations before a session is
     *           intentionally closed.
     */
    virtual void disconnect();

  protected:
    
    /** @brief Called by the EPoll object to process OS events sent to this handle
     *  Session monitors for available data and disconnected TCP sessions 
     */
    void handleEvents(uint32_t events) override;

    /** @brief An event handler called when the client connects to the server
     *  Override connected() to perform operations when a connection is first established 
     */ 
    virtual void connected();
    
    /** @brief   Called when a tcp connection is dropped 
     *  Shuts down the network Socket.
     *  An application can override disconnected() to perform additional cleanup operations 
     *  before the underlying TCP connection gets torn down. 
     *  @remark  To intentionally close a Session, call disconnect() instead 
     */
    virtual void disconnected();

    /** @brief    Called when data is available for reading on the socket
     *  @details  Clients must override the abstract dataAvailable method to do something in 
     *            response to received data 
     */
    virtual void dataAvailable() = 0;  
    
  private:
    State state_ {State::UNCONNECTED};
    in_port_t port_ {0};
    in_addr_t addr_ {0};    
};

}

#endif