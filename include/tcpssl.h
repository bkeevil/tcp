#ifndef TCP_SSL_H
#define TCP_SSL_H

#include <openssl/ssl.h>

namespace tcp {

void initSSLLibrary();
void freeSSLLibrary();
void printSSLErrors();

class SSLContext {
  public:
    SSLContext(bool server);
    ~SSLContext();
    bool setOptions(bool verifypeer = false, bool compression = true, bool tlsonly = true);
    bool useDefaultVerifyPaths() { setVerifyPaths(NULL,NULL); }
    bool setVerifyPaths(char *cafile = NULL, char *capath = NULL);
    bool setCertificateAndKey(char *certfile, char *keyfile);
    char *keypass;
    virtual int passwordCallback(char *buf, int size, int rwflag);
    SSL_CTX *handle() { return ctx_; }
  private:
    SSL_CTX *ctx_;
    bool server_;
};

class SSLSession {
  public:
    SSLSession(SSLContext &context);
    ~SSLSession();
    bool setOptions(bool verifypeer = false);
    bool setCertificateAndKey(char *certfile, char *keyfile);
    char *keypass;
    virtual int passwordCallback(char *buf, int size, int rwflag);
    SSL *handle() { return ssl_; }
  private:
    SSL *ssl_;
};

} // namespace

#endif