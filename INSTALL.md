CNL-CPP: A Common Name Library for C++
======================================

Prerequisites
=============
(These are prerequisites to build CNL-CPP.  To do development of CNL-CPP code
and update the build system, see Development Prerequisites.)

* Required: NDN-CPP (https://github.com/named-data/ndn-cpp)
* Required: libcrypto
* Optional: libsqlite3 (for key storage in NDN-CPP)
* Optional: OSX Security framework (for key storage in NDN-CPP)
* Optional: Protobuf (for generalized objects)
* Optional: log4cxx (for debugging and log output in this and in NDN-CPP)
* Optional: Doxygen (to make documentation)
* Optional: Boost (min version 1.48) with asio (for ThreadsafeFace and async I/O in NDN-CPP)
* Optional: zlib (for FullPSync2017)

The steps to install the prerequisites are the same as to build and install NDN-CPP.
Please see https://github.com/named-data/ndn-cpp/blob/master/INSTALL.md .

[Ubuntu only] After installing NDN-CPP, be sure to update the path to the
shared libraries using `sudo /sbin/ldconfig` .

Build
=====
(These are instructions to build CNL-CPP. To do development of CNL-CPP code and update the build system, see Development.)

To build in a terminal, change directory to the CNL-CPP root. Enter:

To configure on macOS, enter:

    ./configure ADD_CFLAGS=-I/usr/local/opt/openssl/include ADD_CXXFLAGS=-I/usr/local/opt/openssl/include ADD_LDFLAGS=-L/usr/local/opt/openssl/lib

To configure on other systems, enter:

    ./configure

Enter:

    make
    sudo make install

To run the unit tests, in a terminal enter:

    make check

To make documentation, in a terminal enter:

    make doxygen-doc

The documentation output is in `doc/html/index.html`. (If you already did ./configure
before installing Doxygen, you need to do ./configure again before make doxygen-doc.)

Files
=====
This makes the following libraries:

* .libs/libcnl-cpp.a: The C++ library API.

This makes the following example programs:

* bin/test-segmented: Test updating a namespace based on segmented content.

Running make doxygen-doc puts code documentation in doc/html.

Development Prerequisites
=========================
These steps are only needed to do development of CNL-CPP code and update the build system.
First follow the Prerequisites for your platform to build NDN-CPP.
https://github.com/named-data/ndn-cpp/blob/master/INSTALL.md

## OS X 10.9, OS X 10.10.2, OS X 10.11, macOS 10.12 and macOS 10.13
In a terminal, enter:

    brew install automake libtool doxygen

## Ubuntu 12.04 (64 bit and 32 bit), Ubuntu 14.04 (64 bit and 32 bit)
In a terminal, enter:

    sudo apt-get install automake libtool doxygen

Development
===========
Follow Development Prerequisites above for your platform.
Now you can add source code files and update Makefile.am.
After updating, change directory to the CNL-CPP root and enter the following to build the Makefile:

    ./autogen.sh

To build again, follow the instructions above (./configure, make, etc.)
