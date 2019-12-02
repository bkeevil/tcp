#include "tcpssl.h"
#include "openssl/err.h"

namespace tcp {

using namespace std;

SSL_CTX *SSLContext::ctx {nullptr};
int SSLContext::refcount_ {0};
bool SSLContext::supportTLSOnly_ {false};

SSLContext::SSLContext(const bool supportTLSOnly)
{  
  if (ctx = nullptr) {
    SSL_load_error_strings();
    ctx = SSL_CTX_new(TLS_client_method());
    if (ctx != NULL) {
      refcount_++;
      if (supportTLSOnly) {
        SSL_CTX_set_min_proto_version(ctx,TLS1_VERSION); // Don't support SSL
      }
    }
  } else {
    SSL_CTX_up_ref(ctx);
    refcount_++;
  }    
}

SSLContext::~SSLContext()
{
  if (refcount_ > 0) {
    refcount_--;
    SSL_CTX_free(ctx);  
    if (refcount_ == 0) {
      ERR_free_strings();
    }
  }
}

} // namespace tcp