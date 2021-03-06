# Overview

![](https://github.com/bkeevil/tcp/workflows/C%2FC++%20Ubuntu%20Build/badge.svg)

A TCP client/server library for Linux:

- Supports SSL using the openSSL library
- Supports IP6
- Thread safe
- Uses the Linux EPoll mechanism to respond to OS events in a single thread.
- Demo programs `echo server` and `echo client` can be used as a template to create simple TCP client/server applications

This library is currently under active development.

## Documentation

See the docs directory for html docs or go here:

https://bkeevil.github.io/tcp/

If you have Doxygen set up on your machine, go to the repository root type `doxygen` to generate fresh documentation.

## Build Instructions

Install build environment and library dependencies.

``` bash 
apt-get install -y build-essential cmake doxygen libboost-all-dev libssl-dev 
```

The boost library is only required to build the example programs, which use the system, filesystem and program_options libraries.

Use cmake to configure and build the library and example programs

``` bash
git clone https://github.com/bkeevil/tcp.git
cd tcp
mkdir build
cd build
cmake ..
make all
```

You can also `make tcp` to build just the library