LIBTOOL=@LIBTOOL@ --tag=CC

neondir = $(pluginsdir)/arm_neon
neon_PLUGINS =
neon_LTLIBRARIES =
if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(neon_PLUGINS)
else
neon_LTLIBRARIES += $(neon_PLUGINS)
endif


libchroma_yuv_neon_plugin_la_SOURCES = \
	arm_neon/deinterleave_chroma.S \
	arm_neon/i420_yuyv.S \
	arm_neon/i422_yuyv.S \
	arm_neon/yuyv_i422.S \
	arm_neon/chroma_yuv.c arm_neon/chroma_neon.h
libchroma_yuv_neon_plugin_la_CFLAGS = $(AM_CFLAGS)
libchroma_yuv_neon_plugin_LIBTOOLFLAGS = --tag=CC

libvolume_neon_plugin_la_SOURCES = arm_neon/volume.c arm_neon/amplify.S
libvolume_neon_plugin_la_CFLAGS = $(AM_CFLAGS)
libvolume_neon_plugin_LIBTOOLFLAGS = --tag=CC

libyuv_rgb_neon_plugin_la_SOURCES = \
	arm_neon/i420_rgb.S \
	arm_neon/i420_rv16.S \
	arm_neon/nv21_rgb.S \
	arm_neon/nv12_rgb.S \
	arm_neon/yuv_rgb.c
libyuv_rgb_neon_plugin_la_CFLAGS = $(AM_CFLAGS)
libyuv_rgb_neon_plugin_LIBTOOLFLAGS = --tag=CC

if HAVE_NEON
neon_PLUGINS += \
	libchroma_yuv_neon_plugin.la \
	libyuv_rgb_neon_plugin.la
endif

EXTRA_DIST += arm_neon/asm.S
