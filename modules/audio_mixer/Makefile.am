include $(top_srcdir)/modules/common.am
audio_mixerdir = $(pluginsdir)/audio_mixer

libfloat_mixer_plugin_la_SOURCES = float.c
libfloat_mixer_plugin_la_LIBADD = $(LIBM)

libinteger_mixer_plugin_la_SOURCES = integer.c

audio_mixer_LTLIBRARIES = \
	libfloat_mixer_plugin.la \
	libinteger_mixer_plugin.la
