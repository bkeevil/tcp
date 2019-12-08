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
        OPTS_FAILURE
    };    

    ProgramOptions() { setup(); }
    
    void setup();
    void dump();
    void showHelp();
    void showVersion();

    statusReturn_e parseOptions(int argc, char* argv[]);
    
    // General Options
    string config {};
    string host {};
    string port {};
    string log {};
    bool verbose {false};
    bool blocking {false};
    bool ip6 {false};

    // SSL Options
    string certfile {};
    string keyfile {};
    string keypass {};
    string cafile {};
    string capath {};
    bool verifypeer {false};
    bool checkhostname {false};
    bool tlsonly {false};
    bool nocompression {false};

  private:
    template<class T> void dumpOption(const char *name);    
    po::options_description general {"General Options"};
    po::options_description ssl {"SSL Options"};
};