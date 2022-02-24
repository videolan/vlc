aarch64dir = $(pluginsdir)/aarch64
aarch64_LTLIBRARIES =

libdeinterlace_aarch64_plugin_la_SOURCES = \
	isa/aarch64/simd/deinterlace.c isa/aarch64/simd/merge.S

if HAVE_ARM64
aarch64_LTLIBRARIES += \
	libdeinterlace_aarch64_plugin.la
endif

libdeinterlace_sve_plugin_la_SOURCES = \
	isa/aarch64/sve/deinterlace.c isa/aarch64/sve/merge.S

if HAVE_SVE
aarch64_LTLIBRARIES += \
	libdeinterlace_sve_plugin.la
endif
