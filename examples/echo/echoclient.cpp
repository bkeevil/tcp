#include "echoclient.h"

/* Echo Client */

/** @brief Display any data received to cout */
void EchoClient::dataAvailable() {
  int da = available();
  char *buf = (char*)malloc(da);
  read(buf,da);
  cout.write(buf,da);
}