stream_filterdir = $(pluginsdir)/stream_filter

stream_filter_LTLIBRARIES =

libdecomp_plugin_la_SOURCES = stream_filter/decomp.c
libdecomp_plugin_la_LIBADD = $(LIBPTHREAD)
if !HAVE_WIN32
stream_filter_LTLIBRARIES += libdecomp_plugin.la
endif

libsmooth_plugin_la_SOURCES = \
    stream_filter/smooth/smooth.c \
    stream_filter/smooth/utils.c \
    stream_filter/smooth/downloader.c \
    stream_filter/smooth/smooth.h \
    demux/mp4/libmp4.c demux/mp4/libmp4.h
libsmooth_plugin_la_CFLAGS = $(AM_CFLAGS)
libsmooth_plugin_la_LIBADD = $(LIBM)
if HAVE_ZLIB
libsmooth_plugin_la_LIBADD += -lz
endif
stream_filter_LTLIBRARIES += libsmooth_plugin.la

libhds_plugin_la_SOURCES = \
    stream_filter/hds/hds.c

libhds_plugin_la_CFLAGS = $(AM_CFLAGS)
stream_filter_LTLIBRARIES += libhds_plugin.la


libhttplive_plugin_la_SOURCES = stream_filter/httplive.c
libhttplive_plugin_la_CFLAGS = $(AM_CFLAGS) $(GCRYPT_CFLAGS)
libhttplive_plugin_la_LIBADD = $(GCRYPT_LIBS) -lgpg-error
if HAVE_GCRYPT
stream_filter_LTLIBRARIES += libhttplive_plugin.la
endif

librecord_plugin_la_SOURCES = stream_filter/record.c
stream_filter_LTLIBRARIES += librecord_plugin.la

libaribcam_plugin_la_SOURCES = stream_filter/aribcam.c
libaribcam_plugin_la_CFLAGS = $(AM_CFLAGS) $(ARIBB25_CFLAGS)
libaribcam_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(ARIBB25_LDFLAGS) -rpath '$(stream_filterdir)'
libaribcam_plugin_la_LIBADD = $(ARIBB25_LIBS)
stream_filter_LTLIBRARIES += $(LTLIBaribcam)
EXTRA_LTLIBRARIES += libaribcam_plugin.la
