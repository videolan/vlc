d3d11dir = $(pluginsdir)/d3d11
d3d11_LTLIBRARIES =
d3d11_PLUGINS =
d3d11_RPATH =
if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(d3d11_PLUGINS)
else
d3d11_LTLIBRARIES += $(d3d11_PLUGINS)
d3d11_RPATH += -rpath '$(d3d11dir)'
endif

libdirect3d11_filters_plugin_la_SOURCES = hw/d3d11/d3d11_filters.h \
    hw/d3d11/d3d11_filters.c \
    hw/d3d11/d3d11_deinterlace.c \
    hw/d3d11/d3d11_device.c \
    hw/d3d11/d3d11_surface.c \
    hw/d3d11/d3d11_processor.c hw/d3d11/d3d11_processor.h
libdirect3d11_filters_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(d3d11_RPATH)
libdirect3d11_filters_plugin_la_LIBADD = libchroma_copy.la libdeinterlace_common.la libd3d11_common.la
if HAVE_WINSTORE
libdirect3d11_filters_plugin_la_LIBADD += -ld3d11
endif

d3d11_PLUGINS += $(LTLIBdirect3d11_filters)
EXTRA_LTLIBRARIES += libdirect3d11_filters_plugin.la
