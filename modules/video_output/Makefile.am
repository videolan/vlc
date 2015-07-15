voutdir = $(pluginsdir)/video_output
vout_LTLIBRARIES =

EXTRA_DIST += video_output/README

if HAVE_DECKLINK
libdecklinkoutput_plugin_la_SOURCES = video_output/decklink.cpp
libdecklinkoutput_plugin_la_CXXFLAGS = $(AM_CFLAGS) $(CPPFLAGS_decklinkoutput)
libdecklinkoutput_plugin_la_LIBADD = $(LIBS_decklink) $(LIBDL) -lpthread
vout_LTLIBRARIES += libdecklinkoutput_plugin.la
endif

if HAVE_OSX
libvout_macosx_plugin_la_SOURCES = video_output/macosx.m video_output/opengl.c video_output/opengl.h
libvout_macosx_plugin_la_CFLAGS = $(AM_CFLAGS)
libvout_macosx_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)' -Wl,-framework,OpenGL,-framework,Cocoa

libcaopengllayer_plugin_la_SOURCES = video_output/caopengllayer.m video_output/opengl.c video_output/opengl.h
libcaopengllayer_plugin_la_CFLAGS = $(AM_CFLAGS)
libcaopengllayer_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)' -Wl,-framework,OpenGL,-framework,Cocoa,-framework,QuartzCore
vout_LTLIBRARIES += libvout_macosx_plugin.la libcaopengllayer_plugin.la
endif

if HAVE_IOS
libvout_ios2_plugin_la_SOURCES = video_output/ios2.m video_output/opengl.h video_output/opengl.c
libvout_ios2_plugin_la_CFLAGS = $(AM_CFLAGS)
libvout_ios2_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)' -Wl,-framework,OpenGLES,-framework,QuartzCore,-framework,UIKit
vout_LTLIBRARIES += libvout_ios2_plugin.la
endif

### OpenGL ###
# TODO: merge all three source files (?)
libgles2_plugin_la_SOURCES = video_output/opengl.c video_output/opengl.h \
	video_output/gl.c
libgles2_plugin_la_CFLAGS = $(AM_CFLAGS) $(GLES2_CFLAGS) -DUSE_OPENGL_ES=2
libgles2_plugin_la_LIBADD = $(GLES2_LIBS)
libgles2_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'

libgles1_plugin_la_SOURCES = video_output/opengl.c video_output/opengl.h \
	video_output/gl.c
libgles1_plugin_la_CFLAGS = $(AM_CFLAGS) $(GLES1_CFLAGS) -DUSE_OPENGL_ES=1
libgles1_plugin_la_LIBADD = $(GLES1_LIBS)
libgles1_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'

EXTRA_LTLIBRARIES += libgles2_plugin.la libgles1_plugin.la
vout_LTLIBRARIES += $(LTLIBgles2) $(LTLIBgles1)

libgl_plugin_la_SOURCES = video_output/opengl.c video_output/opengl.h \
	video_output/gl.c
libgl_plugin_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS)
libgl_plugin_la_LIBADD = $(GL_LIBS)
if HAVE_GL
vout_LTLIBRARIES += libgl_plugin.la
endif


### XCB ###
libvlc_xcb_events_la_SOURCES = \
	video_output/xcb/events.c video_output/xcb/events.h
libvlc_xcb_events_la_CPPFLAGS = -DMODULE_STRING=\"xcb\"
libvlc_xcb_events_la_CFLAGS = $(AM_CFLAGS) $(XCB_CFLAGS)
libvlc_xcb_events_la_LDFLAGS = -static
libvlc_xcb_events_la_LIBADD = $(XCB_LIBS)

libxcb_x11_plugin_la_SOURCES = \
	video_output/xcb/pictures.c video_output/xcb/pictures.h \
	video_output/xcb/x11.c
