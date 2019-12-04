#include <iostream>
#include <cstring>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/conf.h>
#include <openssl/x509.h>
#include <openssl/buffer.h>
#include <openssl/x509v3.h>
#include <openssl/opensslconf.h>
#include "tcpssl.h"

namespace tcp {

using namespace std;

bool sslinitialized_ {false};

void initSSLLibrary()
{
  if (!sslinitialized_) {
    SSL_library_init();             
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ERR_load_crypto_strings();
    OpenSSL_add_all_ciphers();
    sslinitialized_ = true;
    clog << "OpenSSL library initialized" << endl;
  }
}

void freeSSLLibrary()
{
  ERR_free_strings();
}

void printSSLErrors()
{
  ERR_print_errors_fp(stderr);
}

void print_cn_name(const char* label, X509_NAME* const name)
{
    int idx = -1, success = 0;
    unsigned char *utf8 = NULL;
    
    do
    {
        if(!name) break; /* failed */
        
        idx = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
        if(!(idx > -1))  break; /* failed */
        
        X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, idx);
        if(!entry) break; /* failed */
        
        ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
        if(!data) break; /* failed */
        
        int length = ASN1_STRING_to_UTF8(&utf8, data);
        if(!utf8 || !(length > 0))  break; /* failed */
        
        fprintf(stdout, "  %s: %s\n", label, utf8);
        success = 1;
        
    } while (0);
    
    if(utf8)
        OPENSSL_free(utf8);
    
    if(!success)
        fprintf(stdout, "  %s: <not available>\n", label);
}

void print_san_name(const char* label, X509* const cert)
{
    int success = 0;
    GENERAL_NAMES* names = NULL;
    unsigned char* utf8 = NULL;
    
    do
    {
        if(!cert) break; /* failed */
        
        names = (GENERAL_NAMES*)X509_get_ext_d2i(cert, NID_subject_alt_name, 0, 0 );
        if(!names) break;
        
        int i = 0, count = sk_GENERAL_NAME_num(names);
        if(!count) break; /* failed */
        
        for( i = 0; i < count; ++i )
        {
            GENERAL_NAME* entry = sk_GENERAL_NAME_value(names, i);
            if(!entry) continue;
            
            if(GEN_DNS == entry->type)
            {
                int len1 = 0, len2 = -1;
                
                len1 = ASN1_STRING_to_UTF8(&utf8, entry->d.dNSName);
                if(utf8) {
                    len2 = (int)strlen((const char*)utf8);
                }
                
                if(len1 != len2) {
                    fprintf(stderr, "  Strlen and ASN1_STRING size do not match (embedded null?): %d vs %d\n", len2, len1);
                }
                
                /* If there's a problem with string lengths, then     */
                /* we skip the candidate and move on to the next.     */
                /* Another policy would be to fails since it probably */
                /* indicates the client is under attack.              */
                if(utf8 && len1 && len2 && (len1 == len2)) {
                    fprintf(stdout, "  %s: %s\n", label, utf8);
                    success = 1;
                }
                
                if(utf8) {
                    OPENSSL_free(utf8), utf8 = NULL;
                }
            }
            else
            {
                fprintf(stderr, "  Unknown GENERAL_NAME type: %d\n", entry->type);
            }
        }

    } while (0);
    
    if(names)
        GENERAL_NAMES_free(names);
    
    if(utf8)
        OPENSSL_free(utf8);
    
    if(!success)
        fprintf(stdout, "  %s: <not available>\n", label);
    
}

int verify_callback(int preverify, X509_STORE_CTX* x509_ctx)
{
    int depth = X509_STORE_CTX_get_error_depth(x509_ctx);
    //int err = X509_STORE_CTX_get_error(x509_ctx);
    
    X509* cert = X509_STORE_CTX_get_current_cert(x509_ctx);
    X509_NAME* iname = cert ? X509_get_issuer_name(cert) : NULL;
    X509_NAME* sname = cert ? X509_get_subject_name(cert) : NULL;
    
    print_cn_name("Issuer (cn)", iname);
    print_cn_name("Subject (cn)", sname);
    
    if(depth == 0) {
        /* If depth is 0, its the server's certificate. Print the SANs too */
        print_san_name("Subject (san)", cert);
    }

    return preverify;
}

void print_error_string(unsigned long err, const char* const label)
{
    const char* const str = ERR_reason_error_string(err);
    if(str)
        fprintf(stderr, "%s\n", str);
    else
        fprintf(stderr, "%s failed: %lu (0x%lx)\n", label, err, err);
}

int ctx_password_callback(char *buf, int size, int rwflag, void *userdata)
{
  SSLContext *ctx = (SSLContext*)userdata;
  if (ctx != NULL) {
    return ctx->passwordCallback(buf,size,rwflag);
  } else {
    return 0;
  }
}

