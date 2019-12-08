# Overview

![](https://github.com/bkeevil/tcp/workflows/C%2FC++%20Ubuntu%20Build/badge.svg)

A TCP client/server library for Linux that supports SSL. The library uses the EPoll mechanism to respond to OS events in a single thread.

Under active development

## Documentation

See the docs directory for html docs or go here:

https://bkeevil.github.io/tcp/

If you have Doxygen set up on your machine, go to the repository root type `doxygen` to generate fresh documentation.

## Build Instructions

``` bash
git clone https://github.com/bkeevil/tcp.git
cd tcp
mkdir build
cd build
cmake ..
make
```
