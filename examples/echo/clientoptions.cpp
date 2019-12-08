#include <iostream>
#include <fstream>
#include <stdint.h>
#include "clientoptions.h"
#include "boost/program_options/parsers.hpp"

bool ProgramOptions::parseOptions(int argc, char** argv)
{  
  general.add_options()
    ("help,h", po::bool_switch(), "Produce this help message")
    ("config", po::value<string>(&config), "Config filename")
    ("host,H", po::value<string>(&host), "host name or ip address")
    ("port,P", po::value<string>(&port), "Port or service name")
    ("ip6", po::bool_switch(), "Prefer IPv6")
    ("blocking,b", po::bool_switch(), "Use a blocking socket")
    ("log,l", po::value<string>(&log), "Log filename")
    ("verbose,V", po::bool_switch(), "Verbose logging")
  ;

  ssl.add_options()
    ("certfile", po::value<string>(&certfile), "Certificate file (PEM)")
    ("keyfile", po::value<string>(&keyfile), "Private key file (PEM)")
    ("keypass", po::value<string>(&keypass), "Private key password")
    ("cafile", po::value<string>(&cafile), "Certificate authority file (PEM)")
    ("capath", po::value<string>(&capath), "Certificate authority path (PEM)")
    ("verifypeer", po::bool_switch(), "Verify server certificate signature")
    ("checkhostname", po::bool_switch(), "Check host name against certificate")
    ("tlsonly", po::bool_switch(), "Don't allow deprecated protocols")
    ("nocompression", po::bool_switch(), "Disable TLS compression")
  ;

  po::options_description cmdline_options;
  cmdline_options.add(general).add(ssl);
  po::store(po::command_line_parser(argc, argv).options(cmdline_options).run(), vm);
  po::notify(vm);    
  
  if (vm["help"].as<bool>()) {
    cout << "Useage: echoclient [options]" << endl;
    po::options_description visible_options;
    visible_options.add(general).add(ssl);
    cout << visible_options << "\n";
    return false;
  } else {
    if (vm.count("config")) {
      const string config = vm["config"].as<string>();
      clog << "Using config file: " << config << endl;
      po::options_description config_options;
      config_options.add(general).add(ssl);
      ifstream f(config,ifstream::in);
      if (f) {
        store(parse_config_file(f, config_options), vm);
      } else {
        clog << "config file " << config << " not found" << endl;
      }
    }  
    return true;
  }

}

template<class T> void ProgramOptions::dumpOption(const char *name)
{
  const po::variable_value &v = vm[name];
  if (!v.empty()) {
    cout << name << "=" << v.as<T>() << endl;
  }
}

void ProgramOptions::dump()
{
  dumpOption<string>("config");
  dumpOption<string>("host");
  dumpOption<string>("port");
  dumpOption<bool>("blocking");
  dumpOption<bool>("ip6");
  dumpOption<string>("log");
  dumpOption<bool>("verbose");
  dumpOption<string>("certfile");
  dumpOption<string>("keyfile");
  dumpOption<string>("keypass");
  dumpOption<string>("keypass");
  dumpOption<string>("cafile");
  dumpOption<string>("capath");
  dumpOption<bool>("verifypeer");
  dumpOption<bool>("checkhostname");
  dumpOption<bool>("tlsonly");
  dumpOption<bool>("nocompression");
}