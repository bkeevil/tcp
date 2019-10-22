#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include <iostream>
#include <streambuf>
#include <vector>

namespace tcp {

using namespace std;

class streambuf : public std::streambuf {
    public:
        streambuf(int socket, size_t buff_sz = 256, size_t put_back = 8);
        // moving and copying not allowed
        streambuf(const streambuf &) = delete;
        streambuf &operator= (const streambuf &) = delete;
        streambuf(streambuf &&) = delete;
        streambuf &operator= (streambuf &&) = delete;
    private:
        int_type underflow() override;
    private:
        int socket_;
        const size_t put_back_;
        vector<char> buffer_;
};

class Socket {
  public:
    Socket(int socket) : socket_(socket), streambuf_(socket), stream_(&streambuf_)  { }
    int socket() { return socket_; }
    iostream& stream() { return stream_; }
  protected:
    //void sync();
    virtual void dataAvailable() = 0;
  private:
    int socket_;
    tcp::streambuf streambuf_;
    iostream stream_;
};

} // namespace tcp

#endif // include guard