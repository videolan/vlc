voutdir = $(pluginsdir)/video_output
vout_LTLIBRARIES =

EXTRA_DIST += video_output/README

OPENGL_COMMONSOURCES = video_output/opengl/vout_helper.c \
	video_output/opengl/vout_helper.h video_output/opengl/converter.h \
	video_output/opengl/internal.h video_output/opengl/fragment_shaders.c \
	video_output/opengl/converter_sw.c
if HAVE_LIBPLACEBO
OPENGL_COMMONSOURCES += video_output/placebo_utils.c video_output/placebo_utils.h
endif

OPENGL_COMMONCFLAGS = $(LIBPLACEBO_CFLAGS)
OPENGL_COMMONLIBS = $(LIBPLACEBO_LIBS)

if HAVE_DECKLINK
libdecklinkoutput_plugin_la_SOURCES = video_output/decklink.cpp
libdecklinkoutput_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(CPPFLAGS_decklinkoutput)
libdecklinkoutput_plugin_la_LIBADD = $(LIBS_decklink) $(LIBDL) -lpthread
vout_LTLIBRARIES += libdecklinkoutput_plugin.la
endif

libglconv_cvpx_plugin_la_SOURCES = video_output/opengl/converter_cvpx.c \
	video_output/opengl/converter.h
libglconv_cvpx_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'

if HAVE_OSX
libvout_macosx_plugin_la_SOURCES = video_output/macosx.m $(OPENGL_COMMONSOURCES)
libvout_macosx_plugin_la_CFLAGS = $(AM_CFLAGS) $(OPENGL_COMMONCFLAGS) -DHAVE_GL_CORE_SYMBOLS
libvout_macosx_plugin_la_LIBADD = $(OPENGL_COMMONLIBS)
libvout_macosx_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)' \
	-Wl,-framework,OpenGL,-framework,Cocoa

libcaopengllayer_plugin_la_SOURCES = video_output/caopengllayer.m $(OPENGL_COMMONSOURCES)
libcaopengllayer_plugin_la_CFLAGS = $(AM_CFLAGS) $(OPENGL_COMMONCFLAGS) -DHAVE_GL_CORE_SYMBOLS
libcaopengllayer_plugin_la_LIBADD = $(OPENGL_COMMONLIBS)
libcaopengllayer_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)' \
	-Wl,-framework,OpenGL,-framework,Cocoa,-framework,QuartzCore

libglconv_cvpx_plugin_la_SOURCES += codec/vt_utils.c codec/vt_utils.h
libglconv_cvpx_plugin_la_LDFLAGS += -Wl,-framework,IOSurface \
	-Wl,-framework,Foundation,-framework,CoreVideo,-framework,OpenGL
vout_LTLIBRARIES += libvout_macosx_plugin.la libcaopengllayer_plugin.la \
	libglconv_cvpx_plugin.la
endif
if HAVE_IOS
libglconv_cvpx_plugin_la_CFLAGS = $(AM_CFLAGS) -DUSE_OPENGL_ES2
endif

libvout_ios_plugin_la_SOURCES = video_output/ios.m $(OPENGL_COMMONSOURCES)
libvout_ios_plugin_la_CFLAGS = $(AM_CFLAGS) $(OPENGL_COMMONCFLAGS) -DUSE_OPENGL_ES2
libvout_ios_plugin_la_LIBADD = $(OPENGL_COMMONLIBS)
libvout_ios_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)' \
	-Wl,-framework,OpenGLES,-framework,QuartzCore,-framework,UIKit
if HAVE_IOS
vout_LTLIBRARIES += libvout_ios_plugin.la libglconv_cvpx_plugin.la
endif
if HAVE_TVOS
vout_LTLIBRARIES += libvout_ios_plugin.la libglconv_cvpx_plugin.la
endif

