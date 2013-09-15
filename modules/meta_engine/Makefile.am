include ../common.am
metadir = $(pluginsdir)/meta_engine
EXTRA_LTLIBRARIES =

libfolder_plugin_la_SOURCES = folder.c
meta_LTLIBRARIES = libfolder_plugin.la

libtaglib_plugin_la_SOURCES = taglib.cpp ../demux/xiph_metadata.h ../demux/xiph_metadata.c
libtaglib_plugin_la_CXXFLAGS = $(AM_CFLAGS) $(TAGLIB_CFLAGS)
libtaglib_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(metadir)'
libtaglib_plugin_la_LIBADD = $(TAGLIB_LIBS) -lz
EXTRA_LTLIBRARIES += libtaglib_plugin.la
meta_LTLIBRARIES += $(LTLIBtaglib)
