stream_extractordir = $(pluginsdir)/stream_extractor
stream_extractor_LTLIBRARIES =
stream_extractor_RPATH =
stream_extractor_RPATH += -rpath '$(stream_extractordir)'

libarchive_plugin_la_SOURCES = stream_extractor/archive.c
libarchive_plugin_la_CFLAGS = $(AM_CFLAGS) $(ARCHIVE_CFLAGS)
libarchive_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(stream_extractor_RPATH)
libarchive_plugin_la_LIBADD = $(ARCHIVE_LIBS)
EXTRA_LTLIBRARIES += libarchive_plugin.la
stream_extractor_LTLIBRARIES += $(LTLIBarchive)
