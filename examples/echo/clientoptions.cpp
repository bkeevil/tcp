#include <iostream>
#include <fstream>
#include <stdint.h>
#include "clientoptions.h"
#include "boost/program_options/parsers.hpp"

void ProgramOptions::setup()
{
  general.add_options()
    ("help,h", "Produce this help message")
    ("version,v", "Display version information")
    ("config", po::value<string>(&config), "Config filename")
    ("host,H", po::value<string>(&host), "host name or ip address")
    ("port,P", po::value<string>(&port), "Port or service name")
    ("ip6", po::bool_switch(&ip6), "Prefer IPv6")
    ("blocking,b", po::bool_switch(&blocking), "Use a blocking socket")
    ("log,l", po::value<string>(&log), "Log filename")
    ("verbose,V", po::bool_switch(&verbose), "Verbose logging")
  ;

  ssl.add_options()
    ("certfile", po::value<string>(&certfile), "Certificate file (PEM)")
    ("keyfile", po::value<string>(&keyfile), "Private key file (PEM)")
    ("keypass", po::value<string>(&keypass), "Private key password")
    ("cafile", po::value<string>(&cafile), "Certificate authority file (PEM)")
    ("capath", po::value<string>(&capath), "Certificate authority path (PEM)")
    ("verifypeer", po::bool_switch(&verifypeer)->default_value(false), "Verify server certificate signature")
    ("checkhostname", po::bool_switch(&checkhostname)->default_value(false), "Check host name against certificate")
    ("tlsonly", po::bool_switch(&tlsonly)->default_value(false), "Don't allow deprecated protocols")
    ("nocompression", po::bool_switch(&nocompression)->default_value(false), "Disable TLS compression")
  ;
}

void ProgramOptions::showHelp()
{
  cout << "Useage: echoclient [options]" << endl;
  po::options_description visible_options;
  visible_options.add(general).add(ssl);
  cout << visible_options << "\n";
}

void ProgramOptions::showVersion()
{
  cout << "Version 1.0" << endl;
}

ProgramOptions::statusReturn_e ProgramOptions::parseOptions(int argc, char* argv[])
{  
  po::variables_map vm;
  po::options_description cmdline_options;
  cmdline_options.add(general).add(ssl);
  po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
  po::notify(vm);    
  
  if (vm.count("help")) {
    showHelp();
    return OPTS_HELP;
  }
  
  if (vm.count("version")) {
    showVersion();
    return OPTS_VERSION;
  }

  if (vm.count("config")) {
    if (verbose) clog << "Using config file: " << config << endl;
    
    po::options_description config_options;
    config_options.add(general).add(ssl);
    ifstream f(config,ifstream::in);
    if (f) {
      store(parse_config_file(f, config_options), vm);
    } else {
      cerr << "config file " << config << " not found" << endl;
    }
  }  

  if( !( 0 < vm.count("port"))) {
    cerr << "ERROR: port must be specified!!!" << std::endl;
    showHelp();
    return OPTS_FAILURE;
  }
  
  if (host.empty()) {
    if (ip6) {
      host.assign("::");
    } else {
      host.assign("localhost");
    }
  }

  return OPTS_SUCCESS;
}

void ProgramOptions::dump()
{
  cout << "config=" << config << endl;
  cout << "host=" << host << endl;
  cout << "port=" << port << endl;
  cout << "blocking=" << blocking << endl;
  cout << "ip6=" << ip6 << endl;
  cout << "log=" << log << endl;
  cout << "verbose=" << verbose << endl;
  cout << "certfile=" << certfile << endl;
  cout << "keyfile=" << keyfile << endl;
  cout << "keypass=" << keypass << endl;
  cout << "cafile=" << cafile << endl;
  cout << "capath=" << capath << endl;
  cout << "verifypeer=" << verifypeer << endl;
  cout << "checkhostname=" << checkhostname << endl;
  cout << "tlsonly=" << tlsonly << endl;
  cout << "nocompression=" << nocompression << endl;
}