aarch64dir = $(pluginsdir)/aarch64
aarch64_LTLIBRARIES =
aarch64_PLUGINS =
aarch64_RPATH =

if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(aarch64_PLUGINS)
else
aarch64_LTLIBRARIES += $(aarch64_PLUGINS)
aarch64_RPATH += -rpath '$(aarch64dir)'
endif

libdeinterlace_aarch64_plugin_la_SOURCES = \
	isa/aarch64/simd/deinterlace.c isa/aarch64/simd/merge.S

if HAVE_ARM64
aarch64_PLUGINS += \
	libdeinterlace_aarch64_plugin.la
endif

libdeinterlace_sve_plugin_la_SOURCES = \
	isa/aarch64/sve/deinterlace.c isa/aarch64/sve/merge.S

if HAVE_SVE
aarch64_PLUGINS += \
	libdeinterlace_sve_plugin.la
endif
