basedir = video_splitter

include $(top_srcdir)/modules/common.am

libvlc_LTLIBRARIES = \
	libclone_plugin.la \
	libwall_plugin.la

libclone_plugin_la_SOURCES = clone.c
libclone_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
if HAVE_WIN32
libclone_plugin_la_DEPENDENCIES = libclone_plugin.rc.o
endif

libwall_plugin_la_SOURCES = wall.c
libwall_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
if HAVE_WIN32
libwall_plugin_la_DEPENDENCIES = libwall_plugin.rc.o
endif

libpanoramix_plugin_la_SOURCES = panoramix.c
libpanoramix_plugin_la_CFLAGS =  $(AM_CFLAGS)
libpanoramix_plugin_la_LIBADD =  $(AM_LIBADD) $(LIBM)
if HAVE_WIN32
libvlc_LTLIBRARIES += libpanoramix_plugin.la
libpanoramix_plugin_la_DEPENDENCIES = libpanoramix_plugin.rc.o
else
if HAVE_XCB_RANDR
libpanoramix_plugin_la_CFLAGS += $(XCB_RANDR_CFLAGS)
libpanoramix_plugin_la_LIBADD += $(XCB_RANDR_LIBS)
libvlc_LTLIBRARIES += libpanoramix_plugin.la
endif
endif
