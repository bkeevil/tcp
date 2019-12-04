#include "echoclient.h"

/* Echo Client */

/** @brief Display any data received to cout */
void EchoClient::dataAvailable() {
  for (auto it : inputBuffer) {
    cout << (char)it;
  }
  inputBuffer.clear();
}