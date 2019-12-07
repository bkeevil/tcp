#include "echoclient.h"

/* Echo Client */

/** @brief Display any data received to cout */
void EchoClient::dataAvailable() {
  int da = available();
  if (da > 0) {
    mtx.lock();
    char *buf = (char*)malloc(da+1);
    int dr = read(buf,da);
    buf[da] = 0;
    (void)dr;
    cout << buf;
    mtx.unlock();
  }
}