include aminclude.am

## Place generated object files (.o) into the same directory as their source
## files, in order to avoid collisions when non-recursive make is used.
AUTOMAKE_OPTIONS = subdir-objects

## Additional flags to pass to aclocal when it is invoked automatically at
## make time. The ${ACLOCAL_FLAGS} variable is picked up from the environment
## to provide a way for the user to supply additional arguments.
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

AM_CPPFLAGS=${BOOST_CPPFLAGS} ${PTHREAD_CFLAGS}
AM_LDFLAGS=${BOOST_LDFLAGS} ${BOOST_SYSTEM_LIB} ${BOOST_REGEX_LIB} ${BOOST_ASIO_LIB} ${PTHREAD_LIBS}

# "make install" will also install the include headers.
SUBDIRS = include

lib_LTLIBRARIES = libcnl-cpp.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcnl-cpp.pc

noinst_PROGRAMS = bin/test-generalized-object-consumer \
  bin/test-generalized-object-producer bin/test-generalized-object-stream-consumer \
  bin/test-generalized-object-stream-producer bin/test-nac-consumer \
  bin/test-nac-producer bin/test-segmented bin/test-sync \
  bin/test-versioned-generalized-object-consumer \
  bin/test-versioned-generalized-object-producer

# Public C++ headers.
# NOTE: If a new directory is added, then add it to cnl_cpp_cpp_headers in include/Makefile.am.
cnl_cpp_cpp_headers = \
  include/cnl-cpp/blob-object.hpp \
  include/cnl-cpp/object.hpp \
  include/cnl-cpp/namespace.hpp \
  include/cnl-cpp/segment-stream-handler.hpp \
  include/cnl-cpp/segmented-object-handler.hpp \
  include/cnl-cpp/generalized-object/content-meta-info-object.hpp \
  include/cnl-cpp/generalized-object/generalized-object-handler.hpp \
  include/cnl-cpp/generalized-object/generalized-object-stream-handler.hpp

# C++ code.
libcnl_cpp_la_SOURCES = ${cnl_cpp_cpp_headers} \
  src/object.cpp \
  src/namespace.cpp \
  src/segment-stream-handler.cpp \
  src/segmented-object-handler.cpp \
  src//generalized-object/generalized-object-handler.cpp \
  src//generalized-object/generalized-object-stream-handler.cpp \
  src/impl/pending-incoming-interest-table.cpp \
  src/impl/pending-incoming-interest-table.hpp

bin_test_generalized_object_consumer_SOURCES = examples/test-generalized-object-consumer.cpp
bin_test_generalized_object_consumer_LDADD = libcnl-cpp.la

bin_test_generalized_object_producer_SOURCES = examples/test-generalized-object-producer.cpp
bin_test_generalized_object_producer_LDADD = libcnl-cpp.la

bin_test_generalized_object_stream_consumer_SOURCES = examples/test-generalized-object-stream-consumer.cpp
bin_test_generalized_object_stream_consumer_LDADD = libcnl-cpp.la

bin_test_generalized_object_stream_producer_SOURCES = examples/test-generalized-object-stream-producer.cpp
bin_test_generalized_object_stream_producer_LDADD = libcnl-cpp.la

bin_test_nac_consumer_SOURCES = examples/test-nac-consumer.cpp
bin_test_nac_consumer_LDADD = libcnl-cpp.la

bin_test_nac_producer_SOURCES = examples/test-nac-producer.cpp
bin_test_nac_producer_LDADD = libcnl-cpp.la

bin_test_segmented_SOURCES = examples/test-segmented.cpp
bin_test_segmented_LDADD = libcnl-cpp.la

bin_test_sync_SOURCES = examples/test-sync.cpp
bin_test_sync_LDADD = libcnl-cpp.la

bin_test_versioned_generalized_object_consumer_SOURCES = examples/test-versioned-generalized-object-consumer.cpp
bin_test_versioned_generalized_object_consumer_LDADD = libcnl-cpp.la

bin_test_versioned_generalized_object_producer_SOURCES = examples/test-versioned-generalized-object-producer.cpp
bin_test_versioned_generalized_object_producer_LDADD = libcnl-cpp.la

dist_noinst_SCRIPTS = autogen.sh
