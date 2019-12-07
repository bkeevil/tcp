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
    /** @brief   Constructor
     *  @param   owner    [in]  The tcp::Session or tcp::Client this SSL object is for
     *  @param   context  [in]  The context object used as defaults for this SSL connection
     */
    SSL(DataSocket &owner, SSLContext &context);
    
    /** @brief   Destructor */
    virtual ~SSL();
    
    /** @brief   Sets default options for all SSL objects created from this context 
     *  @details Additional options can be set in the SSLContext object associated with this SSL object
     *  @param   verifypeer  [in]  If true, the server/client will validate the peer certificate
     *  @remarks Logs error information to cerr but does not return error information.
     */
    void setOptions(bool verifypeer = false);

    /** @brief   Sets the certificate filename and key filename
     *  @details Sets the certificate and key file for the SSL connection
     *           To set this property for all client or server connections, see the corresponding method 
     *           in the SSLContext class
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

    /** @brief   Sets the socket file descripter for this SSL object
     *  @details An application must set the file descriptor for the socket prior to calling accept() or connect()
     *  @param   socket   [in]  The linux socket handle
     */    
    bool setfd(int socket);

    /** @brief   Returns the peer certificate subject name or an empty string if none was sent */
    string &getSubjectName();

    /** @brief   Return true if the peer certificate was verified or if no certificate was presented */
    bool verifyResult();

    /** @brief   Starts the SSL client handshake sequence */
    bool connect();

    /** @brief   Starts the SSL server handshake sequence */
    bool accept();

    /** @brief   Reads and decrypts SSL socket data
     *  @param   buffer  [in]  Where to place the read data
     *  @param   size    [in]  The size of buffer
     *  @returns The number of bytes actually read
     *  @remarks Logs error messages to cerr
     */
    size_t read(void *buffer, size_t size);
    
    /** @brief   Encrypts and writes SSL socket data
     *  @param   buffer  [in]  Where to place the read data
     *  @param   size    [in]  The size of buffer
     *  @returns The number of bytes actually read
     *  @remarks Logs error messages to cerr
     */
    size_t write(const void *buffer, size_t size);

    /** @brief   Resets the SSL object for another connection
     *  @details This method could be used by a client reconnecting to the same server
     */
    void clear();
    
    /** @brief   Closes the SSL connection gracefully */
    void shutdown();

    /** @brief   If True, the certificate will be checked for validity on the first read/write operation */
    bool requiresCertPostValidation { false };
     
    /** @brief   A client/server may store the internal hostname property for certificate post validation */
    void setHostname(const string value) { hostname_ = value; }

  protected:

    /** @brief   Performs a post handshake validation of the peer certificate
     *  @details The post validation is performed on the first read or write operation
     *           If the post validation fails the session is disconnected.
     *           This check is only performed if requiresCertPostValidation is true
     */
    bool performCertPostValidation();

    /** @brief   Validate the peer certificate subject name
     *  @details This function should return true if the subject name is considered valid. 
     *           It is intended to verify that the hostname matches the certificate subjectname.
     *           The default implementation uses whte wildcmp function to perform a wildcard compare.
     *           Only called if checkPeerSubjectName is true.  */
    virtual bool validateSubjectName(const string &subjectName, const string &hostName);

    DataSocket &owner_;  /**< A reference to the socket        */
    SSLMode mode_;       /**< Either SERVER or CLIENT          */
    ::SSL *ssl_;         /**< The openSSL handle for API calls */
    friend class SSLContext;
  private:
    void wantsRead();    /**< Responds to a wantsRead message from SSL_write()  */
    void wantsWrite();   /**< Responds to a wantsWrite message from SSL_read()  */
    string subjectName_;
    string hostname_;
    string keypass_;
};

/** @brief   Wildcard compare function 
 *  @details This compare function performs comparisons against a string using the * and ? wildcard characters.
 *  @remarks Used internally to validate that hostName matches a certificate subjectName.
 *           Exposed because it might be useful elsewhere in a program.
 */
int wildcmp(const char *wild, const char *string);

} // namespace

#endif