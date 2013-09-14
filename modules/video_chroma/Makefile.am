basedir = video_chroma
include $(top_srcdir)/modules/common.am

AUTOMAKE_OPTIONS = subdir-objects

libchain_plugin_la_SOURCES = chain.c
libchain_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

libchroma_omx_plugin_la_SOURCES = omxdl.c
libchroma_omx_plugin_la_CFLAGS = $(AM_CFLAGS) $(OMXIP_CFLAGS)
libchroma_omx_plugin_la_LIBADD = $(AM_LIBADD) $(OMXIP_LIBS)

libswscale_plugin_la_SOURCES = swscale.c ../codec/avcodec/chroma.c
libswscale_plugin_la_CFLAGS = $(AM_CFLAGS) $(SWSCALE_CFLAGS)
libswscale_plugin_la_LIBADD = $(AM_LIBADD) $(SWSCALE_LIBS)

libgrey_yuv_plugin_la_SOURCES = grey_yuv.c
libgrey_yuv_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

libi420_rgb_plugin_la_SOURCES = i420_rgb.c i420_rgb.h \
	i420_rgb8.c i420_rgb16.c i420_rgb_c.h
libi420_rgb_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libi420_rgb_plugin_la_LIBADD = $(AM_LIBADD) $(LIBM)

libi420_yuy2_plugin_la_SOURCES = i420_yuy2.c i420_yuy2.h
libi420_yuy2_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

libi422_i420_plugin_la_SOURCES = i422_i420.c
libi422_i420_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

libi422_yuy2_plugin_la_SOURCES = i422_yuy2.c i422_yuy2.h
libi422_yuy2_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

librv32_plugin_la_SOURCES = rv32.c
librv32_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

libyuy2_i420_plugin_la_SOURCES = yuy2_i420.c
libyuy2_i420_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

libyuy2_i422_plugin_la_SOURCES = yuy2_i422.c
libyuy2_i422_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

libvlc_LTLIBRARIES = \
	libi420_rgb_plugin.la \
	libi420_yuy2_plugin.la \
	libi422_i420_plugin.la \
	libi422_yuy2_plugin.la \
	libgrey_yuv_plugin.la \
	libyuy2_i420_plugin.la \
	libyuy2_i422_plugin.la \
	librv32_plugin.la \
	libchain_plugin.la \
	$(LTLIBswscale)

EXTRA_LTLIBRARIES = libswscale_plugin.la libchroma_omx_plugin.la

# MMX
libi420_rgb_mmx_plugin_la_SOURCES = i420_rgb.c i420_rgb.h \
	i420_rgb16.c i420_rgb_mmx.h
libi420_rgb_mmx_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

libi420_yuy2_mmx_plugin_la_SOURCES = i420_yuy2.c i420_yuy2.h
libi420_yuy2_mmx_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

libi422_yuy2_mmx_plugin_la_SOURCES = i422_yuy2.c i422_yuy2.h
libi422_yuy2_mmx_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

if HAVE_MMX
libvlc_LTLIBRARIES += \
	libi420_rgb_mmx_plugin.la \
	libi420_yuy2_mmx_plugin.la \
	libi422_yuy2_mmx_plugin.la
endif

# SSE2
libi420_rgb_sse2_plugin_la_SOURCES = i420_rgb.c i420_rgb.h \
	i420_rgb16.c i420_rgb_sse2.h
libi420_rgb_sse2_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

libi420_yuy2_sse2_plugin_la_SOURCES = i420_yuy2.c i420_yuy2.h
libi420_yuy2_sse2_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

libi422_yuy2_sse2_plugin_la_SOURCES = i422_yuy2.c i422_yuy2.h
libi422_yuy2_sse2_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

if HAVE_SSE2
libvlc_LTLIBRARIES += \
	libi420_rgb_sse2_plugin.la \
	libi420_yuy2_sse2_plugin.la \
	libi422_yuy2_sse2_plugin.la
endif
