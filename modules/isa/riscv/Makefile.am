riscvdir = $(pluginsdir)/riscv
riscv_LTLIBRARIES =
riscv_PLUGINS =
riscv_RPATH =

if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(riscv_PLUGINS)
else
riscv_LTLIBRARIES += $(riscv_PLUGINS)
riscv_RPATH += -rpath '$(riscvdir)'
endif


libdeinterlace_rvv_plugin_la_SOURCES = \
	isa/riscv/deinterlace.c isa/riscv/rvv_merge.S
libtransform_rvv_plugin_la_SOURCES = \
	isa/riscv/transform.c isa/riscv/rvv_transform.S

libvolume_rvv_plugin_la_SOURCES = isa/riscv/mixer.c isa/riscv/rvv_amplify.S
libvolume_rvv_plugin_la_LIBADD = $(AM_LIBADD) $(LIBM)

if HAVE_RVV
riscv_PLUGINS += \
	libdeinterlace_rvv_plugin.la \
	libtransform_rvv_plugin.la \
	libvolume_rvv_plugin.la
endif
