d3d11dir = $(pluginsdir)/d3d11

libdirect3d11_filters_plugin_la_SOURCES = hw/d3d11/d3d11_filters.h \
    hw/d3d11/d3d11_filters.c \
    hw/d3d11/d3d11_deinterlace.c \
    hw/d3d11/d3d11_device.c \
    hw/d3d11/d3d11_surface.c \
    hw/d3d11/d3d11_processor.c hw/d3d11/d3d11_processor.h
libdirect3d11_filters_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(d3d11dir)'
libdirect3d11_filters_plugin_la_LIBADD = libchroma_copy.la libdeinterlace_common.la libd3d11_common.la
if HAVE_WINSTORE
libdirect3d11_filters_plugin_la_LIBADD += -ld3d11
endif

d3d11_LTLIBRARIES = $(LTLIBdirect3d11_filters)
EXTRA_LTLIBRARIES += libdirect3d11_filters_plugin.la

