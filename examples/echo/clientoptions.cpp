#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdint.h>
#include "clientoptions.h"
#include "boost/program_options/parsers.hpp"
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/algorithm/string/trim.hpp>

void ProgramOptions::setup()
{
  command.add_options()
    ("help,h", "Produce this help message")
    ("version,v", "Display version information")
    ("config", po::value<string>(&config), "Config filename")
  ;

  general.add_options()
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
  visible_options.add(command).add(general).add(ssl);
  cout << visible_options << "\n";
}

void ProgramOptions::showVersion()
{
  cout << "Version 1.0" << endl;
}

bool ProgramOptions::validateFilename(string &filename)
{
  char tmp[2000];
  // Strip whitespaces from front/back of filename string
  boost::algorithm::trim(filename);
  realpath(filename.c_str(),tmp);
  filename = tmp;
  return boost::filesystem::is_regular_file(filename);
}

bool ProgramOptions::loadGlobalConfig(po::variables_map &vm) 
{
  po::options_description config_options;
  config_options.add(general).add(ssl);
  string config("/etc/echoclient.conf");
  if (validateFilename(config)) {
    clog << "Loading configuration options from " << config << endl;
    ifstream f(config,ifstream::in);
    if (f) {
      store(parse_config_file(f, config_options), vm);
      return true;
    } else {
      cerr << "ERROR: Could not open file " << config << " for reading" << endl;
    }
  }  
  return false;
}

bool ProgramOptions::loadUserConfig(po::variables_map &vm) 
{
  po::options_description config_options;
  config_options.add(general).add(ssl);
  string config = getenv("HOME") + string("/") + string(".echoclient");
  if (validateFilename(config)) {
    clog << "Loading configuration options from " << config << endl;
    ifstream f(config,ifstream::in);
    if (f) {
      store(parse_config_file(f, config_options), vm);
      return true;
    } else {
      cerr << "ERROR: Could not open file " << config << " for reading" << endl;
    }
  }  
  return false;
}

bool ProgramOptions::loadCommandLineConfig(po::variables_map &vm) 
{
  po::options_description config_options;
  config_options.add(general).add(ssl);
  if (validateFilename(config)) {
    clog << "Loading configuration options from " << config << endl;
    ifstream f(config,ifstream::in);
    if (f) {
      store(parse_config_file(f, config_options), vm);
      return true;
    }
  }  
  return false;
}

ProgramOptions::statusReturn_e ProgramOptions::validateOptions()
{
  if (port.empty()) {
    cerr << "ERROR: port or service name must be specified!!!" << endl;
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

  if ((certfile.empty() && !keyfile.empty()) || (!certfile.empty() && keyfile.empty())) {
    cerr << "ERROR: both a certificate and key must be specified" << endl;
    showHelp();
    return OPTS_FAILURE;
  }

  if (checkhostname && !verifypeer) {
    cerr << "ERROR: The checkhostname option requires verifypeer to be set" << endl;
    showHelp();
    return OPTS_FAILURE;
  }

  return OPTS_SUCCESS;  
}

ProgramOptions::statusReturn_e ProgramOptions::parseOptions(int argc, char* argv[])
{  
  try {
    po::variables_map vm;
    po::options_description command_line_options;
    command_line_options.add(command).add(general).add(ssl);
    po::store(po::parse_command_line(argc, argv, command_line_options), vm);
    po::notify(vm);    
    
    if (vm.count("help")) {
      showHelp();
      return OPTS_HELP;
    }
    
    if (vm.count("version")) {
      showVersion();
      return OPTS_VERSION;
    }

    loadGlobalConfig(vm);
    loadUserConfig(vm);
    
    if (vm.count("config")) {
      loadCommandLineConfig(vm);
    }

    //po::options_description command_line_options;
    //command_line_options.add(general).add(ssl);  
    po::store(po::parse_command_line(argc, argv, command_line_options), vm);
    po::notify(vm);    
  } catch (std::exception &e) {
    cerr << "ERROR - parsing error: " << e.what() << endl;
    return OPTS_FAILURE;
  } catch( ... ) {
    cerr << "ERROR - parsing error (unknown type)." << endl;
    return OPTS_FAILURE;
   }
  return validateOptions();
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