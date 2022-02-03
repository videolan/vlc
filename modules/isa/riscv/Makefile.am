riscvdir = $(pluginsdir)/riscv

libtransform_rvv_plugin_la_SOURCES = \
	isa/riscv/transform.c isa/riscv/rvv_transform.S

libvolume_rvv_plugin_la_SOURCES = isa/riscv/mixer.c isa/riscv/rvv_amplify.S
libvolume_rvv_plugin_la_LIBADD = $(AM_LIBADD) $(LIBM)

if HAVE_RVV
riscv_LTLIBRARIES = \
	libtransform_rvv_plugin.la \
	libvolume_rvv_plugin.la
endif
