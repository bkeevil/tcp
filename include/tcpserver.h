/** @file    tcpserver.h
 *  @brief   Provides basic tcp server functionality
 *  @details Create descendants of tcp::Server and tcp::Session to implement a custom tcp server
 *  @author  Bond Keevil
 *  @version 1.0
 *  @date    2019
 *  @license GPLv3.0
 */
#ifndef TCP_SERVER_H
#define TCP_SERVER_H

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

// Forward declarations

class Server;
class Session;  

/** @brief Listens for TCP connections and establishes Sessions
 *  Construct an instance of tcp::server to start the server. Destroy the object to stop the server.
 *  Override the virtual createSession() method to return a custom session descendant class.
 */
class Server : public SocketHandle {
  public:
    /** @brief Construct a server instance and start listening for connections
     *  Check the value of listening() to determine if the server was started successfully
     *  To stop the server, destroy the server instance.
     *  @param port The port to listen on. If port is 0 then a port number will be chosen automatically.
     *  @param addr The address to bind to. If left at the default of INADDR_ANY then the server will 
     *              bind to all addresses. */
    Server(const int port = 0, const in_addr_t addr = INADDR_ANY);
    
    /** @brief Destroy the server instance
     *  Destoying the server stops it from listening and ends all sessions by calling the 
     *  Session::disconnect() method. */
    ~Server();

    /** @brief Sets listenBacklog to value
     *  The listen backlog is the maximum number of incomming connections pending acception
     *  before the OS stops allowing new connections. */
    void setListenBacklog(int value) { listenBacklog_ = value; }
    
    /** @brief Retrieves the value of listenBacklog
     *  The listen backlog is the maximum number of incomming connections pending acception
     *  before the OS stops allowing new connections. */
    int listenBacklog() const { return listenBacklog_; } 
    
    /** @brief Returns true if the server is listening
     *  listening() will return false if the server was not able to start listening during its 
     *  construction. It also returns false while the server is being destroyed. */
    bool listening() { return listening_; } 

    /** @brief Retrieve the port number the server is listening on */
    in_port_t port() const { return port_; }

    /** @brief Retrieve the address the server listening socket is bound to */
    in_addr_t addr() const { return addr_; }

  protected:
    /** @brief Called by the EPoll class when the listening socket recieves an event from the OS.
     *  The listening socket recieves the EPOLLIN event when a new connection is available to be 
     *  accepted. This is handled by tcp::Server, which calls the acceptConnection method. */
    void handleEvents(uint32_t events) override;
    
    /** @brief Called when the server needs to create a new object of the tcp::Session class.
     *  Users of this component need to create their own custom tcp::Session class and use 
     *  this method to return an instance to it.
     *  @param socket The socket handle to pass to the constructor of tcp::Session descendant
     *  @param peer_address The address and port of the connected peer */
    virtual Session* createSession(const int socket, const sockaddr_in peer_address);

    /** @brief Maps socket handles to their corresponding tcp::Session objects
     *  Descendant classes may need access to the sessions map. */
    std::map<int,tcp::Session*> sessions;

  private:
    bool bindToAddress();
    bool startListening();
    bool acceptConnection();
    int listenBacklog_ {50};
    bool listening_;
    in_port_t port_ {0};
    in_addr_t addr_ {INADDR_ANY};
    friend class Session;
};

/** @brief Represents a TCP connection accepted by the Server 
 *  A Session descendant class is instantiated by the server using the Session::createSession() method.
 *  It is destroyed by calling disconnect() or disconnected() */
class Session : public SocketHandle, public iostream {
  public:
    
    /** @brief  Returns a reference to the Server that owns this Session */
    Server& server() const { return server_; }

    /** @brief  Returns the peer port number used to connect to this Session */
    in_port_t peer_port() const { return port_;   }

    /** @brief  Returns the peer address used to connect to this Session */
    in_addr_t peer_address() const { return addr_;   }
    
    /** @brief  Returns true if the session is connected to a peer */
    bool connected() const { return connected_; }

    /** @brief  Shutdown the socket and close the session 
     *  @details Internally calls disconnect()
     *  @details Override disconnect() to perform additional cleanup operations before a session is
     *           intentionally closed.
     *  @warning disconnect() causes the Session to be destroyed
    */
    virtual void disconnect();
  protected:
    
    /** @brief  Creates a new session
     *  The constructor is protected and is called by the Server::createSession() method */
    Session(Server& server, const int socket, const struct sockaddr_in peer_addr) 
      : SocketHandle(socket), iostream(socket), server_(server), port_(peer_addr.sin_port), addr_(peer_addr.sin_addr.s_addr) { }
    
    /** @brief The destructor is protected and is called by the disconnect() or disconnected() methods */
    virtual ~Session();

    /** @brief Called by the EPoll object to process OS events sent to this handle
     *  Session monitors for available data and disconnected TCP sessions */
    virtual void handleEvents(uint32_t events) override;

    /** @brief Called in response to data being available on the socket
     *  Override this virtual abstract method in descendent classes to read incoming data */
    virtual void dataAvailable() = 0;

    /** @brief Called by the Server::acceptConnection after a connection has been accepted
     *  Override accepted to perform operations when a session is first established.
     *  In the base class, accepted() prints a message to clog.
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

  private:
    Server& server_;
    in_port_t port_;
    in_addr_t addr_;
    bool connected_;
    friend class Server;
};

class LoopbackSession : public Session {
  public:
    LoopbackSession(Server& server, const int socket, const struct sockaddr_in peer_addr) : Session(server,socket,peer_addr) {}
    void dataAvailable() override;
};

}

#endif