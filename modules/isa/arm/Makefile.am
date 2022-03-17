armdir = $(pluginsdir)/arm
neondir = $(pluginsdir)/arm_neon

arm_LTLIBRARIES =
arm_PLUGINS =
arm_RPATH =

neon_LTLIBRARIES =
neon_PLUGINS =
neon_RPATH =

if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(arm_PLUGINS) $(neon_PLUGINS)
else
arm_LTLIBRARIES += $(arm_PLUGINS)
neon_LTLIBRARIES += $(neon_PLUGINS)
arm_RPATH += -rpath '$(armdir)'
neon_RPATH += -rpath '$(neondir)'
endif

libdeinterlace_arm_simd_plugin_la_SOURCES = \
	isa/arm/simd/deinterlace.c isa/arm/simd/merge.S

# TODO? use dedicated conditional, or purge
if HAVE_NEON
arm_PLUGINS += \
	libdeinterlace_arm_simd_plugin.la
endif

libchroma_yuv_neon_plugin_la_SOURCES = \
	isa/arm/neon/deinterleave_chroma.S \
	isa/arm/neon/i420_yuyv.S \
	isa/arm/neon/i422_yuyv.S \
	isa/arm/neon/yuyv_i422.S \
	isa/arm/neon/chroma_yuv.c isa/arm/neon/chroma_neon.h
libchroma_yuv_neon_plugin_LIBTOOLFLAGS = --tag=CC

libdeinterlace_neon_plugin_la_SOURCES = \
	isa/arm/neon/deinterlace.c isa/arm/neon/merge.S

libvolume_neon_plugin_la_SOURCES = isa/arm/neon/volume.c isa/arm/neon/amplify.S
libvolume_neon_plugin_LIBTOOLFLAGS = --tag=CC

libyuv_rgb_neon_plugin_la_SOURCES = \
	isa/arm/neon/i420_rgb.S \
	isa/arm/neon/i420_rv16.S \
	isa/arm/neon/nv21_rgb.S \
	isa/arm/neon/nv12_rgb.S \
	isa/arm/neon/yuv_rgb.c
libyuv_rgb_neon_plugin_LIBTOOLFLAGS = --tag=CC

if HAVE_NEON
neon_PLUGINS += \
	libchroma_yuv_neon_plugin.la \
	libdeinterlace_neon_plugin.la \
	libvolume_neon_plugin.la \
	libyuv_rgb_neon_plugin.la
endif

noinst_HEADERS += isa/arm/asm.S
