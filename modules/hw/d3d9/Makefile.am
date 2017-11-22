d3d9dir = $(pluginsdir)/d3d9

libdirect3d9_filters_plugin_la_SOURCES = hw/d3d9/d3d9_filters.h \
    hw/d3d9/d3d9_filters.c \
    hw/d3d9/dxva2_deinterlace.c \
    hw/d3d9/dxa9.c \
    hw/d3d9/d3d9_instance.c
libdirect3d9_filters_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(d3d9dir)'
libdirect3d9_filters_plugin_la_LIBADD = libchroma_copy.la libdeinterlace_common.la libd3d9_common.la $(LIBCOM)

if HAVE_WIN32_DESKTOP
d3d9_LTLIBRARIES = $(LTLIBdirect3d9_filters)
EXTRA_LTLIBRARIES += libdirect3d9_filters_plugin.la
endif
