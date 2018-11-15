# To build using autotools, see the Development section of the file INSTALL.

AC_PREREQ([2.59])
AC_INIT([cnl-cpp], [0.1], [ndn-lib@lists.cs.ucla.edu], [cnl-cpp], [https://github.com/named-data/cnl-cpp])
AM_INIT_AUTOMAKE([1.10 -Wall no-define foreign])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([configure.ac])
# Create config.h without prefixes.  Put it in the include directory so CFLAGS will have -I./include .
AC_CONFIG_HEADERS(include/config.h)
# Add prefixes and output to a unique file name which will go into the cnl-cpp include install directory.
AX_PREFIX_CONFIG_H(include/cnl-cpp/cnl-cpp-config.h)
AM_MAINTAINER_MODE
AM_PROG_AR
AC_PROG_LIBTOOL
LT_PREREQ([2.2])
LT_INIT()
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_OBJCXX
AC_LANG([C++])

# Special case.
case "$host" in
  *-darwin* | *-macos10*)
     if test -d /opt/local ; then
       CFLAGS="$CFLAGS -I/opt/local/include"
       CXXFLAGS="$CXXFLAGS -I/opt/local/include"
       CPPFLAGS="$CPPFLAGS -I/opt/local/include"
       LDFLAGS="$LDFLAGS -L/opt/local/lib"
     elif test -d /sw ; then
       CFLAGS="$CFLAGS -I/sw/include"
       CXXFLAGS="$CXXFLAGS -I/sw/include"
       CPPFLAGS="$CPPFLAGS -I/sw/include"
       LDFLAGS="$LDFLAGS -L/sw/lib"
     fi
  ;;
esac

AC_ARG_VAR(ADD_CFLAGS, [Append this to CFLAGS])
CFLAGS="$CFLAGS $ADD_CFLAGS"
AC_ARG_VAR(ADD_CXXFLAGS, [Append this to CXXFLAGS])
CXXFLAGS="$CXXFLAGS $ADD_CXXFLAGS"
AC_ARG_VAR(ADD_CPPFLAGS, [Append this to CPPFLAGS])
CPPFLAGS="$CPPFLAGS $ADD_CPPFLAGS"
AC_ARG_VAR(ADD_LDFLAGS, [Append this to LDFLAGS])
LDFLAGS="$LDFLAGS $ADD_LDFLAGS"

AC_TYPE_SIZE_T
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_CHECK_FUNCS([memcmp] [memcpy] [memset])
AC_CHECK_FUNCS([strchr], :, AC_MSG_ERROR([*** strchr not found. Check 'config.log' for more details.]))
AC_CHECK_FUNCS([sscanf], :, AC_MSG_ERROR([*** sscanf not found. Check 'config.log' for more details.]))
AC_CHECK_HEADERS([time.h], :, AC_MSG_ERROR([*** time.h not found. Check 'config.log' for more details.]))
AC_CHECK_HEADERS([sys/time.h], :, AC_MSG_ERROR([*** sys/time.h not found. Check 'config.log' for more details.]))
AC_CHECK_FUNCS([gettimeofday], :, AC_MSG_ERROR([*** gettimeofday not found. processEvents requires it. Check 'config.log' for more details.]))
AC_CHECK_FUNCS([round])
AX_CXX_COMPILE_STDCXX_11(, optional)
AC_C_INLINE

# Use the minimum boost version from NFD.
AX_BOOST_BASE([1.48])

AX_BOOST_ASIO

# Require libcrypto.
AC_CHECK_LIB([crypto], [EVP_EncryptInit], [],
             [AC_MSG_FAILURE([can't find openssl crypto lib])])

# Require libndn-cpp.
AC_CHECK_LIB([ndn-cpp], [ndn_getErrorString], [],
             [AC_MSG_FAILURE([can't find the ndn-cpp library])])

# Require libndn-cpp-tools.
AC_CHECK_LIB([ndn-cpp-tools], [ndntools_getGeneralizedContentMetaComponent], [],
             [AC_MSG_FAILURE([can't find the ndn-cpp-tools library])])

# Conditionally use libsqlite3.  AX_LIB_SQLITE3 defines HAVE_SQLITE3 in confdefs.h .
AX_LIB_SQLITE3()
if grep -q "#define HAVE_SQLITE3" confdefs.h ; then
  AC_CHECK_LIB([sqlite3], [sqlite3_open], [],
               [AC_MSG_FAILURE([have sqlite3 headers but no libsqlite3])])
fi

# Conditionally use the OSX Security framework .
AC_MSG_CHECKING([for OSX Security framework])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <CoreFoundation/CoreFoundation.h>]]
    [[#include <Security/Security.h>]]
    [[#include <CoreServices/CoreServices.h>]]
    [[void test() { SecKeychainRef x; }]])
], [
    AC_MSG_RESULT([yes])
    HAVE_OSX_SECURITY=1
], [
    AC_MSG_RESULT([no])
    HAVE_OSX_SECURITY=0
])
if test "$HAVE_OSX_SECURITY" == "1" ; then
  LIBS="$LIBS -framework Security -framework Cocoa"
fi

AC_MSG_CHECKING([for log4cxx])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <log4cxx/logger.h>]]
    [[log4cxx::LoggerPtr staticModuleLogger;]])
], [
    AC_MSG_RESULT([yes])
    HAVE_LOG4CXX=1
], [
    AC_MSG_RESULT([no])
    HAVE_LOG4CXX=0
])
if test "$HAVE_LOG4CXX" == "1" ; then
  AC_CHECK_LIB([log4cxx], [main], [],
               [AC_MSG_FAILURE([have log4cxx headers but no liblog4cxx])])
fi

AC_MSG_CHECKING([for Protobuf])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <google/protobuf/message.h>]]
    [[google::protobuf::Message *message;]])
], [
    AC_MSG_RESULT([yes])
    HAVE_PROTOBUF=1
], [
    AC_MSG_RESULT([no])
    HAVE_PROTOBUF=0
])
AC_DEFINE_UNQUOTED([HAVE_PROTOBUF], $HAVE_PROTOBUF, [1 if have Google Protobuf.])
if test "$HAVE_PROTOBUF" == "1" ; then
  AC_CHECK_LIB([protobuf], [main], [],
               [AC_MSG_FAILURE([have protobuf headers but no libprotobuf])])
  # libprotobuf needs libpthread.
  AC_CHECK_LIB([pthread], [main], [],
               [AC_MSG_FAILURE([have libprotobuf but no libpthread])])

  # See https://stackoverflow.com/questions/13939904/how-to-use-protocol-buffers-with-autoconf-automake
  AC_ARG_WITH([protoc],
      [AS_HELP_STRING([--with-protoc=/path/of/protoc],
          [Location of the protocol buffers compiler protoc. Defaults to looking on path.])],
      [PROTOC="$withval"],
      [ AS_IF([test "x${PROTOC}" == "x"],
          [AC_PATH_PROG([PROTOC], [protoc], [no])])
      ]
  )
  AS_IF([test "${PROTOC}" == "no"], [AC_MSG_ERROR([ProtoBuf compiler "protoc" not found.])])
fi

DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN([$PACKAGE_NAME],[Doxyfile])

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 libcnl-cpp.pc])
AC_OUTPUT