int ssl_password_callback(char *buf, int size, int rwflag, void *userdata)
{
  SSL *ssl = (SSL*)userdata;
  if (ssl != NULL) {
    return ssl->passwordCallback(buf,size,rwflag);
  } else {
    return 0;
  }
}

SSLContext::SSLContext(SSLMode mode) : mode_(mode)
{
  initSSLLibrary();
  const SSL_METHOD *meth = (mode == SSLMode::SERVER) ? TLS_server_method() : TLS_client_method();
  unsigned long ssl_err = ERR_get_error();
  if (meth == NULL) {
    print_error_string(ssl_err, "TLS_method");
    return;
  }
  ctx_ = SSL_CTX_new(meth);
  ssl_err = ERR_get_error();
  if (ctx_ == NULL) {
    print_error_string(ssl_err, "SSL_CTX_new");
    return;
  }
}

SSLContext::~SSLContext()
{
  SSL_CTX_free(ctx_);
}

void SSLContext::setOptions(bool verifypeer, bool compression, bool tlsonly)
{
  long flags = 0;
  //long res = 1;
  //unsigned long ssl_err = 0;
    
  if (verifypeer) {
    if (mode_ == SSLMode::SERVER) {
      SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT | SSL_VERIFY_CLIENT_ONCE, verify_callback);
    } else {
      SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, verify_callback);
    }
  } else {
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, NULL);
  }
  SSL_CTX_set_verify_depth(ctx_, 4);
  
  if (tlsonly) {
    flags = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
    SSL_CTX_set_min_proto_version(ctx_,TLS1_VERSION);
  } else {
    flags = SSL_OP_ALL;
  }

  if (!compression) {
    flags |= SSL_OP_NO_COMPRESSION;
  }  

  SSL_CTX_set_options(ctx_, flags);
  
  printSSLErrors();  
}

bool SSLContext::setVerifyPaths(const char *cafile, const char *capath)
{
  long res = 1;
  unsigned long ssl_err = 0;

  if (cafile || capath) {
    res = SSL_CTX_load_verify_locations(ctx_, cafile, capath); 
    ssl_err = ERR_get_error();
    if (res == 0) {
      print_error_string(ssl_err,"SSL_CTX_load_verify_locations");
      return false;
    }
  } else {
    res = SSL_CTX_set_default_verify_paths(ctx_);
    ssl_err = ERR_get_error();
    if (res == 0) {
      print_error_string(ssl_err,"SSL_CTX_set_default_verify_paths");
      return false;
    }
  }
  
  printSSLErrors();
  return true;
}

bool SSLContext::setCertificateAndKey(const char *certfile, const char *keyfile)
{
  long res = 1;
  unsigned long ssl_err = 0;

  if ((certfile && !keyfile) || (!certfile && keyfile)) {
    cerr << "Error: Both a certificate and a private key file are required" << endl;
    return false;
  }  

  if (certfile && keyfile) {
    res = SSL_CTX_use_certificate_file(ctx_, certfile,  SSL_FILETYPE_PEM);
    ssl_err = ERR_get_error();
    if ( res != 1) {
      print_error_string(ssl_err,"SSL_CTX_use_certificate_file");
      return false;
    }

    res = SSL_CTX_use_PrivateKey_file(ctx_, keyfile, SSL_FILETYPE_PEM);
    ssl_err = ERR_get_error();
    if (res != 1) {
      print_error_string(ssl_err,"SSL_CTX_use_PrivateKey_file");
      return false;
    }

    /* Make sure the key and certificate file match. */
    res = SSL_CTX_check_private_key(ctx_);
    ssl_err = ERR_get_error();
    if ( res != 1) {
      print_error_string(ssl_err,"SSL_CTX_check_private_key");
      return false;
    }

    SSL_CTX_set_default_passwd_cb(ctx_,&ctx_password_callback);
    SSL_CTX_set_default_passwd_cb_userdata(ctx_,this);
    
    return true;

  } else {
    return false;
  }
}

int SSLContext::passwordCallback(char *buf, int size, int rwflag)
{
  if (keypass) {
    int lsize = max<int>(size,strlen(keypass));
    strncpy(buf,keypass,lsize);
    return lsize;
  } else {
    return 0;
  }
}

SSL::SSL(SSLContext &context)
{
  mode_ = context.mode();
  ssl_ = SSL_new(context.handle());
}

SSL::~SSL()
{
  SSL_free(ssl_);
}

void SSL::setOptions(bool verifypeer)
{
  if (verifypeer) {
    SSL_set_verify(ssl_, SSL_VERIFY_PEER, verify_callback);
  } else {
    SSL_set_verify(ssl_, SSL_VERIFY_NONE, NULL);
  }
  
  printSSLErrors();  
}

