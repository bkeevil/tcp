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

void print_error_string(unsigned long err, const char* const label)
{
  const char* const str = ERR_reason_error_string(err);
  if(str)
    cerr << label << ": " << str << endl;
  else
    cerr << label << " failed: " << err << hex << " (0x" << err << dec << endl;
}

int printSSLErrors_cb(const char *str, size_t len, void *u)
{
  (void)u;
  for (size_t i=0;i<len;i++) {
    cerr.put(str[i]);
  }
  cerr << endl;
  return 1;
}

void printSSLErrors()
{
  ERR_print_errors_fp(stdout);
  ERR_print_errors_cb(&printSSLErrors_cb,NULL);
}

int wildcmp(const char *wild, const char *string) {
  // Written by Jack Handy - <A href="mailto:jakkhandy@hotmail.com">jakkhandy@hotmail.com</A>
  const char *cp = NULL, *mp = NULL;

  while ((*string) && (*wild != '*')) {
    if ((*wild != *string) && (*wild != '?')) {
      return 0;
    }
    wild++;
    string++;
  }

  while (*string) {
    if (*wild == '*') {
      if (!*++wild) {
        return 1;
      }
      mp = wild;
      cp = string+1;
    } else if ((*wild == *string) || (*wild == '?')) {
      wild++;
      string++;
    } else {
      wild = mp;
      string = cp++;
    }
  }

  while (*wild == '*') {
    wild++;
  }
  return !*wild;
}

/** @brief Prints the certificate common name to clog */
void print_cn_name(const char* label, X509_NAME* const name)
{
    int idx = -1, success = 0;
    unsigned char *utf8 = NULL;
    
    do {
      if(!name) break; /* failed */
      
      idx = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
      if(!(idx > -1))  break; /* failed */
      
      X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, idx);
      if(!entry) break; /* failed */
      
      ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
      if(!data) break; /* failed */
      
      int length = ASN1_STRING_to_UTF8(&utf8, data);
      if(!utf8 || !(length > 0))  break; /* failed */
      
      clog << "  " << label << ": " << utf8 << endl;

      success = 1;
      
    } while (0);
    
    if(utf8)
      OPENSSL_free(utf8);
    
    if(!success)
      clog << "  " << label << ": <not available>" << endl;
}

/** @brief Prints the certificate subject alt name to clog */
void print_san_name(const char* label, X509* const cert)
{
    int success = 0;
    GENERAL_NAMES* names = NULL;
    unsigned char* utf8 = NULL;
    
    do {
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
            cerr << "  Strlen and ASN1_STRING size do not match (embedded null?): " << len2 << " vs " << len1 << endl;
          }
          
          /* If there's a problem with string lengths, then     */
          /* we skip the candidate and move on to the next.     */
          /* Another policy would be to fails since it probably */
          /* indicates the client is under attack.              */
          if(utf8 && len1 && len2 && (len1 == len2)) {
            clog << "  " << label << ": " << utf8 << endl;
              success = 1;
          }
          
          if(utf8) {
            OPENSSL_free(utf8), utf8 = NULL;
          }
        } else {
          clog << "  Unknown GENERAL_NAME type: " << entry->type << endl;
        }
      }

    } while (0);
    
    if(names)
      GENERAL_NAMES_free(names);
    
    if(utf8)
      OPENSSL_free(utf8);
    
    if(!success)
      clog << "  " << label << ": <not available>" << endl;
    
}

/** @brief Prints the certificate details to clog */
int verify_callback(int preverify, X509_STORE_CTX* x509_ctx)
{
  int depth = X509_STORE_CTX_get_error_depth(x509_ctx);
  int err = X509_STORE_CTX_get_error(x509_ctx);
  (void)err;

  X509* cert = X509_STORE_CTX_get_current_cert(x509_ctx);
  X509_NAME* iname = cert ? X509_get_issuer_name(cert) : NULL;
  X509_NAME* sname = cert ? X509_get_subject_name(cert) : NULL;
  
  print_cn_name("Issuer (cn)", iname);
  print_cn_name("Subject (cn)", sname);
  
  if(depth == 0) {
      /* If depth is 0, its the server's certificate. Print the SANs too */
      print_san_name("Subject (san)", cert);
  }

  if (preverify) {
    clog << "Certificate verification passed" << endl;
  } else {
    clog << "Certificate verification failed" << endl;
  }

  return preverify;
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
  (void)rwflag;
  if (!keypass_.empty()) {
    int lsize = max<int>(size,keypass_.length());
    strncpy(buf,keypass_.c_str(),lsize);
    return lsize;
  } else {
    return 0;
  }
}

/* SSL */

SSL::SSL(DataSocket &owner, SSLContext &context) : owner_(owner)
{
  mode_ = context.mode_;
  ssl_ = SSL_new(context.ctx_);
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
  (void)rwflag;
  if (!keypass_.empty()) {
    int lsize = max<int>(size,keypass_.length());
    strncpy(buf,keypass_.c_str(),lsize);
    return lsize;
  } else {
    return 0;
  }
}

