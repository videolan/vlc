d3d11dir = $(pluginsdir)/d3d11
d3d11_LTLIBRARIES =

libdirect3d11_deinterlace_plugin_la_SOURCES = hw/d3d11/d3d11_deinterlace.c
libdirect3d11_deinterlace_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
libdirect3d11_deinterlace_plugin_la_LIBADD = libdeinterlace_common.la libd3d11_common.la
if HAVE_WINSTORE
libdirect3d11_deinterlace_plugin_la_LIBADD += -ld3d11
endif
libdirect3d11_adjust_plugin_la_SOURCES = hw/d3d11/d3d11_adjust.c
libdirect3d11_adjust_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
libdirect3d11_adjust_plugin_la_LIBADD = libd3d11_common.la
if HAVE_WINSTORE
libdirect3d11_adjust_plugin_la_LIBADD += -ld3d11
endif

if HAVE_AVCODEC_D3D11VA
#video_filter_LTLIBRARIES += $(LTLIBdirect3d11_deinterlace) $(LTLIBdirect3d11_adjust)
d3d11_LTLIBRARIES += libdirect3d11_deinterlace_plugin.la libdirect3d11_adjust_plugin.la
endif

# GPU/CPU converter
libd3d11_surface_plugin_la_SOURCES = hw/d3d11/d3d11_surface.c
libd3d11_surface_plugin_la_LIBADD = libchroma_copy.la libd3d11_common.la
if HAVE_WINSTORE
libd3d11_surface_plugin_la_LIBADD += -ld3d11
endif

if HAVE_AVCODEC_D3D11VA
d3d11_LTLIBRARIES += libd3d11_surface_plugin.la
endif

