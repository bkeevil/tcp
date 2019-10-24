/** @file     tcpsocket.h
 *  @brief    Shared base classes for tcpclient.h and tcpserver.h
 *  @details  Provides Linux epoll event management, a socket streambuf class, and an iostream based interface 
 *  @remarks  Applications need to call `epoll.poll(100)` regularly in their code for network 
 *            events to function correctly.
 *  @author   Bond Keevil
 *  @version  1.0
 *  @date     2019
 *  @license  GPLv3.0
 */

#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include <iostream>
#include <streambuf>
#include <vector>
#include <map>
#include <sys/epoll.h>

/** @brief Contains Socket, Server, Session and Client classes for a tcp client/server */
namespace tcp {

using namespace std;

class Socket;

/** @brief   Provides a streambuf interface to a unix socket handle
 *  @details See the streambuf documentation for your stdlib for more info on this class
 */
class streambuf : public std::streambuf {
  public:
    /** @brief   Create a new tcp::streambuf class
     *  @details This class is used by the tcp::iostram class to provide a C++ stream interface to client and
     *           server network sessions. It should not be necessary to use or instantiate this class directly.
     *  @param   socket     The socket handle to use for io
     *  @param   rx_buff_sz The size of the recvbuffer
     *  @param   tx_buff_sz The size of the sendbuffer
     *  @param   put_back   The maximum number of characters that can be put back on the stream 
     *  @remark  The put_back buffer is a space at the beginning of the recvbuffer that contains 
     *           the most recently read characters. This allows characters to be put back on the 
     *           stream after despite a buffer refresh.
     */   
    explicit streambuf(int socket, size_t rx_buff_sz = 256, size_t tx_buff_sz = 256, size_t put_back = 8);
    
    streambuf(const streambuf &) = delete; /**< Copy constructor not allowed */
    streambuf &operator= (const streambuf &) = delete; /**< Copy assignment operator not allowed */
    streambuf(streambuf &&) = delete; /**< Move constructor not allowed */
    streambuf &operator= (streambuf &&) = delete; /**< Move assignement operator not allowed */
    
    /** @brief Returns the Linux socket handle */
    int socket() const { return socket_; }

    /** @brief   Returns the number of bytes available to be read
     *  @details The value returned includes unread bytes in the recvbuffer plus any bytes in the operating 
     *           system's receive buffer that have yet to be retrieved.
     */  
    int available() { return showmanyc() + (egptr() - gptr()); }

  protected:

    // Overrides of descendant class. See https://cppreference.com for docs
    int_type underflow() override; /**< https://en.cppreference.com/w/cpp/io/basic_streambuf/underflow */
    streamsize showmanyc() override; /**< https://en.cppreference.com/w/cpp/io/basic_streambuf/showmanyc */
    streamsize xsgetn (char* s, streamsize n) override; /**< https://en.cppreference.com/w/cpp/io/basic_streambuf/sgetn
    int_type overflow(int_type ch) override; /**< https://en.cppreference.com/w/cpp/io/basic_streambuf/overflow */
    int sync() override; /**< https://en.cppreference.com/w/cpp/io/basic_streambuf/pubsync */
    streamsize xsputn (const char* s, streamsize n) override; /**< https://en.cppreference.com/w/cpp/io/basic_streambuf/sputn */
  
  private:
    /** @brief Sends the contents of the send buffer to the socket 
     * 
     *  @param more  Set to true to tell the operating system to wait for more data before sending 
     *               the packet. Set to false to send the packet immediately. */
    int internalflush(bool more);
    
    int socket_;
    const size_t put_back_;
    vector<char> recvbuffer_;
    vector<char> sendbuffer_;
};

/** @brief An iostream descendant that represents a network socket */
class iostream : public std::iostream {
  public:
    /** @brief Constructs an iostream that uses a tcp::streambuf to read/write a network socket */
    iostream(int socket) : std::iostream(&streambuf_), streambuf_(socket) { }
  protected:
    tcp::streambuf streambuf_;  
};

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
    Socket(const int socket = 0, const bool blocking = false, const int events = (EPOLLIN | EPOLLRDHUP));
  
    /** @brief Closes the socket (if necessary) and destroys the Socket
     *  @remark The socket should first be shut down using the Linux shutdown() command  */
    ~Socket();
  
    /** @brief Return the OS socket handle */
    int socket() const { return socket_; }
  
  protected:
    
    /** @brief Changes which epoll events the socket listens for.
     *  Descendant classes may want to override this
     *  @param events A bitmask of event flags. See the epoll documentation */
    bool setEvents(int events);

    /** @brief   Called when the socket recieves an event from the OS
     *  @details Descendant classes override this abstract method to respond to epoll events
     *  @param   events   A bitmask of event flags. See the epoll documentation */
    virtual void handleEvents(uint32_t events) = 0;
  
  private:
    int socket_;
    int events_;
    friend class EPoll;
};

/** @brief   A singleton EPoll object.
 *  @details The poll() method needs to be called at regular intervals
 *  @details It is possible to have more than one EPoll in a multi-threaded application
 *           but the poll() method for each instance has to be called individually.
 */
extern EPoll epoll;

} // namespace tcp

#endif // include guard