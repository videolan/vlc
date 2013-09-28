chromadir = $(pluginsdir)/video_chroma

libchain_plugin_la_SOURCES = video_chroma/chain.c

libchroma_omx_plugin_la_SOURCES = video_chroma/omxdl.c
libchroma_omx_plugin_la_CFLAGS = $(AM_CFLAGS) $(OMXIP_CFLAGS)
libchroma_omx_plugin_la_LIBADD = $(OMXIP_LIBS)

libswscale_plugin_la_SOURCES = video_chroma/swscale.c codec/avcodec/chroma.c
libswscale_plugin_la_CFLAGS = $(AM_CFLAGS) $(SWSCALE_CFLAGS)
libswscale_plugin_la_LIBADD = $(SWSCALE_LIBS)
libswscale_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(chromadir)'

libgrey_yuv_plugin_la_SOURCES = video_chroma/grey_yuv.c

libi420_rgb_plugin_la_SOURCES = video_chroma/i420_rgb.c video_chroma/i420_rgb.h \
	video_chroma/i420_rgb8.c video_chroma/i420_rgb16.c video_chroma/i420_rgb_c.h
libi420_rgb_plugin_la_LIBADD = $(LIBM)

libi420_yuy2_plugin_la_SOURCES = video_chroma/i420_yuy2.c video_chroma/i420_yuy2.h
libi420_yuy2_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_i420_yuy2

libi422_i420_plugin_la_SOURCES = video_chroma/i422_i420.c

libi422_yuy2_plugin_la_SOURCES = video_chroma/i422_yuy2.c video_chroma/i422_yuy2.h
libi422_yuy2_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_i422_yuy2

librv32_plugin_la_SOURCES = video_chroma/rv32.c

libyuy2_i420_plugin_la_SOURCES = video_chroma/yuy2_i420.c

libyuy2_i422_plugin_la_SOURCES = video_chroma/yuy2_i422.c

chroma_LTLIBRARIES = \
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

EXTRA_LTLIBRARIES += libswscale_plugin.la libchroma_omx_plugin.la

# AltiVec
libi420_yuy2_altivec_plugin_la_SOURCES = video_chroma/i420_yuy2.c video_chroma/i420_yuy2.h
libi420_yuy2_altivec_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_i420_yuy2_altivec
libi420_yuy2_altivec_plugin_la_CFLAGS = $(AM_CFLAGS) $(ALTIVEC_CFLAGS)

if HAVE_ALTIVEC
chroma_LTLIBRARIES += \
	libi420_yuy2_altivec_plugin.la
endif

# MMX
libi420_rgb_mmx_plugin_la_SOURCES = video_chroma/i420_rgb.c video_chroma/i420_rgb.h \
	video_chroma/i420_rgb16_x86.c video_chroma/i420_rgb_mmx.h
libi420_rgb_mmx_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DMMX

libi420_yuy2_mmx_plugin_la_SOURCES = video_chroma/i420_yuy2.c video_chroma/i420_yuy2.h
libi420_yuy2_mmx_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_i420_yuy2_mmx

libi422_yuy2_mmx_plugin_la_SOURCES = video_chroma/i422_yuy2.c video_chroma/i422_yuy2.h
libi422_yuy2_mmx_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_i422_yuy2_mmx

if HAVE_MMX
chroma_LTLIBRARIES += \
	libi420_rgb_mmx_plugin.la \
	libi420_yuy2_mmx_plugin.la \
	libi422_yuy2_mmx_plugin.la
endif

# SSE2
libi420_rgb_sse2_plugin_la_SOURCES = video_chroma/i420_rgb.c video_chroma/i420_rgb.h \
	video_chroma/i420_rgb16_x86.c video_chroma/i420_rgb_sse2.h
libi420_rgb_sse2_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DSSE2

libi420_yuy2_sse2_plugin_la_SOURCES = video_chroma/i420_yuy2.c video_chroma/i420_yuy2.h
libi420_yuy2_sse2_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_i420_yuy2_sse2

libi422_yuy2_sse2_plugin_la_SOURCES = video_chroma/i422_yuy2.c video_chroma/i422_yuy2.h
libi422_yuy2_sse2_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_i422_yuy2_sse2

if HAVE_SSE2
chroma_LTLIBRARIES += \
	libi420_rgb_sse2_plugin.la \
	libi420_yuy2_sse2_plugin.la \
	libi422_yuy2_sse2_plugin.la
endif
