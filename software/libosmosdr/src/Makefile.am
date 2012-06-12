# This is _NOT_ the library release version, it's an API version.
# Please read Chapter 6 "Library interface versions" of the libtool documentation before making any modification
LIBVERSION=0:0:0

INCLUDES = $(all_includes) -I$(top_srcdir)/include
AM_CFLAGS = ${CFLAGS} -fPIC ${SYMBOL_VISIBILITY}

lib_LTLIBRARIES = libosmosdr.la

libosmosdr_la_SOURCES = libosmosdr.c
libosmosdr_la_LDFLAGS = -version-info $(LIBVERSION)

bin_PROGRAMS         = osmo_sdr

osmo_sdr_SOURCES     = osmo_sdr.c
osmo_sdr_LDADD       = libosmosdr.la
