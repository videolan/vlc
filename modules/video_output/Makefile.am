voutdir = $(pluginsdir)/video_output
vout_LTLIBRARIES =

EXTRA_DIST += video_output/README

include video_output/opengl/Makefile.am

if HAVE_DECKLINK
libdecklinkoutput_plugin_la_SOURCES = video_output/decklink.cpp \
                                      stream_out/sdi/Ancillary.cpp \
                                      stream_out/sdi/Ancillary.hpp \
                                      stream_out/sdi/DBMHelper.cpp \
                                      stream_out/sdi/DBMHelper.hpp \
                                      stream_out/sdi/SDIGenerator.cpp \
                                      stream_out/sdi/SDIGenerator.hpp \
                                      stream_out/sdi/V210.cpp \
                                      stream_out/sdi/V210.hpp
libdecklinkoutput_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(CPPFLAGS_decklinkoutput)
libdecklinkoutput_plugin_la_LIBADD = $(LIBS_decklink) $(LIBDL) -lpthread
vout_LTLIBRARIES += libdecklinkoutput_plugin.la
endif

libglinterop_cvpx_plugin_la_SOURCES = video_output/opengl/interop_cvpx.c \
	video_output/opengl/interop.h
libglinterop_cvpx_plugin_la_SOURCES += codec/vt_utils.c codec/vt_utils.h
libglinterop_cvpx_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)' \
	-Wl,-framework,Foundation,-framework,CoreVideo

if HAVE_OSX
libvout_macosx_plugin_la_SOURCES = video_output/macosx.m
libvout_macosx_plugin_la_CFLAGS = $(AM_CFLAGS) -DHAVE_GL_CORE_SYMBOLS
libvout_macosx_plugin_la_LIBADD = libvlc_opengl.la
libvout_macosx_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)' \
	-Wl,-framework,OpenGL,-framework,Cocoa

libcaopengllayer_plugin_la_SOURCES = video_output/caopengllayer.m
libcaopengllayer_plugin_la_CFLAGS = $(AM_CFLAGS)  -DHAVE_GL_CORE_SYMBOLS
libcaopengllayer_plugin_la_LIBADD = libvlc_opengl.la
libcaopengllayer_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)' \
	-Wl,-framework,OpenGL,-framework,Cocoa,-framework,QuartzCore

libglinterop_cvpx_plugin_la_LDFLAGS += -Wl,-framework,IOSurface,-framework,OpenGL
vout_LTLIBRARIES += libvout_macosx_plugin.la libcaopengllayer_plugin.la \
	libglinterop_cvpx_plugin.la
endif
if HAVE_IOS
libglinterop_cvpx_plugin_la_CFLAGS = $(AM_CFLAGS) -DUSE_OPENGL_ES2
endif

libvout_ios_plugin_la_SOURCES = video_output/ios.m
libvout_ios_plugin_la_CFLAGS = $(AM_CFLAGS) $(OPENGL_COMMONCFLAGS) -DUSE_OPENGL_ES2
libvout_ios_plugin_la_LIBADD = libvlc_opengles.la
libvout_ios_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)' \
	-Wl,-framework,Foundation,-framework,OpenGLES,-framework,QuartzCore,-framework,UIKit
if HAVE_IOS
vout_LTLIBRARIES += libvout_ios_plugin.la libglinterop_cvpx_plugin.la
endif
if HAVE_TVOS
vout_LTLIBRARIES += libvout_ios_plugin.la libglinterop_cvpx_plugin.la
endif

libglinterop_vaapi_plugin_la_SOURCES = video_output/opengl/interop_vaapi.c \
	video_output/opengl/interop.h \
	hw/vaapi/vlc_vaapi.c hw/vaapi/vlc_vaapi.h
libglinterop_vaapi_plugin_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS)
libglinterop_vaapi_plugin_la_LIBADD = $(LIBVA_LIBS) $(LIBVA_EGL_LIBS)

libglinterop_vdpau_plugin_la_SOURCES = video_output/opengl/interop_vdpau.c \
	video_output/opengl/interop.h hw/vdpau/picture.c hw/vdpau/vlc_vdpau.h
libglinterop_vdpau_plugin_la_CFLAGS = $(AM_CFLAGS) $(VDPAU_CFLAGS)
libglinterop_vdpau_plugin_la_LIBADD = $(LIBDL) libvlc_vdpau.la $(X_LIBS) $(X_PRE_LIBS) -lX11

