vaapidir = $(pluginsdir)/vaapi

libdecdev_vaapi_wl_plugin_la_SOURCES = hw/vaapi/decoder_device.c \
	hw/vaapi/vlc_vaapi.c hw/vaapi/vlc_vaapi.h
libdecdev_vaapi_wl_plugin_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS) -DHAVE_VA_WL $(LIBVA_WL_CFLAGS)
libdecdev_vaapi_wl_plugin_la_LIBADD = $(LIBVA_LIBS) $(LIBVA_WL_LIBS)

libdecdev_vaapi_x11_plugin_la_SOURCES = $(libdecdev_vaapi_wl_plugin_la_SOURCES)
libdecdev_vaapi_x11_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBVA_CFLAGS) -DHAVE_VA_X11
libdecdev_vaapi_x11_plugin_la_LIBADD = $(LIBVA_LIBS) $(LIBVA_X11_LIBS) $(X_LIBS) $(X_PRE_LIBS) -lX11

libdecdev_vaapi_drm_plugin_la_SOURCES = $(libdecdev_vaapi_wl_plugin_la_SOURCES)
libdecdev_vaapi_drm_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBVA_CFLAGS) -DHAVE_VA_DRM
libdecdev_vaapi_drm_plugin_la_LIBADD = $(LIBVA_LIBS) $(LIBVA_DRM_LIBS)

libvaapi_filters_plugin_la_SOURCES = hw/vaapi/filters.c hw/vaapi/filters.h \
	hw/vaapi/vlc_vaapi.c hw/vaapi/vlc_vaapi.h \
	hw/vaapi/chroma.c

libvaapi_filters_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libvaapi_filters_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBVA_CFLAGS)
libvaapi_filters_plugin_la_LIBADD = libchroma_copy.la $(AM_LIBADD) $(LIBVA_LIBS)

if HAVE_VAAPI
vaapi_LTLIBRARIES = libvaapi_filters_plugin.la
if HAVE_WAYLAND
if HAVE_VAAPI_WL
vaapi_LTLIBRARIES += libdecdev_vaapi_wl_plugin.la
endif
endif
if HAVE_XCB
if HAVE_VAAPI_X11
vaapi_LTLIBRARIES += libdecdev_vaapi_x11_plugin.la
endif
endif
if HAVE_VAAPI_DRM
vaapi_LTLIBRARIES += libdecdev_vaapi_drm_plugin.la
endif
endif
