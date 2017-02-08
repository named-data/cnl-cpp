# "make install" will install these in the install include directory, for example
# /usr/local/cnl-cpp/namespace.hpp.
# These files are included by the application with, for example
# #include <cnl-cpp/namspace.hpp>
# Internal include headers are not put in this public install directory.

# Public C++ headers.
# Use cnl-cpp/*.hpp instead of *.* to not overwrite *.h from above.
cnl_cpp_cpp_headers = \
  $(wildcard cnl-cpp/*.hpp)

nobase_include_HEADERS = $(cnl_cpp_cpp_headers)