### OpenGL ###
libgles2_plugin_la_SOURCES = $(OPENGL_COMMONSOURCES) video_output/opengl/display.c
libgles2_plugin_la_CFLAGS = $(AM_CFLAGS) $(GLES2_CFLAGS) -DUSE_OPENGL_ES2 $(OPENGL_COMMONCFLAGS)
libgles2_plugin_la_LIBADD = $(GLES2_LIBS) $(LIBM) $(OPENGL_COMMONLIBS)
libgles2_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'

EXTRA_LTLIBRARIES += libgles2_plugin.la
vout_LTLIBRARIES += $(LTLIBgles2)

libgl_plugin_la_SOURCES = $(OPENGL_COMMONSOURCES) video_output/opengl/display.c
libgl_plugin_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS) $(OPENGL_COMMONCFLAGS)
libgl_plugin_la_LIBADD = $(LIBM) $(OPENGL_COMMONLIBS)
if HAVE_WIN32
libgl_plugin_la_CFLAGS += -DHAVE_GL_CORE_SYMBOLS
libgl_plugin_la_LIBADD += $(GL_LIBS)
endif

libglconv_vaapi_plugin_la_SOURCES = video_output/opengl/converter_vaapi.c \
	video_output/opengl/converter.h \
	hw/vaapi/vlc_vaapi.c hw/vaapi/vlc_vaapi.h
libglconv_vaapi_plugin_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS)
libglconv_vaapi_plugin_la_LIBADD = $(LIBVA_LIBS) $(LIBVA_EGL_LIBS)

libglconv_vdpau_plugin_la_SOURCES = video_output/opengl/converter_vdpau.c \
	video_output/opengl/converter.h hw/vdpau/picture.c hw/vdpau/vlc_vdpau.h
libglconv_vdpau_plugin_la_CFLAGS = $(AM_CFLAGS) $(VDPAU_CFLAGS)
libglconv_vdpau_plugin_la_LIBADD = $(LIBDL) libvlc_vdpau.la $(X_LIBS) $(X_PRE_LIBS) -lX11

if HAVE_GL
vout_LTLIBRARIES += libgl_plugin.la
if HAVE_EGL
if HAVE_VAAPI
vout_LTLIBRARIES += libglconv_vaapi_plugin.la
endif
endif # HAVE_EGL

if HAVE_VDPAU
vout_LTLIBRARIES += libglconv_vdpau_plugin.la
endif
endif # HAVE_GL

### Vulkan ###

VULKAN_COMMONSOURCES = video_output/vulkan/instance.c \
	video_output/vulkan/instance.h \
	video_output/vulkan/platform.h \
	video_output/placebo_utils.c \
	video_output/placebo_utils.h

# Trigger the c++ linker because of glslang dependency of libplacebo
VULKAN_COMMONSOURCES += dummy.cpp

VULKAN_COMMONCFLAGS = $(VULKAN_CFLAGS) $(LIBPLACEBO_CFLAGS)
VULKAN_COMMONLIBS = $(VULKAN_LIBS) $(LIBPLACEBO_LIBS)

libvk_plugin_la_SOURCES = $(VULKAN_COMMONSOURCES) video_output/vulkan/display.c
libvk_plugin_la_CFLAGS = $(AM_CFLAGS) $(VULKAN_COMMONCFLAGS)
libvk_plugin_la_LIBADD = $(VULKAN_COMMONLIBS)

libvk_x11_plugin_la_SOURCES = $(VULKAN_COMMONSOURCES) video_output/vulkan/surface.c \
			      video_output/vulkan/platform_xcb.c
libvk_x11_plugin_la_CFLAGS = $(AM_CFLAGS) $(XCB_CFLAGS) $(VULKAN_COMMONCFLAGS) \
			     -DVK_USE_PLATFORM_XCB_KHR -DPLATFORM_NAME=X11
libvk_x11_plugin_la_LIBADD = $(VULKAN_COMMONLIBS) $(XCB_LIBS)

