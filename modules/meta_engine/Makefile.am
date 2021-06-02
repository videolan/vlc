metadir = $(pluginsdir)/meta_engine
meta_RPATH =
meta_RPATH += -rpath '$(metadir)'

libfolder_plugin_la_SOURCES = meta_engine/folder.c
meta_LTLIBRARIES = libfolder_plugin.la

libtaglib_plugin_la_SOURCES = meta_engine/taglib.cpp \
	demux/xiph_metadata.h demux/xiph_metadata.c
libtaglib_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(TAGLIB_CFLAGS)
libtaglib_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(meta_RPATH)
libtaglib_plugin_la_LIBADD = $(TAGLIB_LIBS) -lz
EXTRA_LTLIBRARIES += libtaglib_plugin.la
meta_LTLIBRARIES += $(LTLIBtaglib)
