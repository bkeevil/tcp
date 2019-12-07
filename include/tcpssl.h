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

/** @brief   Encapsulates an openSSL SSL_CTX record */
class SSLContext {
  public:
    
    /** @brief   Constructor
     *  @param   mode [in]  Is this context record for client connections or a server */ 
    SSLContext(SSLMode mode);

    /** @brief   Destructor */
    virtual ~SSLContext();
    
    /** @brief   Sets default options for all SSL objects created from this context 
     *  @param   verifypeer  [in]  If true, the server/client will validate the peer certificate
     *  @param   compression [in]  Set to false to disable SSL compression
     *  @param   tlsonly     [in]  if true, disable the SSLv2 and SSLv3 protocols. This is recommended. 
     *  @remarks Logs error information to cerr but does not return error information.
     */
    void setOptions(bool verifypeer = false, bool compression = true, bool tlsonly = false);

    /** @brief   Use the default operating system certificate store for validation 
     *  @returns True if the CA filename or CA path were set successfully, false otherwise. Check cerr log for details.
     */
    bool useDefaultVerifyPaths() { return setVerifyPaths(NULL,NULL); }

    /** @brief   Use the specified CA certificate or certificate directory
     *  @details Provide one or the other or set both to NULL to use the operating system certificate store
     *  @param   cafile     [in]  The filename of a CA certificate chain in PEM (or CRT) format
     *  @param   capath     [in]  The path of a directory containing CA certificates that are considered valid 
     *  @returns True if the CA filename or CA path were set successfully, false otherwise. Check cerr log for details.
     */
    bool setVerifyPaths(const char *cafile = NULL, const char *capath = NULL);

    /** @brief   Sets the certificate filename and key filename
     *  @details Sets the certificate and key file for all SSL connections created from this context.
     *           This is primarily intended for servers but clients may want to connect to 
     *           more than one server using the same certificate and key.
     *           This default certificate and key set can be overriden for a specific connection
     *           using the equivelant method from the SSL class.
     *  @param   certfile    [in]  The filename of a certificate chain in PEM (or CRT) format
     *  @param   keyfile     [in]  The filename of a private key in PEM (or CRT) format
     *  @returns True of the certificate and key were successfully set, false otherwise. Check cerr log for details.
     *  @remarks If the keyfile requires a password to decrypt, passwordCallback will be called to provide 
     *           that password.
     */
    bool setCertificateAndKey(const char *certfile, const char *keyfile);
    
    /** @brief   Sets the value of the private key password */
    void setPrivateKeyPassword(string value) { keypass_ = value; }

    /** @brief   Called when a password to decrypt a private key is required
     *  @details Descendant classes may want to override this to provide a more secure means of storing the 
     *           private key password. The default behavior is to return the value of keypass_.
     *  @remarks See the openSSL function `SSL_CTX_set_default_passwd_cb` for more details
     *  @remarks This must be public because it is called from the passwordCallback() function
     *  @remarks This password callback function only applies to private keys assigned to this SSL_CTX object,
     *           it is not 'inherited' from any SSL objects created from it.
     */
    virtual int passwordCallback(char *buf, int size, int rwflag);

  protected:
    SSL_CTX *ctx_; /**< The openSSL context object */
    SSLMode mode_; /**< The mode of the context object is passed to SSL objects created from this context */
    friend class SSL;
  private:
    string keypass_;
};

/** @brief   Encapsulates an SSL connection data structure */
class SSL {
  public:
    SSL(DataSocket &owner, SSLContext &context);
    virtual ~SSL();
    void setOptions(bool verifypeer = false);
    bool setCertificateAndKey(const char *certfile, const char *keyfile);
    virtual int passwordCallback(char *buf, int size, int rwflag);
    bool setfd(int socket);

    /** @brief   Returns the peer certificate subject name or an empty string if none was sent */
    string &getSubjectName(string &result);

    /** @brief   Return true if the peer certificate was verified or if no certificate was presented */
    bool verifyResult();

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