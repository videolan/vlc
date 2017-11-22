d3d9dir = $(pluginsdir)/d3d9
d3d9_LTLIBRARIES =

# GPU/CPU converter
libdxa9_plugin_la_SOURCES = video_chroma/dxa9.c
libdxa9_plugin_la_LIBADD = libd3d9_common.la libchroma_copy.la

if HAVE_AVCODEC_DXVA2
d3d9_LTLIBRARIES += libdxa9_plugin.la
endif

libdirect3d9_deinterlace_plugin_la_SOURCES = video_output/win32/dxva2_deinterlace.c
libdirect3d9_deinterlace_plugin_la_LIBADD = libd3d9_common.la $(LIBCOM) libdeinterlace_common.la
libdirect3d9_deinterlace_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
libdirect3d9_adjust_plugin_la_SOURCES = video_output/win32/d3d9_adjust.c
libdirect3d9_adjust_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
libdirect3d9_adjust_plugin_la_LIBADD = libd3d9_common.la $(LIBCOM)

if HAVE_WIN32_DESKTOP
#video_filter_LTLIBRARIES = $(LTLIBdirect3d9_deinterlace) $(LTLIBdirect3d9_adjust)
d3d9_LTLIBRARIES += libdirect3d9_deinterlace_plugin.la libdirect3d9_adjust_plugin.la
endif
