AUTOMAKE_OPTIONS=foreign no-dependencies

x_includedir = $(pkgincludedir)/main

noinst_LIBRARIES = libmain.a

x_include_HEADERS = \
snort_debug.h \
snort_types.h \
snort.h \
snort_config.h \
thread.h

libmain_a_SOURCES = \
analyzer.cc \
analyzer.h \
build.h \
help.cc \
help.h \
modules.cc \
modules.h \
policy.cc \
policy.h \
shell.cc \
shell.h \
snort.cc \
snort.h \
snort_config.cc \
snort_config.h \
snort_debug.cc \
snort_module.cc \
snort_module.h \
thread.cc

AM_CXXFLAGS = @AM_CXXFLAGS@