bool SSL::setfd(int socket)
{
  if (socket == 0) {
    cerr << "SSL::setfd: socket is NULL" << endl;
    return false;
  } else {
    int res = SSL_set_fd(ssl_,socket);
    if (res != 1) {
      unsigned long ssl_err = ERR_get_error();
      print_error_string(ssl_err,"SSL::set_fd");
      return false;
    } else {
      return true;
    }
  }
}

string &SSL::getSubjectName()
{  
  if (!subjectName_.empty()) {
    return subjectName_;
  }

  X509 *cert = SSL_get_peer_certificate(ssl_);
  if (cert) {
    X509_NAME* sname = X509_get_subject_name(cert);
    if (sname) {

      int idx = -1;
      unsigned char *utf8 = NULL;
    
      do {
        idx = X509_NAME_get_index_by_NID(sname, NID_commonName, -1);
        if(!(idx > -1))  break; /* failed */
      
        X509_NAME_ENTRY* entry = X509_NAME_get_entry(sname, idx);
        if(!entry) break; /* failed */
      
        ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
        if(!data) break; /* failed */
      
        int length = ASN1_STRING_to_UTF8(&utf8, data);
        if(!utf8 || !(length > 0))  break; /* failed */
      
        subjectName_.assign((char*)utf8);
      
      } while (0);
    
      if(utf8)
        OPENSSL_free(utf8);
    }
    X509_free(cert);
  }
  return subjectName_;
}

bool SSL::validateSubjectName(const string &subjectName, const string &hostname)
{
  return wildcmp(subjectName.c_str(),hostname.c_str());
}

bool SSL::performCertPostValidation()
{
  if (!verifyResult()) {
    cerr << "Peer certificate validation failed" << endl;
    return false;
  } else {
    
    if (!getSubjectName().empty()) {
      if (!validateSubjectName(subjectName_,hostname_)) {
        cerr << "Peer certificate subject name " << subjectName_ << " does not match host name " << hostname_ << endl;
        return false;
      }
    }
    
    requiresCertPostValidation = false;

  }

  return true;
}

bool SSL::verifyResult()
{
  return (SSL_get_verify_result(ssl_) == X509_V_OK);
}

bool SSL::connect()
{
  int res = SSL_connect(ssl_);
  if (res <= 0) {
    unsigned long ssl_err = ERR_get_error();
    switch (ssl_err) {
      case SSL_ERROR_NONE: return true;
      case SSL_ERROR_WANT_READ: wantsRead(); return connect(); break; 
      case SSL_ERROR_WANT_WRITE: wantsWrite(); return connect(); break;
      default: print_error_string(ssl_err,"SSL_connect");
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
      case SSL_ERROR_NONE: return true;
      case SSL_ERROR_WANT_READ: wantsRead(); return accept(); break; 
      case SSL_ERROR_WANT_WRITE: wantsWrite(); return accept(); break;
      default: print_error_string(ssl_err,"SSL_accept");
    }
    printSSLErrors();
    return false;
  } else {
    return true;
  }
}

size_t SSL::read(void *buffer, size_t size)
{
  if (requiresCertPostValidation && !performCertPostValidation())
    owner_.disconnected();
    
  int res = SSL_read(ssl_,buffer,size);
  if (res <= 0) {
    unsigned long ssl_err = SSL_get_error(ssl_,res);
    switch (ssl_err) {
      case SSL_ERROR_NONE: return 0;
      case SSL_ERROR_WANT_READ: return 0; break; 
      case SSL_ERROR_WANT_WRITE: wantsWrite(); return read(buffer,size); break;
      default: print_error_string(ssl_err,"SSL_read"); return 0;
    }
  } else {
    return res;
  }
}

size_t SSL::write(const void *buffer, size_t size)
{
  if (requiresCertPostValidation && !performCertPostValidation())
    owner_.disconnected();

  int res = SSL_write(ssl_,buffer,size);
  if (res <= 0) {
    unsigned long ssl_err = SSL_get_error(ssl_,res);
    switch (ssl_err) {
      case SSL_ERROR_NONE: return 0;
      case SSL_ERROR_WANT_READ: wantsRead(); return write(buffer,size); break; 
      case SSL_ERROR_WANT_WRITE: return write(buffer,size); break;
      default: print_error_string(ssl_err,"SSL_write"); return 0; break;
    }
  } else {
    return res;
  }
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

void SSL::wantsRead()
{
  owner_.mtx.lock();
  owner_.readToInputBuffer();
  owner_.mtx.unlock();
}

void SSL::wantsWrite()
{
  owner_.mtx.lock();
  owner_.sendOutputBuffer();
  owner_.mtx.unlock();
}

} // namespace tcp