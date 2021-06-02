metadir = $(pluginsdir)/meta_engine
meta_PLUGINS =
meta_LTLIBRARIES =
meta_RPATH =
if HAVE_PARTIAL_LINKING
noinst_LTLIBRARIES += $(meta_PLUGINS)
else
meta_LTLIBRARIES += $(meta_PLUGINS)
meta_RPATH += -rpath '$(metadir)'
endif

libfolder_plugin_la_SOURCES = meta_engine/folder.c
meta_PLUGINS += libfolder_plugin.la

libtaglib_plugin_la_SOURCES = meta_engine/taglib.cpp \
			      meta_engine/ID3Pictures.h
libtaglib_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(TAGLIB_CFLAGS)
libtaglib_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(meta_RPATH)
libtaglib_plugin_la_LIBADD = $(TAGLIB_LIBS) -lz
EXTRA_LTLIBRARIES += libtaglib_plugin.la
meta_PLUGINS += $(LTLIBtaglib)