libxcb_x11_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(XCB_CFLAGS) $(XCB_SHM_CFLAGS)
libxcb_x11_plugin_la_LIBADD = libvlc_xcb_events.la \
	$(XCB_LIBS) $(XCB_SHM_LIBS)

libxcb_xv_plugin_la_SOURCES = \
	video_output/xcb/pictures.c video_output/xcb/pictures.h \
	video_output/xcb/xvideo.c
libxcb_xv_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(XCB_CFLAGS) $(XCB_SHM_CFLAGS) $(XCB_XV_CFLAGS)
libxcb_xv_plugin_la_LIBADD = libvlc_xcb_events.la \
	$(XCB_LIBS) $(XCB_SHM_LIBS) $(XCB_XV_LIBS)

libxcb_glx_plugin_la_SOURCES = \
	video_output/opengl.c video_output/opengl.h \
	video_output/xcb/glx.c
libxcb_glx_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(XCB_CFLAGS) $(GL_CFLAGS)
libxcb_glx_plugin_la_LIBADD = libvlc_xcb_events.la \
	$(XCB_LIBS) $(GL_LIBS)

libxcb_window_plugin_la_SOURCES =  \
	video_output/xcb/keys.c \
	video_output/xcb/keysym.h video_output/xcb/xcb_keysym.h \
	video_output/xcb/window.c
libxcb_window_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(CFLAGS_xcb_window) \
	$(XPROTO_CFLAGS) $(XCB_CFLAGS) $(XCB_KEYSYMS_CFLAGS)
libxcb_window_plugin_la_LIBADD = $(XPROTO_LIBS) $(XCB_LIBS) $(XCB_KEYSYMS_LIBS)

libegl_x11_plugin_la_SOURCES = video_output/egl.c
libegl_x11_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_PLATFORM_X11=1
libegl_x11_plugin_la_CFLAGS = $(AM_CFLAGS) $(EGL_CFLAGS)
libegl_x11_plugin_la_LIBADD = $(EGL_LIBS) $(X_LIBS) $(X_PRE_LIBS) -lX11

libglx_plugin_la_SOURCES = video_output/glx.c
libglx_plugin_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS)
libglx_plugin_la_LIBADD = $(GL_LIBS) $(X_LIBS) $(X_PRE_LIBS) -lX11

if HAVE_XCB
noinst_LTLIBRARIES += libvlc_xcb_events.la
vout_LTLIBRARIES += libxcb_x11_plugin.la libxcb_window_plugin.la
if HAVE_XCB_KEYSYMS
libxcb_window_plugin_la_CFLAGS += -DHAVE_XCB_KEYSYMS
endif
if HAVE_XCB_XVIDEO
vout_LTLIBRARIES += libxcb_xv_plugin.la
endif
if HAVE_EGL
vout_LTLIBRARIES += libegl_x11_plugin.la
endif
if HAVE_GL
vout_LTLIBRARIES += libxcb_glx_plugin.la libglx_plugin.la
endif
endif


### Wayland ###
libwl_shm_plugin_la_SOURCES = video_output/wayland/shm.c
nodist_libwl_shm_plugin_la_SOURCES = video_output/wayland/scaler-protocol.c
libwl_shm_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-I$(builddir)/video_output/wayland
libwl_shm_plugin_la_CFLAGS = $(WAYLAND_CLIENT_CFLAGS)
libwl_shm_plugin_la_LIBADD = $(WAYLAND_CLIENT_LIBS)
EXTRA_DIST += video_output/wayland/scaler.xml
CLEANFILES += $(nodist_libwl_shm_plugin_la_SOURCES)

libwl_shell_surface_plugin_la_SOURCES = video_output/wayland/shell_surface.c
libwl_shell_surface_plugin_la_CFLAGS = $(WAYLAND_CLIENT_CFLAGS)
libwl_shell_surface_plugin_la_LIBADD = $(WAYLAND_CLIENT_LIBS) $(LIBPTHREAD)