libvk_win32_plugin_la_SOURCES = $(VULKAN_COMMONSOURCES) video_output/vulkan/surface.c \
				video_output/vulkan/platform_win32.c
libvk_win32_plugin_la_CFLAGS = $(AM_CFLAGS) $(VULKAN_COMMONCFLAGS) \
			       -DVK_USE_PLATFORM_WIN32_KHR -DPLATFORM_NAME=Win32
libvk_win32_plugin_la_LIBADD = $(VULKAN_COMMONLIBS)

libvk_android_plugin_la_SOURCES = $(VULKAN_COMMONSOURCES) video_output/vulkan/surface.c \
				video_output/vulkan/platform_android.c
libvk_android_plugin_la_CFLAGS = $(AM_CFLAGS) $(VULKAN_COMMONCFLAGS) \
			       -DVK_USE_PLATFORM_ANDROID_KHR -DPLATFORM_NAME=Android
libvk_android_plugin_la_LIBADD = $(VULKAN_COMMONLIBS)

if HAVE_VULKAN
vout_LTLIBRARIES += libvk_plugin.la
if HAVE_XCB
vout_LTLIBRARIES += libvk_x11_plugin.la
endif
if HAVE_WIN32_DESKTOP
vout_LTLIBRARIES += libvk_win32_plugin.la
endif
if HAVE_ANDROID
vout_LTLIBRARIES += libvk_android_plugin.la
endif
endif

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

libegl_x11_plugin_la_SOURCES = video_output/opengl/egl.c
libegl_x11_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_PLATFORM_X11=1
libegl_x11_plugin_la_CFLAGS = $(AM_CFLAGS) $(EGL_CFLAGS)
libegl_x11_plugin_la_LIBADD = $(EGL_LIBS) $(X_LIBS) $(X_PRE_LIBS) -lX11

libglx_plugin_la_SOURCES = video_output/glx.c
libglx_plugin_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS)
libglx_plugin_la_LIBADD = $(GL_LIBS) $(X_LIBS) $(X_PRE_LIBS) -lX11

if HAVE_XCB
pkglib_LTLIBRARIES += libvlc_xcb_events.la
vout_LTLIBRARIES += libxcb_x11_plugin.la libxcb_render_plugin.la libxcb_window_plugin.la
if HAVE_XKBCOMMON_X11
libxcb_window_plugin_la_SOURCES += \
	video_output/xcb/keysym.h video_output/xcb/xcb_keysym.h \
	video_output/xcb/vlc_xkb.h video_output/xcb/xkb.c
libxcb_window_plugin_la_CFLAGS += -DHAVE_XKBCOMMON
endif
if HAVE_EGL
vout_LTLIBRARIES += libegl_x11_plugin.la
endif
if HAVE_GL
vout_LTLIBRARIES += libglx_plugin.la
endif
endif


### Wayland ###
libwl_shm_plugin_la_SOURCES = video_output/wayland/shm.c
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


### Win32 ###
libdirect3d9_plugin_la_SOURCES = video_output/win32/direct3d9.c \
	video_output/win32/common.c video_output/win32/common.h \
	video_output/win32/events.c video_output/win32/events.h \
	video_output/win32/builtin_shaders.h \
	video_output/win32/win32touch.c video_output/win32/win32touch.h
libdirect3d9_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_direct3d9
libdirect3d9_plugin_la_LIBADD = libchroma_copy.la libd3d9_common.la -lgdi32 $(LIBCOM) -luuid
libdirect3d9_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
if HAVE_WIN32_DESKTOP
vout_LTLIBRARIES += $(LTLIBdirect3d9)
EXTRA_LTLIBRARIES += libdirect3d9_plugin.la
endif

libdirect3d11_plugin_la_SOURCES = video_output/win32/direct3d11.c \
 video_output/win32/d3d11_quad.c video_output/win32/d3d11_quad.h \
 video_output/win32/d3d11_shaders.c video_output/win32/d3d11_shaders.h \
 video_output/win32/common.c video_output/win32/common.h
