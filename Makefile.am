include aminclude.am

## Place generated object files (.o) into the same directory as their source
## files, in order to avoid collisions when non-recursive make is used.
AUTOMAKE_OPTIONS = subdir-objects

## Additional flags to pass to aclocal when it is invoked automatically at
## make time. The ${ACLOCAL_FLAGS} variable is picked up from the environment
## to provide a way for the user to supply additional arguments.
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

AM_CPPFLAGS=${BOOST_CPPFLAGS}
AM_LDFLAGS=${BOOST_SYSTEM_LIB} ${BOOST_REGEX_LIB} ${BOOST_ASIO_LIB}

# "make install" will also install the include headers.
SUBDIRS = include

lib_LTLIBRARIES = libcnl-cpp.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcnl-cpp.pc

noinst_PROGRAMS = bin/test-segmented

# Public C++ headers.
# NOTE: If a new directory is added, then add it to cnl_cpp_cpp_headers in include/Makefile.am.
cnl_cpp_cpp_headers = \
  include/cnl-cpp/namespace.hpp \
  include/cnl-cpp/segment-stream.hpp \
  include/cnl-cpp/segmented-content.hpp

# C++ code.
libcnl_cpp_la_SOURCES = ${cnl_cpp_cpp_headers} \
  src/namespace.cpp \
  src/segment-stream.cpp \
  src/segmented-content.cpp

bin_test_segmented_SOURCES = examples/test-segmented.cpp
bin_test_segmented_LDADD = libcnl-cpp.la

dist_noinst_SCRIPTS = autogen.sh