#include "echoclient.h"

/* Echo Client */

/** @brief Display any data received to cout */
void EchoClient::dataAvailable() {
  char c[255];
  memset(c,0,255);
  getline(c,255);
  cout.write(c,255);
  cout << endl;
  cout.flush();
}