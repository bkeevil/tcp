/** @file    tcpserver.h
 *  @brief   Provides basic tcp server functionality
 *  @details Create descendants of tcp::Server and tcp::Session to implement a custom tcp server
 *  @author  Bond Keevil
 *  @version 1.0
 *  @date    2019
 *  @copyright GPLv3.0
 */

#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <iostream>
#include <map>
#include <string.h>
#include "tcpsocket.h"
#include "tcpssl.h"

namespace tcp {

using namespace std;

// Forward declarations:

class Server;
class Session;  

/** @brief   Listens for TCP connections and establishes Sessions
 *  @details Construct an instance of tcp::server to start the server. Destroy the object to stop the server.
 *  @remark  Override the virtual createSession() method to return a custom session descendant class.
 */
class Server : public Socket {
  public:
    /** @brief   Construct a server instance
     *  @param   domain   Either AF_INET or AF_INET6
     */
    Server(const int domain = AF_INET) : Socket(domain,0,false,EPOLLIN) {}
    
    /** @brief   Destroy the server instance
     *  @details Destoying the server stops it from listening and ends all sessions by calling the 
     *           Session::disconnect() method. 
     */
    virtual ~Server();

    /** @brief   Start up the server */
    void start();

    /** @brief   Stop the server */
    void stop();

    /** @brief   Sets listenBacklog
     *  @details The listen backlog is the maximum number of incomming connections pending acception
     *           before the OS stops allowing new connections. 
     *  @default 50
     */
    void setListenBacklog(int value) { listenBacklog_ = value; }
    
    /** @brief   Gets listenBacklog
     *  @details The listen backlog is the maximum number of incomming connections pending acception
     *           before the OS stops allowing new connections. 
     *  @default 50
     */ 
    int listenBacklog() const { return listenBacklog_; } 
    
    /** @brief   Determine if the server is listening
     *  @returns Returns true if the server is listening
     *  @returns Returns false if the server was not able to start listening. 
     *  @returns Returns false while the server is being destroyed. 
     */
    bool listening() { return listening_; } 

    /** @brief   Retrieve the IP address the server listening socket is bound to.
     *  @remark  Set to INADDR_ANY to listen on all interfaces
     *  @default INADDR_ANY
     */
    struct sockaddr * addr() { return (struct sockaddr*)&addr_; }

    /** @brief   Returns the length of the sockaddr structure returned by addr() */
    int addrlen() const { return (getDomain() == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)); }
    
    /** @brief   Print a list of interface addresses to cout */
    bool printifaddrs();

    /** @brief   Returns an interface address from an interface name and domain */
    bool findifaddr(const string ifname, sockaddr *addr);

    /** @brief The port number to listen on */
    in_port_t port {0};

    /** @brief The interface address to bind to */
    string bindaddress {""};

    /** @brief Get the SSL context for the server */
    SSLContext &ctx() { return ctx_; }

    /** @brief   Getter for useSSL property */
    bool useSSL {false};
    
  protected:
  
    /** @brief   Called by the EPoll class when the listening socket recieves an event from the OS.
     *  @details The listening socket recieves the EPOLLIN event when a new connection is available to be 
     *           accepted. This is handled by tcp::Server, which calls the acceptConnection method. 
     */
    void handleEvents(uint32_t events) override;
    
    /** @brief   Called when the server needs to create a new object of the tcp::Session class.
     *  @details Users of this component need to create their own custom tcp::Session class and use 
     *           this method to return an instance to it.
     *  @param   socket       The socket handle to pass to the constructor of tcp::Session descendant
     *  @param   peer_address The address and port of the connected peer 
     */
    virtual Session* createSession(const int socket, const sockaddr_in peer_address) = 0;

    /** @brief   Maps socket handles to their corresponding tcp::Session objects
     *  @details Descendant classes may need access to the sessions map. 
     */
    std::map<int,tcp::Session*> sessions;

  private:
    bool bindToAddress();
    bool startListening();
    bool acceptConnection();
    in_port_t port_ {0}; 
    string bindaddress_ {""};
    int listenBacklog_ {128};
    SSLContext ctx_ {SSLMode::SERVER};
    bool listening_;
    struct sockaddr_storage addr_;
    friend class Session;
};

/** @brief   Represents a TCP connection accepted by the Server 
 *  @details A Session descendant class is instantiated by the server using the Session::createSession() 
 *           method. It is destroyed by calling disconnect() or disconnected() 
 */
class Session : public Socket {
  public:
    
    /** @brief  Returns a reference to the Server that owns this Session */
    Server &server() const { return server_; }

    /** @brief  Returns the peer port number used to connect to this Session */
    in_port_t peer_port() const { return port_;   }

    /** @brief  Returns the peer address used to connect to this Session */
    in_addr_t peer_address() const { return addr_;   }
    
    /** @brief  Returns true if the session is connected to a peer */
    bool connected() const { return connected_; }

    /** @brief   Shutdown the socket and close the session 
     *  @details Internally calls disconnect()
     *  @details Override disconnect() to perform additional cleanup operations before a session is
     *           intentionally closed.
     *  @warning disconnect() causes the Session to be destroyed
     */
    virtual void disconnect();

  protected:
    
    /** @brief   Creates a new session
     *  @details The constructor is protected and is called by the Server::createSession() method 
     */
    Session(Server& server, const int socket, const struct sockaddr_in peer_addr) 
      : Socket(server.getDomain(),socket), server_(server), port_(peer_addr.sin_port), addr_(peer_addr.sin_addr.s_addr) { }
    
    /** @brief The destructor is protected and is called by the disconnect() or disconnected() methods */
    virtual ~Session();

    /** @brief   Handles EPoll events by routing them to a Socket
     *  @details Called by the EPoll object to process OS events sent to this handle
     *           Session monitors for available data and disconnected TCP sessions 
     */
    virtual void handleEvents(uint32_t events) override;

    /** @brief   Called in response to data being available on the socket
     *  @details Override this virtual abstract method in descendent classes to read incoming data */
    virtual void dataAvailable() = 0;

    /** @brief   Called by the Server::acceptConnection after a connection has been accepted
     *  @details Override accepted to perform operations when a session is first established.
     *           In the base class, accepted() prints a message to clog.
     */
    virtual void accepted();
    
    /** @brief   Called when a tcp connection is dropped 
     *  @details Shuts down the network socket, removes itself from Server.sessions[], then destroys itself.
     *  @details An application can override disconnected() to perform additional cleanup operations 
     *           before the underlying TCP connection gets torn down. 
     *  @remark  To intentionally close a Session, call disconnect() instead
     *  @warning disconnected() destroys the Session
     */
    virtual void disconnected();

    int available();
    int read(void *buffer, const int size);
    int peek(void *buffer, const int size);
    int write(const void *buffer, const int size, const bool more = false);

  private:
    Server& server_;
    in_port_t port_;
    in_addr_t addr_;
    bool connected_;
    SSL *ssl_;
    friend class Server;
};

}

#endif