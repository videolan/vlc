stream_extractordir = $(pluginsdir)/stream_extractor
stream_extractor_LTLIBRARIES =

libarchive_plugin_la_SOURCES = stream_extractor/archive.c
libarchive_plugin_la_CFLAGS = $(AM_CFLAGS) $(ARCHIVE_CFLAGS)
libarchive_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(stream_extractordir)'
libarchive_plugin_la_LIBADD = $(ARCHIVE_LIBS)
EXTRA_LTLIBRARIES += libarchive_plugin.la
stream_extractor_LTLIBRARIES += $(LTLIBarchive)
