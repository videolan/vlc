vaapidir = $(pluginsdir)/vaapi

libvaapi_filters_plugin_la_SOURCES = hw/vaapi/filters.c hw/vaapi/filters.h \
	hw/vaapi/vlc_vaapi.c hw/vaapi/vlc_vaapi.h \
	hw/vaapi/chroma.c video_chroma/copy.c video_chroma/copy.h

libvaapi_filters_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libvaapi_filters_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBVA_CFLAGS)
libvaapi_filters_plugin_la_LIBADD = $(AM_LIBADD) $(LIBVA_LIBS)

if HAVE_VAAPI
vaapi_LTLIBRARIES = libvaapi_filters_plugin.la
endif
