stream_filterdir = $(pluginsdir)/stream_filter

stream_filter_LTLIBRARIES =

libcache_read_plugin_la_SOURCES = stream_filter/cache_read.c
stream_filter_LTLIBRARIES += libcache_read_plugin.la

libcache_block_plugin_la_SOURCES = stream_filter/cache_block.c
stream_filter_LTLIBRARIES += libcache_block_plugin.la

libdecomp_plugin_la_SOURCES = stream_filter/decomp.c
if !HAVE_WIN32
if !HAVE_TVOS
stream_filter_LTLIBRARIES += libdecomp_plugin.la
endif
endif

libinflate_plugin_la_SOURCES = stream_filter/inflate.c
libinflate_plugin_la_LIBADD = -lz
if HAVE_ZLIB
stream_filter_LTLIBRARIES += libinflate_plugin.la
endif

libprefetch_plugin_la_SOURCES = stream_filter/prefetch.c
if !HAVE_WINSTORE
stream_filter_LTLIBRARIES += libprefetch_plugin.la
endif

libhds_plugin_la_SOURCES = \
    stream_filter/hds/hds.c

libhds_plugin_la_CFLAGS = $(AM_CFLAGS)
stream_filter_LTLIBRARIES += libhds_plugin.la

librecord_plugin_la_SOURCES = stream_filter/record.c
stream_filter_LTLIBRARIES += librecord_plugin.la

libaribcam_plugin_la_SOURCES = stream_filter/aribcam.c
libaribcam_plugin_la_CFLAGS = $(AM_CFLAGS) $(ARIBB25_CFLAGS)
libaribcam_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(ARIBB25_LDFLAGS) -rpath '$(stream_filterdir)'
libaribcam_plugin_la_LIBADD = $(ARIBB25_LIBS)
stream_filter_LTLIBRARIES += $(LTLIBaribcam)
EXTRA_LTLIBRARIES += libaribcam_plugin.la

libaccesstweaks_plugin_la_SOURCES = stream_filter/accesstweaks.c
libaccesstweaks_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(stream_filterdir)'
check_LTLIBRARIES += libaccesstweaks_plugin.la

libadf_plugin_la_SOURCES = stream_filter/adf.c
stream_filter_LTLIBRARIES += libadf_plugin.la

libskiptags_plugin_la_SOURCES = stream_filter/skiptags.c
stream_filter_LTLIBRARIES += libskiptags_plugin.la
