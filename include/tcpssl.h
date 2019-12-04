#ifndef TCP_SSL_H
#define TCP_SSL_H

#include <openssl/ssl.h>

namespace tcp {

void initSSLLibrary();
void freeSSLLibrary();
void printSSLErrors();

enum class SSLMode { CLIENT, SERVER };

class SSLContext {
  public:
    SSLContext(SSLMode mode);
    virtual ~SSLContext();
    void setOptions(bool verifypeer = false, bool compression = true, bool tlsonly = true);
    bool useDefaultVerifyPaths() { return setVerifyPaths(NULL,NULL); }
    bool setVerifyPaths(const char *cafile = NULL, const char *capath = NULL);
    bool setCertificateAndKey(const char *certfile, const char *keyfile);
    char *keypass;
    virtual int passwordCallback(char *buf, int size, int rwflag);
    SSL_CTX *handle() { return ctx_; }
    SSLMode mode() { return mode_; }
  private:
    SSL_CTX *ctx_;
    SSLMode mode_;
};

class SSL {
  public:
    SSL(SSLContext &context);
    virtual ~SSL();
    void setOptions(bool verifypeer = false);
    bool setCertificateAndKey(const char *certfile, const char *keyfile);
    char *keypass;
    virtual int passwordCallback(char *buf, int size, int rwflag);
    ::SSL *handle() { return ssl_; }
    bool setfd(int socket);
    bool connect();
    bool accept();
    int pending();
    int peek(void *buffer, const int size);
    int read(void *buffer, const int size);
    int write(const void *buffer, const int size);
    void clear();
    void shutdown();
    SSLMode mode() { return mode_; }
  private:
    SSLMode mode_;
    ::SSL *ssl_;
};

} // namespace

#endif