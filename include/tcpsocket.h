#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include <iostream>
#include <streambuf>
#include <vector>

namespace tcp {

using namespace std;

class streambuf : public std::streambuf {
  public:
    explicit streambuf(int socket, size_t rx_buff_sz = 256, size_t tx_buff_sz = 256, size_t put_back = 8);
    // moving and copying not allowed :
    streambuf(const streambuf &) = delete;
    streambuf &operator= (const streambuf &) = delete;
    streambuf(streambuf &&) = delete;
    streambuf &operator= (streambuf &&) = delete;
  protected:
    int_type underflow() override;
    streamsize showmanyc() override;
    streamsize xsgetn (char* s, streamsize n) override;
    int_type overflow(int_type ch) override;
    int sync() override;
    streamsize xsputn (const char* s, streamsize n) override;
  private:
    int internalflush(bool more);
    int socket_;
    const size_t put_back_;
    vector<char> recvbuffer_;
    vector<char> sendbuffer_;
};

class Socket : public iostream {
  private:
    int socket_;
    tcp::streambuf streambuf_;
  public:
    Socket(int socket) : iostream(&streambuf_), socket_(socket), streambuf_(socket)   { }
    int socket() { return socket_; }
  protected:
    virtual void dataAvailable() = 0;
};

} // namespace tcp

#endif // include guard