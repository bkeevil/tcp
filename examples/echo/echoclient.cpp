#include "echoclient.h"

/* Echo Client */

/** @brief Display any data received to cout */
void EchoClient::dataAvailable() {
  /*char c;
  c = streambuf_.sbumpc();
  while (c != traits_type::eof()) {
    cout << c;
    c = streambuf_.sbumpc();
  } */
  cout << rdbuf() <<endl;
}