include $(top_srcdir)/modules/common.am
mmaldir = $(pluginsdir)/mmal

libmmal_vout_plugin_la_SOURCES = vout.c subpic.c subpic.h mmal_picture.c mmal_picture.h \
  mmal_cma.c mmal_cma.h
libmmal_vout_plugin_la_CFLAGS = $(AM_CFLAGS) $(MMAL_CFLAGS)
libmmal_vout_plugin_la_LIBADD = $(MMAL_LIBS) $(LIBM)

libmmal_codec_plugin_la_SOURCES = codec.c subpic.c subpic.h mmal_picture.c mmal_picture.h \
  mmal_cma.c mmal_cma.h
libmmal_codec_plugin_la_CFLAGS = $(AM_CFLAGS) $(MMAL_CFLAGS)
libmmal_codec_plugin_la_LIBADD = $(MMAL_LIBS)

libmmal_converter_plugin_la_SOURCES = converter.c subpic.c subpic.h mmal_picture.c mmal_picture.h \
  mmal_cma.c mmal_cma.h
libmmal_converter_plugin_la_CFLAGS = $(AM_CFLAGS) $(MMAL_CFLAGS)
libmmal_converter_plugin_la_LIBADD = $(MMAL_LIBS)

libmmal_deinterlace_plugin_la_SOURCES = deinterlace.c mmal_picture.c mmal_picture.h \
  mmal_cma.c mmal_cma.h
libmmal_deinterlace_plugin_la_CFLAGS = $(AM_CFLAGS) $(MMAL_CFLAGS)
libmmal_deinterlace_plugin_la_LIBADD = $(MMAL_LIBS)

libmmal_device_plugin_la_SOURCES = decoder_device.c mmal_picture.h
libmmal_device_plugin_la_CFLAGS = $(AM_CFLAGS) $(MMAL_CFLAGS)
libmmal_device_plugin_la_LIBADD = $(MMAL_LIBS)

if HAVE_MMAL
mmal_LTLIBRARIES = \
	libmmal_device_plugin.la \
	libmmal_deinterlace_plugin.la \
	libmmal_converter_plugin.la \
	libmmal_codec_plugin.la \
	libmmal_vout_plugin.la
endif
