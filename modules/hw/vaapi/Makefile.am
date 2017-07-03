vaapidir = $(pluginsdir)/vaapi

libvaapi_chroma_plugin_la_SOURCES = hw/vaapi/chroma.c hw/vaapi/vlc_vaapi.c hw/vaapi/vlc_vaapi.h	\
				    video_chroma/copy.c video_chroma/copy.h
libvaapi_chroma_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBVA_CFLAGS)
libvaapi_chroma_plugin_la_LIBADD = $(AM_LIBADD) $(LIBVA_LIBS)

libvaapi_filters_plugin_la_SOURCES = hw/vaapi/filters.c hw/vaapi/vlc_vaapi.c hw/vaapi/vlc_vaapi.h
libvaapi_filters_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libvaapi_filters_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBVA_CFLAGS)
libvaapi_filters_plugin_la_LIBADD = $(AM_LIBADD) $(LIBVA_LIBS)

if HAVE_VAAPI
vaapi_LTLIBRARIES = libvaapi_chroma_plugin.la	\
		    libvaapi_filters_plugin.la
endif
