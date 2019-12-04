#ifndef TCP_SSL_H
#define TCP_SSL_H

#include <string>
#include <openssl/ssl.h>
#include "tcpsocket.h"

namespace tcp {

using namespace std;

void initSSLLibrary();
void freeSSLLibrary();
void printSSLErrors();

enum class SSLMode { CLIENT, SERVER };

class SSLContext {
  public:
    SSLContext(SSLMode mode);
    virtual ~SSLContext();
    void setOptions(bool verifypeer = false, bool compression = true, bool tlsonly = false);
    bool useDefaultVerifyPaths() { return setVerifyPaths(NULL,NULL); }
    bool setVerifyPaths(const char *cafile = NULL, const char *capath = NULL);
    bool setCertificateAndKey(const char *certfile, const char *keyfile);
    string keypass;
    virtual int passwordCallback(char *buf, int size, int rwflag);
    SSL_CTX *handle() { return ctx_; }
    SSLMode mode() { return mode_; }
  private:
    SSL_CTX *ctx_;
    SSLMode mode_;
};

class SSL {
  public:
    SSL(Socket &owner, SSLContext &context);
    virtual ~SSL();
    void setOptions(bool verifypeer = false);
    bool setCertificateAndKey(const char *certfile, const char *keyfile);
    string keypass;
    virtual int passwordCallback(char *buf, int size, int rwflag);
    ::SSL *handle() { return ssl_; }
    bool setfd(int socket);
    bool connect();
    bool accept();
    int read(void *buffer, const int size);
    int write(const void *buffer, const int size);
    void clear();
    void shutdown();
    SSLMode mode() { return mode_; }
  protected:
    void wantsRead();
    void wantsWrite();
  private:
    Socket &owner_;
    SSLMode mode_;
    ::SSL *ssl_;
};

} // namespace

#endif