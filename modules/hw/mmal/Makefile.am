include $(top_srcdir)/modules/common.am
mmaldir = $(pluginsdir)/mmal

AM_CFLAGS += $(CFLAGS_mmal)
AM_LDFLAGS += -rpath '$(mmaldir)' $(LDFLAGS_mmal)

libmmal_vout_plugin_la_SOURCES = vout.c mmal_picture.c mmal_picture.h
libmmal_vout_plugin_la_CFLAGS = $(AM_CFLAGS)
libmmal_vout_plugin_la_LDFLAGS = $(AM_LDFLAGS) -lm
libmmal_vout_plugin_la_LIBADD = $(LIBS_mmal)
mmal_LTLIBRARIES = libmmal_vout_plugin.la

libmmal_codec_plugin_la_SOURCES = codec.c
libmmal_codec_plugin_la_CFLAGS = $(AM_CFLAGS)
libmmal_codec_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libmmal_codec_plugin_la_LIBADD = $(LIBS_mmal)
mmal_LTLIBRARIES += libmmal_codec_plugin.la

libmmal_deinterlace_plugin_la_SOURCES = deinterlace.c mmal_picture.c
libmmal_deinterlace_plugin_la_CFLAGS = $(AM_CFLAGS)
libmmal_deinterlace_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libmmal_deinterlace_plugin_la_LIBADD = $(LIBS_mmal)
mmal_LTLIBRARIES += libmmal_deinterlace_plugin.la