bool SSL::setCertificateAndKey(const char *certfile, const char *keyfile)
{
  long res = 1;
  unsigned long ssl_err = 0;

  if ((certfile && !keyfile) || (!certfile && keyfile)) {
    cerr << "Error: Both a certificate and a private key file are required" << endl;
    return false;
  }  

  if (certfile && keyfile) {
    res = SSL_use_certificate_file(ssl_, certfile,  SSL_FILETYPE_PEM);
    ssl_err = ERR_get_error();
    if ( res != 1) {
      print_error_string(ssl_err,"SSL_use_certificate_file");
      return false;
    }

    res = SSL_use_PrivateKey_file(ssl_, keyfile, SSL_FILETYPE_PEM);
    ssl_err = ERR_get_error();
    if (res != 1) {
      print_error_string(ssl_err,"SSL_use_PrivateKey_file");
      return false;
    }

    /* Make sure the key and certificate file match. */
    res = SSL_check_private_key(ssl_);
    ssl_err = ERR_get_error();
    if ( res != 1) {
      print_error_string(ssl_err,"SSL_CTX_check_private_key");
      return false;
    }

    SSL_set_default_passwd_cb(ssl_,&ssl_password_callback);
    SSL_set_default_passwd_cb_userdata(ssl_,this);
    
    return true;

  } else {
    return false;
  }
}

int SSL::passwordCallback(char *buf, int size, int rwflag)
{
  if (keypass) {
    int lsize = max<int>(size,strlen(keypass));
    strncpy(buf,keypass,lsize);
    return lsize;
  } else {
    return 0;
  }
}

bool SSL::setfd(int socket)
{
  if (socket == 0) {
    cerr << "SSLSession::setfd: socket is NULL" << endl;
    return false;
  } else {
    int res = SSL_set_fd(ssl_,socket);
    if (res != 1) {
      unsigned long ssl_err = ERR_get_error();
      print_error_string(ssl_err,"SSL_set_fd");
      return false;
    } else {
      return true;
    }
  }
}

bool SSL::connect()
{
  int res = SSL_connect(ssl_);
  if (res <= 0) {
    unsigned long ssl_err = ERR_get_error();
    switch (ssl_err) {
      case SSL_ERROR_WANT_READ: cerr << "peek wants read" << endl; break; 
      case SSL_ERROR_WANT_WRITE: cerr << "peek wants write" << endl; break;
      default: print_error_string(ssl_err,"SSL_peek");
    }
    printSSLErrors();
    return false;
  } else {
    return true;
  }
}

bool SSL::accept()
{
  int res = SSL_accept(ssl_);
  if (res <= 0) {
    unsigned long ssl_err = ERR_get_error();
    switch (ssl_err) {
      case SSL_ERROR_WANT_READ: cerr << "peek wants read" << endl; break; 
      case SSL_ERROR_WANT_WRITE: cerr << "peek wants write" << endl; break;
      default: print_error_string(ssl_err,"SSL_peek");
    }
    printSSLErrors();
    return false;
  } else {
    return true;
  }
}

int SSL::pending()
{
  return SSL_pending(ssl_);
}

int SSL::peek(void *buffer, const int size)
{
  int res = SSL_peek(ssl_,buffer,size);
  if (res <= 0) {
    unsigned long ssl_err = SSL_get_error(ssl_,res);
    switch (ssl_err) {
      case SSL_ERROR_WANT_READ: cerr << "peek wants read" << endl; break; 
      case SSL_ERROR_WANT_WRITE: cerr << "peek wants write" << endl; break;
      default: print_error_string(ssl_err,"SSL_peek");
    }
  } 
  return res;
} 

int SSL::read(void *buffer, const int size)
{
  int res = SSL_read(ssl_,buffer,size);
  if (res <= 0) {
    unsigned long ssl_err = SSL_get_error(ssl_,res);
    switch (ssl_err) {
      case SSL_ERROR_WANT_READ: cerr << "read wants read" << endl; break; 
      case SSL_ERROR_WANT_WRITE: cerr << "read wants write" << endl; break;
      default: print_error_string(ssl_err,"SSL_read");
    }
  } 
  return res;
}

int SSL::write(const void *buffer, const int size)
{
  int res = SSL_write(ssl_,buffer,size);
  if (res <= 0) {
    unsigned long ssl_err = SSL_get_error(ssl_,res);
    switch (ssl_err) {
      case SSL_ERROR_WANT_READ: cerr << "write wants read" << endl; break; 
      case SSL_ERROR_WANT_WRITE: cerr << "write wants write" << endl; break;
      default: print_error_string(ssl_err,"SSL_write");
    }
  } 
  return res;
}

void SSL::clear()
{
  int res = SSL_clear(ssl_);
  if ( res != 1) {
    unsigned long ssl_err = ERR_get_error();
    print_error_string(ssl_err,"SSL_clear");
  }
}

void SSL::shutdown()
{
  int res = SSL_shutdown(ssl_);
  if ( res != 1) {
    unsigned long ssl_err = ERR_get_error();
    print_error_string(ssl_err,"SSL_shutdown");
  }
}

} // namespace tcp