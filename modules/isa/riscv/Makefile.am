riscvdir = $(pluginsdir)/riscv

libvolume_rvv_plugin_la_SOURCES = isa/riscv/mixer.c isa/riscv/rvv_amplify.S
libvolume_rvv_plugin_la_LIBADD = $(AM_LIBADD) $(LIBM)

if HAVE_RVV
riscv_LTLIBRARIES = \
	libvolume_rvv_plugin.la
endif
