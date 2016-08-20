splitterdir = $(pluginsdir)/video_splitter

splitter_LTLIBRARIES = \
	libclone_plugin.la \
	libwall_plugin.la

libclone_plugin_la_SOURCES = video_splitter/clone.c

libwall_plugin_la_SOURCES = video_splitter/wall.c

libpanoramix_plugin_la_SOURCES = video_splitter/panoramix.c
libpanoramix_plugin_la_CFLAGS = $(AM_CFLAGS)
libpanoramix_plugin_la_LIBADD = $(LIBM)
if HAVE_WIN32_DESKTOP
splitter_LTLIBRARIES += libpanoramix_plugin.la
else
if HAVE_XCB_RANDR
libpanoramix_plugin_la_CFLAGS += $(XCB_RANDR_CFLAGS) $(XCB_CFLAGS)
libpanoramix_plugin_la_LIBADD += $(XCB_RANDR_LIBS) $(XCB_LIBS)
splitter_LTLIBRARIES += libpanoramix_plugin.la
endif
endif