libdirect3d11_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
 -DMODULE_NAME_IS_direct3d11
libdirect3d11_plugin_la_LIBADD = libchroma_copy.la libd3d11_common.la $(LIBCOM) -luuid
if !HAVE_WINSTORE
libdirect3d11_plugin_la_SOURCES += video_output/win32/events.c \
 video_output/win32/events.h \
 video_output/win32/sensors.cpp \
 video_output/win32/win32touch.c video_output/win32/win32touch.h
libdirect3d11_plugin_la_LIBADD += -lgdi32
else
libdirect3d11_plugin_la_LIBADD += -ld3d11
libdirect3d11_plugin_la_LIBADD += -ld3dcompiler_47
endif
libdirect3d11_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
vout_LTLIBRARIES += $(LTLIBdirect3d11)
EXTRA_LTLIBRARIES += libdirect3d11_plugin.la

libglwin32_plugin_la_SOURCES = $(OPENGL_COMMONSOURCES) \
	video_output/win32/glwin32.c \
	video_output/win32/common.c video_output/win32/common.h \
	video_output/win32/events.c video_output/win32/events.h \
	video_output/win32/sensors.cpp \
	video_output/win32/win32touch.c video_output/win32/win32touch.h
libwgl_plugin_la_SOURCES = video_output/win32/wgl.c $(OPENGL_COMMONSOURCES)

libglwin32_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_glwin32

libglwin32_plugin_la_LIBADD = libchroma_copy.la -lopengl32 -lgdi32 $(LIBCOM) -luuid $(OPENGL_COMMONLIBS)
libwgl_plugin_la_LIBADD = -lopengl32 -lgdi32 $(OPENGL_COMMONLIBS)

libglwin32_plugin_la_CFLAGS = $(AM_CFLAGS) $(OPENGL_COMMONCFLAGS) -DHAVE_GL_CORE_SYMBOLS
libwgl_plugin_la_CFLAGS = $(AM_CFLAGS) $(OPENGL_COMMONCFLAGS)

libglwin32_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
libwgl_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'

if HAVE_WIN32_DESKTOP
vout_LTLIBRARIES += $(LTLIBglwin32) $(LTLIBwgl)
EXTRA_LTLIBRARIES += libglwin32_plugin.la libwgl_plugin.la
endif

libwingdi_plugin_la_SOURCES = video_output/win32/wingdi.c \
	video_output/win32/common.c video_output/win32/common.h \
	video_output/win32/events.c video_output/win32/events.h \
	video_output/win32/win32touch.c video_output/win32/win32touch.h
libwingdi_plugin_la_LIBADD = libchroma_copy.la -lgdi32 $(LIBCOM) -luuid
if HAVE_WIN32_DESKTOP
vout_LTLIBRARIES += libwingdi_plugin.la
endif

libwinhibit_plugin_la_SOURCES = video_output/win32/inhibit.c
if HAVE_WIN32_DESKTOP
vout_LTLIBRARIES += libwinhibit_plugin.la
endif

libegl_win32_plugin_la_SOURCES = video_output/opengl/egl.c
libegl_win32_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_PLATFORM_WIN32=1
libegl_win32_plugin_la_CFLAGS = $(AM_CFLAGS) $(EGL_CFLAGS)
libegl_win32_plugin_la_LIBADD = $(EGL_LIBS)
if HAVE_WIN32
if HAVE_EGL
vout_LTLIBRARIES += libegl_win32_plugin.la
endif
endif

libdrawable_plugin_la_SOURCES = video_output/drawable.c
if HAVE_WIN32
vout_LTLIBRARIES += libdrawable_plugin.la
endif

libwin32_window_plugin_la_SOURCES = video_output/win32/window.c
if HAVE_WIN32_DESKTOP
vout_LTLIBRARIES += libwin32_window_plugin.la
endif


