# Overview

A TCP client/server library for Linux that supports SSL. The library uses the EPoll mechanism to respond to OS events in a single thread. This library is designed to be run in a single thread.

## Documentation

I used Doxygen to embed the documentation in the code. Once Doxygen is installed on your system,
go to the repository root type `doxygen` to generate the documentation.
Documentation is generated to the docs\ directory.

The documentation is currently a bit behind as the library is being actively developed.

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
