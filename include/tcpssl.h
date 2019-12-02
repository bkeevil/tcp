/** @file     tcpssl.h
 *  @brief    OpenSSL utilities for tcpclient.h and tcpserver.h
 *  @details   
 *  @remarks  
 *  @author   Bond Keevil
 *  @version  1.0
 *  @date     2019
 *  @copyright  GPLv3.0
 */

#ifndef TCP_SSL_H
#define TCP_SSL_H

#include <iostream>
#include <streambuf>
#include <openssl/ssl.h>

/** @brief Contains Socket, Server, Session and Client classes for a tcp client/server */
namespace tcp {

using namespace std;

class SSLContext {
  public:
    SSLContext(bool supportTLSOnly = true);
    ~SSLContext();
    SSL_CTX *context() const { return ctx; }
    static bool supportTLSOnly() { return supportTLSOnly_; }
    static void supportTLSOnly(bool value) { supportTLSOnly_ = value; }
  private:
    static SSL_CTX *ctx;
    static bool supportTLSOnly_;
    static int refcount_;
};

} // namespace mqtt

#endif // include guard