libegl_wl_plugin_la_SOURCES = video_output/egl.c
libegl_wl_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_PLATFORM_WAYLAND=1
libegl_wl_plugin_la_CFLAGS = $(AM_CFLAGS) $(EGL_CFLAGS) $(WAYLAND_EGL_CFLAGS)
libegl_wl_plugin_la_LIBADD = $(EGL_LIBS) $(WAYLAND_EGL_LIBS)

if HAVE_WAYLAND
BUILT_SOURCES += video_output/wayland/scaler-client-protocol.h
vout_LTLIBRARIES += libwl_shm_plugin.la
vout_LTLIBRARIES += libwl_shell_surface_plugin.la
if HAVE_WAYLAND_EGL
if HAVE_EGL
vout_LTLIBRARIES += libegl_wl_plugin.la
endif
endif
endif


### Win32 ###
libdirect2d_plugin_la_SOURCES = video_output/msw/direct2d.c \
	video_output/msw/common.c video_output/msw/common.h \
	video_output/msw/events.c video_output/msw/events.h \
	video_output/msw/win32touch.c video_output/msw/win32touch.h
libdirect2d_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_direct2d
libdirect2d_plugin_la_LIBADD = -lgdi32 -lole32 -luuid
libdirect2d_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
vout_LTLIBRARIES += $(LTLIBdirect2d)
EXTRA_LTLIBRARIES += libdirect2d_plugin.la

libdirect3d9_plugin_la_SOURCES = video_output/msw/direct3d9.c \
	video_output/msw/common.c video_output/msw/common.h \
	video_output/msw/events.c video_output/msw/events.h \
	video_output/msw/builtin_shaders.h \
	video_output/msw/win32touch.c video_output/msw/win32touch.h
libdirect3d9_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_direct3d9
libdirect3d9_plugin_la_LIBADD = -lgdi32 -lole32 -luuid
libdirect3d9_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
vout_LTLIBRARIES += $(LTLIBdirect3d9)
EXTRA_LTLIBRARIES += libdirect3d9_plugin.la

libdirect3d11_plugin_la_SOURCES = video_output/msw/direct3d11.c \
 video_output/msw/common.c video_output/msw/common.h
libdirect3d11_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
 -DMODULE_NAME_IS_direct3d11
if !HAVE_WINSTORE
libdirect3d11_plugin_la_SOURCES += video_output/msw/events.c \
 video_output/msw/events.h \
 video_output/msw/win32touch.c video_output/msw/win32touch.h
libdirect3d11_plugin_la_LIBADD = -lgdi32 -lole32 -luuid
else
libdirect3d11_plugin_la_LIBADD = -ld3dcompiler -lole32 -luuid
endif
libdirect3d11_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
vout_LTLIBRARIES += $(LTLIBdirect3d11)
EXTRA_LTLIBRARIES += libdirect3d11_plugin.la

libdirectdraw_plugin_la_SOURCES = video_output/msw/directdraw.c \
	video_output/msw/common.c video_output/msw/common.h \
	video_output/msw/events.c video_output/msw/events.h \
	video_output/msw/win32touch.c video_output/msw/win32touch.h
libdirectdraw_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_directdraw
libdirectdraw_plugin_la_LIBADD = -luser32 -lgdi32 -lole32 -luuid
if HAVE_DIRECTX
vout_LTLIBRARIES += libdirectdraw_plugin.la
endif

libglwin32_plugin_la_SOURCES = video_output/msw/glwin32.c \
	video_output/opengl.c video_output/opengl.h \
	video_output/msw/common.c video_output/msw/common.h \
	video_output/msw/events.c video_output/msw/events.h \
	video_output/msw/win32touch.c video_output/msw/win32touch.h
libglwin32_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_glwin32
libglwin32_plugin_la_LIBADD = -lopengl32 -lgdi32 -lole32 -luuid
libglwin32_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
vout_LTLIBRARIES += $(LTLIBglwin32)
EXTRA_LTLIBRARIES += libglwin32_plugin.la

