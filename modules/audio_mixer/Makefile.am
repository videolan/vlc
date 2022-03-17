audio_mixerdir = $(pluginsdir)/audio_mixer

libfloat_mixer_plugin_la_SOURCES = audio_mixer/float.c
libfloat_mixer_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libfloat_mixer_plugin_la_LIBADD = $(LIBM)

libinteger_mixer_plugin_la_SOURCES = audio_mixer/integer.c
libinteger_mixer_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libinteger_mixer_plugin_la_LIBADD = $(LIBM)

audio_mixer_PLUGINS = \
	libfloat_mixer_plugin.la \
	libinteger_mixer_plugin.la

audio_mixer_LTLIBRARIES =
if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(audio_mixer_PLUGINS)
else
audio_mixer_LTLIBRARIES += $(audio_mixer_PLUGINS)
endif
