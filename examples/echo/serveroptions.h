#ifndef SERVER_OPTIONS_H
#define SERVER_OPTIONS_H

#include <string>
#include <algorithm>
#include <netinet/ip.h>
#include "boost/program_options.hpp"

using namespace std;

namespace po = boost::program_options;

class ProgramOptions {
  public:    
    enum statusReturn_e {
        OPTS_SUCCESS,
        OPTS_VERSION,
        OPTS_HELP,
        OPTS_INTERFACES,
        OPTS_FAILURE
    };    

    ProgramOptions() { setup(); }
    
    void setup();
    void dump();
    void showHelp();
    void showVersion();
    bool validateFilename(string &filename);
    bool loadGlobalConfig(po::variables_map &vm);
    bool loadUserConfig(po::variables_map &vm);
    bool loadCommandLineConfig(po::variables_map &vm);
    statusReturn_e validateOptions();
    statusReturn_e parseOptions(int argc, char* argv[]);
    
    // General Options
    string config {};
    string interface {};
    uint16_t port {};
    string log {};
    bool verbose {false};
    bool ip6 {false};
    
    // SSL Options
    string certfile {};
    string keyfile {};
    string keypass {};
    string cafile {};
    string capath {};
    bool verifypeer {false};
    bool tlsonly {false};
    bool nocompression {false};

  private:
    template<class T> void dumpOption(const char *name);    
    po::options_description command {"Command Line Options"};
    po::options_description general {"General Options"};
    po::options_description ssl {"SSL Options"};
};

#endif // include guard