if HAVE_GL
if HAVE_EGL
if HAVE_VAAPI
vout_LTLIBRARIES += libglinterop_vaapi_plugin.la
endif
endif # HAVE_EGL

if HAVE_VDPAU
vout_LTLIBRARIES += libglinterop_vdpau_plugin.la
endif
endif # HAVE_GL

### X11 ###
include video_output/xcb/Makefile.am

### Vulkan ###
include video_output/vulkan/Makefile.am

### Wayland ###
include video_output/wayland/Makefile.am


### Win32 ###
libdirect3d9_plugin_la_SOURCES = video_output/win32/direct3d9.c \
	video_output/win32/common.c video_output/win32/common.h \
	video_output/win32/events.c video_output/win32/events.h \
	video_output/win32/builtin_shaders.h \
	video_output/win32/win32touch.c video_output/win32/win32touch.h
libdirect3d9_plugin_la_LIBADD = libchroma_copy.la libd3d9_common.la -lgdi32 $(LIBCOM) -luuid
libdirect3d9_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'

libglinterop_dxva2_plugin_la_SOURCES = video_output/opengl/interop_dxva2.c \
	video_output/opengl/interop.h
libglinterop_dxva2_plugin_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS)
libglinterop_dxva2_plugin_la_LIBADD = libd3d9_common.la

if HAVE_WIN32_DESKTOP
vout_LTLIBRARIES += $(LTLIBdirect3d9)
EXTRA_LTLIBRARIES += libdirect3d9_plugin.la
if HAVE_GL
vout_LTLIBRARIES += libglinterop_dxva2_plugin.la
endif
endif

libdirect3d11_plugin_la_SOURCES = video_output/win32/direct3d11.c \
 video_output/win32/d3d11_quad.c video_output/win32/d3d11_quad.h \
 video_output/win32/d3d11_shaders.c video_output/win32/d3d11_shaders.h \
 video_output/win32/d3d11_swapchain.c video_output/win32/d3d11_swapchain.h \
 video_output/win32/common.c video_output/win32/common.h
libdirect3d11_plugin_la_LIBADD = libchroma_copy.la libd3d11_common.la $(LIBCOM) -luuid
if HAVE_DCOMP
libdirect3d11_plugin_la_SOURCES += video_output/win32/dcomp_wrapper.cpp video_output/win32/dcomp_wrapper.h
endif
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

libglwin32_plugin_la_SOURCES = \
	video_output/win32/glwin32.c \
	video_output/win32/common.c video_output/win32/common.h \
	video_output/win32/events.c video_output/win32/events.h \
	video_output/win32/sensors.cpp \
	video_output/win32/win32touch.c video_output/win32/win32touch.h
libwgl_plugin_la_SOURCES = video_output/win32/wgl.c

libglwin32_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DMODULE_NAME_IS_glwin32

libglwin32_plugin_la_LIBADD = libchroma_copy.la -lopengl32 -lgdi32 $(LIBCOM) -luuid libvlc_opengl.la
libwgl_plugin_la_LIBADD = -lopengl32 -lgdi32 libvlc_opengl.la

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

libdrawable_plugin_la_SOURCES = video_output/win32/drawable.c
if HAVE_WIN32_DESKTOP
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
	video_output/android/utils.c video_output/android/utils.h
libandroid_display_plugin_la_CFLAGS = $(AM_CFLAGS)
libandroid_display_plugin_la_LIBADD = $(LIBDL)

libglinterop_android_plugin_la_SOURCES = video_output/opengl/interop_android.c \
	video_output/opengl/interop.h
libglinterop_android_plugin_la_CFLAGS = $(AM_CFLAGS) -DUSE_OPENGL_ES2

if HAVE_ANDROID
vout_LTLIBRARIES += libandroid_window_plugin.la libandroid_display_plugin.la
if HAVE_EGL
vout_LTLIBRARIES += libegl_android_plugin.la libglinterop_android_plugin.la
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
libwextern_plugin_la_SOURCES = video_output/wextern.c
libyuv_plugin_la_SOURCES = video_output/yuv.c
libvgl_plugin_la_SOURCES = video_output/vgl.c

vout_LTLIBRARIES += \
	libflaschen_plugin.la \
	libvdummy_plugin.la \
	libvideo_splitter_plugin.la \
	libvmem_plugin.la \
	libwdummy_plugin.la \
	libwextern_plugin.la \
	libvgl_plugin.la \
	libyuv_plugin.la
