### Wayland ###
libwl_shm_plugin_la_SOURCES = \
	video_output/wayland/registry.c video_output/wayland/registry.h \
	video_output/wayland/shm.c
nodist_libwl_shm_plugin_la_SOURCES = \
	video_output/wayland/viewporter-client-protocol.h \
	video_output/wayland/viewporter-protocol.c
libwl_shm_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-I$(builddir)/video_output/wayland
libwl_shm_plugin_la_CFLAGS = $(WAYLAND_CLIENT_CFLAGS)
libwl_shm_plugin_la_LIBADD = $(WAYLAND_CLIENT_LIBS)
CLEANFILES += $(nodist_libwl_shm_plugin_la_SOURCES)

video_output/wayland/viewporter-client-protocol.h: \
		$(WAYLAND_PROTOCOLS)/stable/viewporter/viewporter.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header $< $@

video_output/wayland/viewporter-protocol.c: \
		$(WAYLAND_PROTOCOLS)/stable/viewporter/viewporter.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) private-code $< $@

libwl_shell_plugin_la_SOURCES = $(libxdg_shell_plugin_la_SOURCES)
libwl_shell_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-I$(builddir)/video_output/wayland
libwl_shell_plugin_la_CFLAGS = $(libxdg_shell_plugin_la_CFLAGS)
libwl_shell_plugin_la_LIBADD = $(libxdg_shell_plugin_la_LIBADD)

libxdg_shell_plugin_la_SOURCES = \
	video_output/wayland/output.h video_output/wayland/output.c \
	video_output/wayland/input.h video_output/wayland/input.c \
	video_output/xcb/vlc_xkb.h video_output/xcb/xkb.c \
	video_output/wayland/xdg-shell.c
nodist_libxdg_shell_plugin_la_SOURCES = \
	video_output/wayland/xdg-shell-client-protocol.h \
	video_output/wayland/xdg-shell-protocol.c \
	video_output/wayland/xdg-decoration-client-protocol.h \
	video_output/wayland/xdg-decoration-protocol.c
libxdg_shell_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DXDG_SHELL \
	-I$(builddir)/video_output/wayland
libxdg_shell_plugin_la_CFLAGS = \
	$(WAYLAND_CLIENT_CFLAGS) $(WAYLAND_CURSOR_CFLAGS) \
	$(XKBCOMMON_CFLAGS)
libxdg_shell_plugin_la_LIBADD = \
	$(WAYLAND_CLIENT_LIBS) $(WAYLAND_CURSOR_LIBS) \
	$(XKBCOMMON_LIBS)
if HAVE_XKBCOMMON
libwl_shell_plugin_la_CPPFLAGS += -DHAVE_XKBCOMMON
libxdg_shell_plugin_la_CPPFLAGS += -DHAVE_XKBCOMMON
endif

video_output/wayland/xdg-shell-client-protocol.h: \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml \
		video_output/Makefile.am
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header $< $@

video_output/wayland/xdg-shell-protocol.c: \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml \
		video_output/Makefile.am
	$(AM_V_GEN)$(WAYLAND_SCANNER) private-code $< $@

video_output/wayland/xdg-decoration-client-protocol.h: \
		$(WAYLAND_PROTOCOLS)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml \
		video_output/Makefile.am
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header $< $@

video_output/wayland/xdg-decoration-protocol.c: \
		$(WAYLAND_PROTOCOLS)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml \
		video_output/Makefile.am
	$(AM_V_GEN)$(WAYLAND_SCANNER) private-code $< $@

libegl_wl_plugin_la_SOURCES = video_output/opengl/egl.c
libegl_wl_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_PLATFORM_WAYLAND=1
libegl_wl_plugin_la_CFLAGS = $(AM_CFLAGS) $(EGL_CFLAGS) $(WAYLAND_EGL_CFLAGS)
libegl_wl_plugin_la_LIBADD = $(EGL_LIBS) $(WAYLAND_EGL_LIBS)

if HAVE_WAYLAND
BUILT_SOURCES += $(nodist_libwl_shm_plugin_la_SOURCES)
vout_LTLIBRARIES += libwl_shm_plugin.la
vout_LTLIBRARIES += libwl_shell_plugin.la
BUILT_SOURCES += $(nodist_libxdg_shell_plugin_la_SOURCES)
vout_LTLIBRARIES += libxdg_shell_plugin.la
if HAVE_EGL
vout_LTLIBRARIES += libegl_wl_plugin.la
endif
endif
