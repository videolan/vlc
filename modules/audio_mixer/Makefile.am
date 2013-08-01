basedir = audio_mixer
include $(top_srcdir)/modules/common.am

libfloat_mixer_plugin_la_SOURCES = float.c
libfloat_mixer_plugin_la_CFLAGS = $(AM_CFLAGS)
libfloat_mixer_plugin_la_LIBADD = $(AM_LIBADD) $(LIBM)
if HAVE_WIN32
libfloat_mixer_plugin_la_DEPENDENCIES = libfloat_mixer_plugin.rc.o
endif

libinteger_mixer_plugin_la_SOURCES = integer.c
libinteger_mixer_plugin_la_CFLAGS = $(AM_CFLAGS)
libinteger_mixer_plugin_la_LIBADD = $(AM_LIBADD)
if HAVE_WIN32
libinteger_mixer_plugin_la_DEPENDENCIES = libinteger_mixer_plugin.rc.o
endif

libvlc_LTLIBRARIES = \
	libfloat_mixer_plugin.la \
	libinteger_mixer_plugin.la
