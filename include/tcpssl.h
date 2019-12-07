/** @file    tcpssl.h
 *  @brief   Provides ssl client and server functionality
 *  @details A single SSLContext class is shared by all clients of an application
 *           A single SSLContext class is created for each listening server in an application
 *  @author  Bond Keevil
 *  @version 1.0
 *  @date    2019
 *  @copyright GPLv3.0
 */

#ifndef TCP_SSL_H
#define TCP_SSL_H

#include <string>
#include <openssl/ssl.h>
#include "tcpsocket.h"

namespace tcp {

using namespace std;

/** @brief   Initialize the openSSL library
 *  @details Applications should call this method once at application startup.
 *           It must be called before any other openSSL library functions are called.
 */
void initSSLLibrary();

/** @brief   Free up resources created by the openSSL library
 *  @details Applications should call this method at application shutdown.
 */
void freeSSLLibrary();

/** @brief   This method logs openSSL errors to cerr */
void printSSLErrors();

class DataSocket;

enum class SSLMode { CLIENT, SERVER };

/** @brief   Encapsulates an SSL_CTX record */
class SSLContext {
  public:
    SSLContext(SSLMode mode);
    virtual ~SSLContext();
    void setOptions(bool verifypeer = false, bool compression = true, bool tlsonly = false);
    bool useDefaultVerifyPaths() { return setVerifyPaths(NULL,NULL); }
    bool setVerifyPaths(const char *cafile = NULL, const char *capath = NULL);
    bool setCertificateAndKey(const char *certfile, const char *keyfile);
    virtual int passwordCallback(char *buf, int size, int rwflag);
    SSL_CTX *handle() { return ctx_; }
    SSLMode mode() { return mode_; }
    string keypass;
  private:
    SSL_CTX *ctx_;
    SSLMode mode_;
};

/** @brief   Encapsulates an SSL record */
class SSL {
  public:
    SSL(DataSocket &owner, SSLContext &context);
    virtual ~SSL();
    void setOptions(bool verifypeer = false);
    bool setCertificateAndKey(const char *certfile, const char *keyfile);
    virtual int passwordCallback(char *buf, int size, int rwflag);
    bool setfd(int socket);
    bool connect();
    bool accept();
    size_t read(void *buffer, size_t size);
    size_t write(const void *buffer, size_t size);
    void clear();
    void shutdown();
    ::SSL *handle() { return ssl_; }
    SSLMode mode() { return mode_; }
    string keypass;
  protected:
    void wantsRead();
    void wantsWrite();
  private:
    DataSocket &owner_;
    SSLMode mode_;
    ::SSL *ssl_;
};

} // namespace

#endif