libwingdi_plugin_la_SOURCES = video_output/msw/wingdi.c \
	video_output/msw/common.c video_output/msw/common.h \
	video_output/msw/events.c video_output/msw/events.h \
	video_output/msw/win32touch.c video_output/msw/win32touch.h
libwingdi_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_wingdi
libwingdi_plugin_la_LIBADD = -lgdi32 -lole32 -luuid
if HAVE_WIN32
if !HAVE_WINSTORE
vout_LTLIBRARIES += libwingdi_plugin.la
endif
endif

libegl_win32_plugin_la_SOURCES = video_output/egl.c
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

libegl_android_plugin_la_SOURCES = video_output/egl.c
libegl_android_plugin_la_CFLAGS = $(AM_CFLAGS) $(EGL_CFLAGS) -DUSE_PLATFORM_ANDROID=1
libegl_android_plugin_la_LIBADD = $(EGL_LIBS)

libandroid_native_window_plugin_la_SOURCES = video_output/android/nativewindow.c \
	video_output/android/utils.c video_output/android/utils.h
libandroid_native_window_plugin_la_CFLAGS = $(AM_CFLAGS)
libandroid_native_window_plugin_la_LIBADD = $(LIBDL)

libandroid_window_plugin_la_SOURCES = video_output/android/android_window.c video_output/android/android_window.h video_output/android/utils.c video_output/android/utils.h
libandroid_window_plugin_la_CFLAGS = $(AM_CFLAGS)
libandroid_window_plugin_la_LIBADD = $(LIBDL)
libandroid_window_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'

if HAVE_ANDROID
vout_LTLIBRARIES += libandroid_native_window_plugin.la libandroid_window_plugin.la
vout_LTLIBRARIES += $(LTLIBandroid_window)
if HAVE_EGL
vout_LTLIBRARIES += libegl_android_plugin.la
endif
endif


### FrameBuffer ###

libdirectfb_plugin_la_SOURCES = video_output/directfb.c
libdirectfb_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_directfb)
libdirectfb_plugin_la_LIBADD = $(LIBS_directfb)
libdirectfb_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
EXTRA_LTLIBRARIES += libdirectfb_plugin.la
vout_LTLIBRARIES += $(LTLIBdirectfb)

libfb_plugin_la_SOURCES = video_output/fb.c
libfb_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
EXTRA_LTLIBRARIES += libfb_plugin.la
vout_LTLIBRARIES += $(LTLIBfb)


### Simple DirectMedia Layer ###
libvout_sdl_plugin_la_SOURCES = video_output/sdl.c
libvout_sdl_plugin_la_CFLAGS = $(AM_CFLAGS) $(SDL_CFLAGS)
libvout_sdl_plugin_la_LIBADD = $(SDL_LIBS)
if !HAVE_WIN32
if !HAVE_OS2
libvout_sdl_plugin_la_LIBADD += $(X_LIBS) $(X_PRE_LIBS) -lX11
endif
endif
libvout_sdl_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
EXTRA_LTLIBRARIES += libvout_sdl_plugin.la
vout_LTLIBRARIES += $(LTLIBvout_sdl)



### ASCII Art ###
libaa_plugin_la_SOURCES = video_output/aa.c
libaa_plugin_la_LIBADD = -laa
if !HAVE_WIN32
libaa_plugin_la_LIBADD += $(X_LIBS) $(X_PRE_LIBS) -lX11
endif
libaa_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
EXTRA_LTLIBRARIES += libaa_plugin.la
vout_LTLIBRARIES += $(LTLIBaa)


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
libvdummy_plugin_la_SOURCES = video_output/vdummy.c
libvmem_plugin_la_SOURCES = video_output/vmem.c
libyuv_plugin_la_SOURCES = video_output/yuv.c

vout_LTLIBRARIES += \
	libvdummy_plugin.la \
	libvmem_plugin.la \
	libyuv_plugin.la
