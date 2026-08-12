ppcdir = $(pluginsdir)/ppc
ppc_PLUGINS =
ppc_LTLIBRARIES =
if HAVE_PARTIAL_LINKING
noinst_LTLIBRARIES += $(ppc_PLUGINS)
else
ppc_LTLIBRARIES += $(ppc_PLUGINS)
endif

libdeinterlace_ppc_plugin_la_SOURCES = \
    isa/ppc/deinterlace.c

if HAVE_ALTIVEC
ppc_PLUGINS += \
    libdeinterlace_ppc_plugin.la
endif
