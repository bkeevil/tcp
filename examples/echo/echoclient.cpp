#include "echoclient.h"

/* Echo Client */

/** @brief Display any data received to cout */
void EchoClient::dataAvailable() {
  int da = available();
  if (da > 0) {
    char *buf = (char*)malloc(da);
    da = read(buf,da);
    if (da > 0) {
      cout.write(buf,da);
    }
    free(buf);
  }
}