/** @file     tcpsocket.h
 *  @brief    Shared base classes for tcpclient.h and tcpserver.h
 *  @details  Provides Linux epoll event management, openSSL interface, and socket I/O buffering 
 *  @remarks  This library is thread safe
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
#include <deque>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "tcpssl.h"

/** @brief A tcp client/server library for linux that supports openSSL and EPoll */
namespace tcp {

using namespace std;

/** @brief   Set the output stream used by the library for log, warning and error messages.
 *  @details Defaults to clog */
void setLogStream(ostream *os);

/** @brief  Send an error message to the log stream */
void error(string msg);

/** @brief  Send a labelled error message to the log stream */
void error(string label, string msg);

/** @brief  Send a warning message to the log stream */
void warning(string msg);

/** @brief  Send an labelled warning message to the log stream */
void warning(string label, string msg);

/** @brief  Send an log message to the log stream */
void log(string msg);

/** @brief  Send a labelled log message to the log stream */
void log(string label, string msg);

class Socket;
class SSLContext;

/** @brief   Determines the state of a socket. 
 *  @details Not all states are valid for every socket type. */
enum class SocketState {UNCONNECTED=0, LISTENING, CONNECTING, CONNECTED, DISCONNECTED};

/** @brief   Encapsulates the EPoll interface
 *  @details Applications need to provide an epoll object for each thread in the application
 *           that uses sockets. These threads then call `EPoll.poll(100)` at regular intervals 
 *           to check for and respond to network events. 
 *  
 *  @details Appropriate events are added/remove from the epoll event list when a tcp::Socket 
 *           is created/destroyed. See the protected tcp::Socket.setEvents() method if you need
 *           to change which events a socket listens to.
 *
 *  @details When incoming events are recieved, they are automatically dispatched to the virtual
 *           Socket.handleEvents() method.
 */
class EPoll {
  public:
    /** @brief Constructor */
    EPoll();

    /** @brief Destructor */
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
  
    /** @brief Construct a blocking or non-blocking socket handle that responds to certain epoll events 
     *  @param domain Either AF_INET or AF_INET6
     *  @param socket The socket handle to encapsulate. If 0 is provided, a socket handle will be automatically created.
     *  @param blocking If true, a blocking socket will be created. If false, a non-blocking socket will be created.
     *  @param events A bit flag of the epoll events to register interest include
     * 
     *  @remark A client or server listener will typically call the constructor with socket=0 to start with a new socket.
     *  @remark A server session will create a Socket by providing the socket handle returned from an accept command.
     */
    Socket(EPoll &epoll, const int domain = AF_INET, const int socket = 0, const bool blocking = false, const int events = (EPOLLIN | EPOLLRDHUP));

    /** @brief Closes and destroys the socket
     *  @remark An active socket should first be shut down using the disconnect() command  */
    ~Socket();
  
    /** @brief Return the linux socket handle */
    int socket() const { return socket_; }
    
    /** @brief Return the socket domain (AF_INET or AF_INET6) */
    int domain() const { return domain_; }

    /** @brief   Shuts down the socket gracefully */
    virtual void disconnect();

  protected:

    /** @brief Changes which epoll events the socket listens for.
     *  Descendant classes may want to override this
     *  @param events A bitmask of event flags. See the epoll documentation */
    bool setEvents(int events);

    /** @brief   Called when the socket recieves an epoll event
     *  @details Descendant classes override this abstract method to respond to epoll events
     *  @param   events   A bitmask of event flags. See the epoll documentation */
    virtual void handleEvents(uint32_t events) = 0;

    /** @brief   Called when a connection is disconnected due to a network error
     *  @details Sets the socket state to DISCONNECTED and frees its resources. 
     *           Override disconnected to perform additional cleanup of a dropped socket connection. */
    virtual void disconnected();

    /** @brief   The mutex used to provide exclusive access to the socket */
    recursive_mutex mtx;

    /** @brief   Returns a reference to the epoll instance used by this socket */
    EPoll &epoll() { return epoll_; }

    /** @brief   Descendant classes can manipulate the socket state directly */
    SocketState state_;    

  private:
    EPoll &epoll_;
    int events_;
    int domain_;
    int socket_;
    friend class EPoll;
};

/** @brief   Represents a buffered socket that can send and receive data using optional SSL encryption
 *  @details This class provides properties and methods common to both the Client and Session classes */
class DataSocket : public Socket {
  public:
    DataSocket(EPoll &epoll, const int domain = AF_INET, const int socket = 0, const bool blocking = false, const int events = (EPOLLIN | EPOLLRDHUP)) :
      Socket(epoll,domain,socket,blocking,events) {}

    /** @brief Returns the number of bytes available in the inputBuffer */
    size_t available() { return inputBuffer.size(); }

    /** @brief   Reads up to size bytes from inputBuffer into buffer
     *  @returns The number of bytes actually read */
    size_t read(void *buffer, size_t size);
    
    /** @brief   Writes the contents of buffer to the outputBuffer
     *  @details The content of the outputBuffer will be sent automatically at the next EPoll event */
    size_t write(const void *buffer, size_t size);

  protected:

    /** @brief Reads all available data from the socket into inputBuffer */
    void readToInputBuffer();

    /** @brief   Writes all available data from the outputBuffer to the socket 
     *  @details Any data that could not be written will be retained in the outputBuffer and sent 
     *           with the next call to sendOutputBuffer() */
    void sendOutputBuffer();

    /** @brief   Sets the epoll event flags
     *  @details The flags will include EPOLLOUT if value==true */
    void canSend(bool value);

    /** @brief   Called by the EPoll class when the listening socket recieves an epoll event
     *  @details Calls either disconnected(), readToInputBuffer() + dataAvailable() or sendOutputBuffer()
     *           depending on the events that have been set. */        
    void handleEvents(uint32_t events) override;
    
    /** @brief   Shuts down any SSL connection gracefully
     *  @details Socket::disconnect() is called to shutdown the underlying socket */
    void disconnect() override;

    /** @brief   Called when a connection is disconnected
     *  @details Frees the SSL object that is associated with the connection
     */
    void disconnected() override;

    /** @brief    Called whenever new data is appended to the inputBuffer
     *  @details  Clients should override the dataAvailable method to do something in 
     *            response to received data */
    virtual void dataAvailable() = 0;

    /** @brief    Factory method for returning an SSL object.
     *  @details  Override to replace the SSL class used. */
    virtual SSL *createSSL(SSLContext *context);

    /** @brief   Exposes the underlying SSL record used for openSSL calls to descendant classes */
    SSL *ssl_ {nullptr};

  private:    
    size_t read_(void *buffer, size_t size);
    size_t write_(const void *buffer, size_t size);
    deque<uint8_t> inputBuffer;
    deque<uint8_t> outputBuffer;
    friend class SSL;
};

/** @brief   Tries to determine which address family to use from a host and port string
 *  @details If host is other than a numeric address, the address family will be detemined through a
 *           canonical name lookup
 *  @return  AF_INET or AF_INET6 if an address family can be determined, AF_UNSPEC otherwise */
int getDomainFromHostAndPort(const char* host, const char* port, int def_domain = AF_INET);

} // namespace tcp

#endif // include guard
