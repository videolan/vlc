### XCB ###
libvlc_xcb_events_la_SOURCES = \
	video_output/xcb/events.c video_output/xcb/events.h
libvlc_xcb_events_la_CFLAGS = $(AM_CFLAGS) $(XCB_CFLAGS)
libvlc_xcb_events_la_LDFLAGS = \
	-no-undefined \
	-export-symbols-regex ^vlc_xcb_ \
	-version-info 0:0:0
libvlc_xcb_events_la_LIBADD = $(XCB_LIBS) $(LTLIBVLCCORE)

libxcb_x11_plugin_la_SOURCES = \
	video_output/xcb/pictures.c video_output/xcb/pictures.h \
	video_output/xcb/x11.c
libxcb_x11_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(XCB_CFLAGS) $(XCB_SHM_CFLAGS)
libxcb_x11_plugin_la_LIBADD = libvlc_xcb_events.la \
	$(XCB_LIBS) $(XCB_SHM_LIBS)

libxcb_render_plugin_la_SOURCES = \
	video_output/xcb/pictures.c video_output/xcb/pictures.h \
	video_output/xcb/render.c
libxcb_render_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(XCB_CFLAGS) $(XCB_SHM_CFLAGS) $(XCB_RENDER_CFLAGS)
libxcb_render_plugin_la_LIBADD = libvlc_xcb_events.la \
	$(XCB_LIBS) $(XCB_SHM_LIBS) $(XCB_RENDER_LIBS) $(LIBM)

libxcb_window_plugin_la_SOURCES = video_output/xcb/window.c
libxcb_window_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(CFLAGS_xcb_window) \
	$(XPROTO_CFLAGS) $(XCB_CFLAGS) \
	$(XCB_XKB_CFLAGS) $(XKBCOMMON_X11_CFLAGS)
libxcb_window_plugin_la_LIBADD = $(XPROTO_LIBS) $(XCB_LIBS) \
	$(XCB_XKB_LIBS) $(XKBCOMMON_X11_LIBS)

libvk_x11_plugin_la_SOURCES = $(VULKAN_COMMONSOURCES) \
	video_output/vulkan/surface.c \
	video_output/vulkan/platform_xcb.c
libvk_x11_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(XCB_CFLAGS) $(VULKAN_COMMONCFLAGS) \
	-DVK_USE_PLATFORM_XCB_KHR -DPLATFORM_NAME=X11
libvk_x11_plugin_la_LIBADD = $(VULKAN_COMMONLIBS) $(XCB_LIBS)

libegl_x11_plugin_la_SOURCES = video_output/opengl/egl.c
libegl_x11_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_PLATFORM_X11=1
libegl_x11_plugin_la_CFLAGS = $(AM_CFLAGS) $(EGL_CFLAGS)
libegl_x11_plugin_la_LIBADD = $(EGL_LIBS) $(X_LIBS) $(X_PRE_LIBS) -lX11

libglx_plugin_la_SOURCES = video_output/glx.c
libglx_plugin_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS)
libglx_plugin_la_LIBADD = $(GL_LIBS) $(X_LIBS) $(X_PRE_LIBS) -lX11

if HAVE_XCB
pkglib_LTLIBRARIES += libvlc_xcb_events.la
vout_LTLIBRARIES += \
	libxcb_x11_plugin.la \
	libxcb_render_plugin.la \
	libxcb_window_plugin.la
if HAVE_XKBCOMMON_X11
libxcb_window_plugin_la_SOURCES += \
	video_output/xcb/keysym.h video_output/xcb/xcb_keysym.h \
	video_output/xcb/vlc_xkb.h video_output/xcb/xkb.c
libxcb_window_plugin_la_CFLAGS += -DHAVE_XKBCOMMON
endif
if HAVE_VULKAN
vout_LTLIBRARIES += libvk_x11_plugin.la
endif
if HAVE_EGL
vout_LTLIBRARIES += libegl_x11_plugin.la
endif
if HAVE_GL
vout_LTLIBRARIES += libglx_plugin.la
endif
endif
