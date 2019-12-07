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

/** @brief Contains Socket, Server, Session and Client classes for a tcp client/server */
namespace tcp {

using namespace std;

class Socket;

/** @brief   Determines the state of a socket. 
 *  @details Not all states are valid for every socket type. */
enum class SocketState {UNCONNECTED=0, LISTENING, CONNECTING, CONNECTED, DISCONNECTED};


/** @brief   Encapsulates the EPoll interface
 *  @details Applications need to call `EPoll.poll(100)` at regular intervals to check for and respond to network
 *           events. A global `epoll` singleton object is provided for single threaded applications to use. 
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
     * @param domain Either AF_INET or AF_INET6
     * @param socket The socket handle to encapsulate. If 0 is provided, a socket handle will be automatically created.
     * @param blocking If true, a blocking socket will be created. If false, a non-blocking socket will be created.
     * @param events A bit flag of the epoll events to register interest include
     * 
     * @remark A client or server listener will typically call the constructor with socket=0 to start with a new socket.
     * @remark A server session will create a Socket by providing the socket handle returned from an accept command.
     */
    Socket(EPoll &epoll, const int domain = AF_INET, const int socket = 0, const bool blocking = false, const int events = (EPOLLIN | EPOLLRDHUP));

    /** @brief Closes the socket (if necessary) and then odestroys the Socket
     *  @remark An active socket should first be shut down using the disconnect() command  */
    ~Socket();
  
    /** @brief Return the OS socket handle */
    int socket() const { return socket_; }
    
    /** @brief Return the socket domain (AF_INET, AF_INET6, AF_UNIX) */
    int domain() const { return domain_; }

    /** @brief   Shuts down the socket gracefully */
    virtual void disconnect();

  protected:

    /** @brief Changes which epoll events the socket listens for.
     *  Descendant classes may want to override this
     *  @param events A bitmask of event flags. See the epoll documentation */
    bool setEvents(int events);

    /** @brief   Called when the socket recieves an event from the OS
     *  @details Descendant classes override this abstract method to respond to epoll events
     *  @param   events   A bitmask of event flags. See the epoll documentation */
    virtual void handleEvents(uint32_t events) = 0;

    /** @brief   Called when a connection is disconnected due to a network error
     *  @details Sets the socket state to DISCONNECTED and frees its resources. 
     *           Override disconnected to perform additional cleanup of a dropped socket connection. */
    virtual void disconnected();

    /** @brief   Mutex for providing exclusive access to the socket */
    recursive_mutex mtx;

    /** @brief   Returns a pointer to the epoll instance used by this socket */
    EPoll &epoll() { return epoll_; }

    /** @brief   Either AF_INET or AF_INET6 */
    int domain() { return domain_; }

    /** @brief   The unix socket handle */
    int socket() { return socket_; }

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

    /** @brief Returns an estimate of how many bytes are available in the inputBuffer */
    size_t available() { return inputBuffer.size(); }

    /** @brief   Reads up to size bytes from inputBuffer into buffer
     *  @returns The number of bytes actually read */
    size_t read(void *buffer, size_t size);
    
    /** @brief   Writes the contents of buffer to the outputBuffer
     *  @details The content of the outputBuffer will be sent automatically at the next EPoll event */
    size_t write(const void *buffer, size_t size);

    /** @brief If true, the client will attempt to verify the peer certificate */
    bool verifypeer {false};

    /** @brief The filename of the client certificate in PEM format */
    string certfile;

    /** @brief The filename of the client private key in PEM format */
    string keyfile;

    /** @brief The password for the private key file, if required */
    string keypass;

  protected:

    /** @brief Reads all available data from the socket into inputBuffer */
    void readToInputBuffer();

    /** @brief   Writes all available data from the outputBuffer to the socket 
     *  @details Any data that could not be written will be retained in the outputBuffer and sent 
     *           with the next call to sendOutputBuffer() */
    void sendOutputBuffer();

    /** @brief   Sets the value of the sockets EPoll event flags
     *  @details The flags will include EPOLLOUT if value==true */
    void canSend(bool value);

    /** @brief   Called by the EPoll class when the listening socket recieves an event from the OS.
     *  @details Calls either disconnected(), readToInputBuffer() + dataAvailable() or sendOutputBuffer()
     *           depending on the events that have been set. */        
    void handleEvents(uint32_t events) override;
    
    /** @brief   Shuts down any SSL connection gracefully and frees its resources
     *  @details Socket::disconnect() is called to shutdown the underlying socket */
    void disconnect() override;

    /** @brief   Called when a connection is disconnected
     *  @details Frees the SSL object that is associated with the connection
     */
    void disconnected() override;

    /** @brief    Called when new data is available for reading from inputBuffer
     *  @details  Clients should override the dataAvailable method to do something in 
     *            response to received data 
     */
    virtual void dataAvailable() = 0;

    /** @brief   Exposes the underlying SSL record used for openSSL calls to descendant classes */
    SSL *ssl_ {nullptr};

  private:    
    size_t read_(void *buffer, size_t size);
    size_t write_(const void *buffer, size_t size);
    deque<uint8_t> inputBuffer;
    deque<uint8_t> outputBuffer;
    friend class SSL;
};

} // namespace tcp

#endif // include guard
