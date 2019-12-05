# Overview

A TCP client/server library for Linux that supports SSL. The library uses the EPoll mechanism to respond to OS events in a single thread. This library is designed to be run in a single thread.

## Documentation

See the docs directory for html docs or go here:

https://bkeevil.github.io/tcp/

If you have Doxygen set up on your machine, go to the repository root type `doxygen` to generate fresh documentation.

The documentation in the repository is currently a bit behind because the library is being actively developed.

## Installation

``` bash
git clone https://github.com/bkeevil/tcp.git
cd tcp
mkdir build
cd build
cmake ..
make
```

## Testing

To run CTest unit testing:
(this is broken)

```
cd build
ctest -v
```