### OS/2 ###
if HAVE_OS2
vout_LTLIBRARIES += libdrawable_plugin.la
endif

libkva_plugin_la_SOURCES = video_output/kva.c
libkva_plugin_la_LIBADD = $(KVA_LIBS)
if HAVE_KVA
vout_LTLIBRARIES += libkva_plugin.la
endif


### Android ###

libegl_android_plugin_la_SOURCES = video_output/opengl/egl.c
libegl_android_plugin_la_CFLAGS = $(AM_CFLAGS) $(EGL_CFLAGS) -DUSE_PLATFORM_ANDROID=1
libegl_android_plugin_la_LIBADD = $(EGL_LIBS)

libandroid_window_plugin_la_SOURCES = video_output/android/window.c \
	video_output/android/utils.c video_output/android/utils.h
libandroid_window_plugin_la_CFLAGS = $(AM_CFLAGS)
libandroid_window_plugin_la_LIBADD = $(LIBDL)

libandroid_display_plugin_la_SOURCES = video_output/android/display.c \
	video_output/android/display.h \
	video_output/android/utils.c video_output/android/utils.h
libandroid_display_plugin_la_CFLAGS = $(AM_CFLAGS)
libandroid_display_plugin_la_LIBADD = $(LIBDL)

libglconv_android_plugin_la_SOURCES = video_output/opengl/converter_android.c \
	video_output/opengl/converter.h
libglconv_android_plugin_la_CFLAGS = $(AM_CFLAGS) -DUSE_OPENGL_ES2

if HAVE_ANDROID
vout_LTLIBRARIES += libandroid_window_plugin.la libandroid_display_plugin.la
if HAVE_EGL
vout_LTLIBRARIES += libegl_android_plugin.la libglconv_android_plugin.la
endif
endif


### FrameBuffer ###

libfb_plugin_la_SOURCES = video_output/fb.c
libfb_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
EXTRA_LTLIBRARIES += libfb_plugin.la
vout_LTLIBRARIES += $(LTLIBfb)


### Kernel Mode Setting ###

libkms_plugin_la_SOURCES = video_output/kms.c
libkms_plugin_la_CFLAGS = $(AM_CFLAGS) $(KMS_CFLAGS)
libkms_plugin_la_LIBADD = $(KMS_LIBS)
libkms_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
EXTRA_LTLIBRARIES += libkms_plugin.la
vout_LTLIBRARIES += $(LTLIBkms)


### Coloured ASCII art ###
libcaca_plugin_la_SOURCES = video_output/caca.c
libcaca_plugin_la_CFLAGS = $(AM_CFLAGS) $(CACA_CFLAGS)
libcaca_plugin_la_LIBADD = $(CACA_LIBS)
if !HAVE_WIN32
if !HAVE_DARWIN
libcaca_plugin_la_LIBADD += $(X_LIBS) $(X_PRE_LIBS) -lX11
endif
endif
libcaca_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
EXTRA_LTLIBRARIES += libcaca_plugin.la
vout_LTLIBRARIES += $(LTLIBcaca)

### Common ###

libflaschen_plugin_la_SOURCES = video_output/flaschen.c
libflaschen_plugin_la_LIBADD = $(SOCKET_LIBS)

libvdummy_plugin_la_SOURCES = video_output/vdummy.c
libvideo_splitter_plugin_la_SOURCES = video_output/splitter.c
libvmem_plugin_la_SOURCES = video_output/vmem.c
libwdummy_plugin_la_SOURCES = video_output/wdummy.c
libyuv_plugin_la_SOURCES = video_output/yuv.c
libvgl_plugin_la_SOURCES = video_output/vgl.c

vout_LTLIBRARIES += \
	libflaschen_plugin.la \
	libvdummy_plugin.la \
	libvideo_splitter_plugin.la \
	libvmem_plugin.la \
	libwdummy_plugin.la \
	libvgl_plugin.la \
	libyuv_plugin.la
