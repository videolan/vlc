x86dir = $(pluginsdir)/x86

x86_PLUGINS =
x86_LTLIBRARIES =
if HAVE_PARTIAL_LINKING
noinst_LTLIBRARIES += $(x86_PLUGINS)
else
x86_LTLIBRARIES += $(x86_PLUGINS)
endif

libdeinterlace_x86_plugin_la_SOURCES = \
    isa/x86/deinterlace.c

if HAVE_SSE2
x86_PLUGINS += \
    libdeinterlace_x86_plugin.la 
endif
