#include <string>
#include <algorithm>
#include <netinet/ip.h>
#include "boost/program_options.hpp"

using namespace std;

namespace po = boost::program_options;

class ProgramOptions {
  public:    
    void dump();
    bool parseOptions(int argc, char** argv);
    po::variables_map vm;
    string config;
    string host;
    string port;
    string log;
    string certfile;
    string keyfile;
    string keypass;
    string cafile;
    string capath;
  protected:
    template<class T> void dumpOption(const char *name);    
    po::options_description general {"General Options"};
    po::options_description ssl {"SSL Options"};
    po::positional_options_description posopt;
};