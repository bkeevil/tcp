/** @file     tcpsocket.h
 *  @brief    Shared base classes for tcpclient.h and tcpserver.h
 *  @details  Provides Linux epoll event management, a socket streambuf class, and an iostream based interface 
 *  @remarks  Applications need to call `epoll.poll(100)` regularly in their code for network 
 *            events to function correctly.
 *  @author   Bond Keevil
 *  @version  1.0
 *  @date     2019
 *  @copyright  GPLv3.0
 */

#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include <iostream>
#include <vector>
#include <map>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <openssl/ssl.h>

/** @brief Contains Socket, Server, Session and Client classes for a tcp client/server */
namespace tcp {

using namespace std;

class Socket;

/** @brief   Encapsulates the EPoll interface
 *  @details Applications need to call EPoll.poll() at regular intervals to check for and respond to network
 *           events. A global `epoll` singleton object is provided for applications.
 *  
 *  @details Appropriate events are added/remove from the epoll event list when a tcp::Socket 
 *           is created/destroyed. See the protected tcp::Socket.setEvents() method if you need
 *           to change which events to listen to.
 * 
 *  @details When incoming events are recieved, they are automatically dispatched to the appropriate 
 *           tcp::Socket.handleEvents() method.
 *
 *  @details It is possible to have more than one EPoll in a multi-threaded application
 *           but the poll() method for each instance has to be called individually.
 */
class EPoll {
  public:
    EPoll();
    ~EPoll();
    /** @brief Call poll() regularly to respond to network events 
     *  @param timeout Number of ms to wait for an event. Can be zero. */
    void poll(int timeout); 
  private:
    static const int MAX_EVENTS = 10; /**< Maximum number of epoll events to handle per poll() call */
    bool add(Socket& socket, int events);
    bool update(Socket& socket, int events);
    bool remove(Socket& socket);    
    void handleEvents(uint32_t events, int fd);
    int handle_;
    epoll_event events[MAX_EVENTS];
    std::map<int,tcp::Socket*> sockets;
    friend class Socket;
};

/** @brief Encapsulates a socket handle that is capable of recieving epoll events */
class Socket {
  public:
  
    /**
     * @brief Construct a blocking or non-blocking socket handle that responds to certain epoll events 
     * 
     * @param socket The socket handle to encapsulate. If 0 is provided, a socket handle will be automatically created.
     * @param blocking If true, a blocking socket will be created. If false, a non-blocking socket will be created.
     * @param events A bit flag of the epoll events to register interest include
     * 
     * @remark A client or server listener will typically call the constructor with socket=0 to start with a new socket.
     * @remark A server session will create a Socket by providing the socket handle returned from an accept command.
     */
    Socket(const int domain = AF_INET, const int socket = 0, const bool blocking = false, const int events = (EPOLLIN | EPOLLRDHUP));

    /** @brief Closes the socket (if necessary) and destroys the Socket
     *  @remark The socket should first be shut down using the Linux shutdown() command  */
    ~Socket();
  
    /** @brief Return the OS socket handle */
    int getSocket() const { return socket_; }
    
    /** @brief Return the socket domain (AF_INET, AF_INET6, AF_UNIX) */
    int getDomain() const { return domain_; }

  protected:
    
    /** @brief Changes which epoll events the socket listens for.
     *  Descendant classes may want to override this
     *  @param events A bitmask of event flags. See the epoll documentation */
    bool setEvents(int events);

    /** @brief   Called when the socket recieves an event from the OS
     *  @details Descendant classes override this abstract method to respond to epoll events
     *  @param   events   A bitmask of event flags. See the epoll documentation */
    virtual void handleEvents(uint32_t events) = 0;
  
    void initSSL(const bool server, const char *certfile = nullptr, const char *keyfile = nullptr, const char *cafile = nullptr, const char *capath = nullptr);
    void freeSSL();
    void printSSLErrors();
    static SSL_CTX *ctx() { return ctx_; }

    int socket_;
  private:
    int domain_;
    int events_;
    static SSL_CTX *ctx_;
    static int refcount_;
    static bool sslinitialized_;
    friend class EPoll;
};

extern EPoll epoll;

} // namespace tcp

#endif // include guard
