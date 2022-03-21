voutdir = $(pluginsdir)/video_output
vout_PLUGINS =
vout_LTLIBRARIES =
vout_RPATH =
if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(vout_PLUGINS)
else
vout_LTLIBRARIES += $(vout_PLUGINS)
vout_RPATH += -rpath '$(voutdir)'
endif

EXTRA_DIST += video_output/README

### libplacebo ###
if HAVE_LIBPLACEBO
include video_output/libplacebo/Makefile.am
endif

### OpenGL ###
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
libdecklinkoutput_plugin_la_LIBADD = $(LIBS_decklinkoutput)
if HAVE_WIN32
libdecklinkoutput_plugin_la_LIBADD += $(LIBCOM)
libdecklinkoutput_plugin_la_CXXFLAGS += $(LIBCOMCXXFLAGS)
endif
if HAVE_LINUX
libdecklinkoutput_plugin_la_LIBADD += $(LIBDL)
endif
vout_PLUGINS += libdecklinkoutput_plugin.la
endif

libglinterop_cvpx_plugin_la_SOURCES = video_output/opengl/interop_cvpx.m \
	video_output/opengl/interop.h
libglinterop_cvpx_plugin_la_LIBADD = libvlc_vtutils.la
libglinterop_cvpx_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(vout_RPATH) \
	-Wl,-framework,Foundation,-framework,CoreVideo,-framework,IOSurface
libglinterop_cvpx_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
if HAVE_DARWIN
if HAVE_OSX
libglinterop_cvpx_plugin_la_LDFLAGS += -Wl,-framework,OpenGL
else
libglinterop_cvpx_plugin_la_CPPFLAGS += -DUSE_OPENGL_ES2
libglinterop_cvpx_plugin_la_LDFLAGS += -Wl,-framework,IOSurface,-framework,OpenGLES
endif
vout_PLUGINS += libglinterop_cvpx_plugin.la
endif

if HAVE_OSX
libwindow_macosx_plugin_la_SOURCES = video_output/window_macosx.m
libwindow_macosx_plugin_la_LDFLAGS = $(AM_LDFLAGS) \
	-Wl,-framework,Cocoa
libwindow_macosx_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) \
	-fobjc-arc -fobjc-exceptions

libvout_macosx_plugin_la_SOURCES = video_output/macosx.m \
	$(OPENGL_VOUT_COMMONSOURCES)
libvout_macosx_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DHAVE_GL_CORE_SYMBOLS
libvout_macosx_plugin_la_LIBADD = libvlc_opengl.la
libvout_macosx_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(vout_RPATH) \
	-Wl,-framework,OpenGL,-framework,Cocoa

libcaopengllayer_plugin_la_SOURCES = video_output/caopengllayer.m \
	$(OPENGL_VOUT_COMMONSOURCES)
libcaopengllayer_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)  -DHAVE_GL_CORE_SYMBOLS
libcaopengllayer_plugin_la_LIBADD = libvlc_opengl.la
libcaopengllayer_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(vout_RPATH) \
	-Wl,-framework,OpenGL,-framework,Cocoa,-framework,QuartzCore

vout_PLUGINS += libvout_macosx_plugin.la libcaopengllayer_plugin.la libwindow_macosx_plugin.la
endif

libvout_ios_plugin_la_SOURCES = video_output/opengl/display.c $(OPENGL_VOUT_COMMONSOURCES)
libvout_ios_plugin_la_CFLAGS = $(AM_CFLAGS) $(OPENGL_COMMONCFLAGS) -DUSE_OPENGL_ES2
libvout_ios_plugin_la_LIBADD = libvlc_opengles.la

libuiview_window_plugin_la_SOURCES = video_output/apple/VLCVideoUIView.m
libuiview_window_plugin_la_LIBADD = ../src/libvlc_darwin.la
libuiview_window_plugin_la_LDFLAGS = $(AM_LDFLAGS) \
	-Wl,-framework,Foundation,-framework,QuartzCore,-framework,UIKit
libuiview_window_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -fobjc-arc

libcaeagl_ios_plugin_la_SOURCES = video_output/apple/VLCOpenGLES2VideoView.m
libcaeagl_ios_plugin_la_LIBADD = ../src/libvlc_darwin.la
libcaeagl_ios_plugin_la_LDFLAGS = $(AM_LDFLAGS) \
	-Wl,-framework,Foundation,-framework,OpenGLES,-framework,QuartzCore,-framework,UIKit
libcaeagl_ios_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -fobjc-arc

libcvpx_gl_plugin_la_SOURCES = video_output/apple/VLCCVOpenGLProvider.m
libcvpx_gl_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -fobjc-arc
libcvpx_gl_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DGL_SILENCE_DEPRECATION
libcvpx_gl_plugin_la_LDFLAGS = $(AM_LDFLAGS) \
    -Wl,-framework,Foundation,-framework,CoreFoundation,-framework,CoreVideo
libcvpx_gl_plugin_la_LIBADD = libvlc_vtutils.la

if HAVE_DARWIN
if HAVE_OSX
libcvpx_gl_plugin_la_LDFLAGS += -Wl,-framework,OpenGL
libcvpx_gl_plugin_la_LIBADD += libvlc_opengl.la
else
libcvpx_gl_plugin_la_CPPFLAGS += -DUSE_OPENGL_ES2
libcvpx_gl_plugin_la_LDFLAGS += -Wl,-framework,OpenGLES
libcvpx_gl_plugin_la_LIBADD += libvlc_opengles.la
endif
vout_PLUGINS += libcvpx_gl_plugin.la
endif

if HAVE_IOS
vout_PLUGINS += libvout_ios_plugin.la \
	libuiview_window_plugin.la \
	libcaeagl_ios_plugin.la
endif
if HAVE_TVOS
vout_PLUGINS += \
	libvout_ios_plugin.la \
	libuiview_window_plugin.la \
	libcaeagl_ios_plugin.la
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
vout_PLUGINS += libglinterop_vaapi_plugin.la
endif
endif # HAVE_EGL

if HAVE_VDPAU
vout_PLUGINS += libglinterop_vdpau_plugin.la
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
libdirect3d9_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(vout_RPATH)

libglinterop_dxva2_plugin_la_SOURCES = video_output/opengl/interop_dxva2.c \
	video_output/opengl/interop.h
libglinterop_dxva2_plugin_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS)
libglinterop_dxva2_plugin_la_LIBADD = libd3d9_common.la -lopengl32

if HAVE_WIN32_DESKTOP
vout_PLUGINS += $(LTLIBdirect3d9)
EXTRA_LTLIBRARIES += libdirect3d9_plugin.la
if HAVE_GL
vout_PLUGINS += libglinterop_dxva2_plugin.la
endif
endif

libdirect3d11_plugin_la_SOURCES = video_output/win32/direct3d11.cpp \
 video_output/win32/d3d11_quad.cpp video_output/win32/d3d11_quad.h \
 video_output/win32/d3d11_shaders.cpp video_output/win32/d3d11_shaders.h \
 video_output/win32/d3d_shaders.c video_output/win32/d3d_shaders.h \
 video_output/win32/d3d_dynamic_shader.c video_output/win32/d3d_dynamic_shader.h \
 video_output/win32/d3d11_swapchain.cpp video_output/win32/d3d11_swapchain.h \
 video_output/win32/dxgi_swapchain.cpp video_output/win32/dxgi_swapchain.h \
 video_output/win32/common.c video_output/win32/common.h
libdirect3d11_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(LIBCOMCXXFLAGS)
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
libdirect3d11_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(vout_RPATH)
vout_PLUGINS += $(LTLIBdirect3d11)
EXTRA_LTLIBRARIES += libdirect3d11_plugin.la

libglwin32_plugin_la_SOURCES = $(OPENGL_VOUT_COMMONSOURCES) \
	video_output/win32/glwin32.c \
	video_output/win32/common.c video_output/win32/common.h \
	video_output/win32/events.c video_output/win32/events.h \
	video_output/win32/sensors.cpp \
	video_output/win32/win32touch.c video_output/win32/win32touch.h
libwgl_plugin_la_SOURCES = video_output/win32/wgl.c

libglwin32_plugin_la_LIBADD = libchroma_copy.la -lopengl32 -lgdi32 $(LIBCOM) -luuid libvlc_opengl.la
libwgl_plugin_la_LIBADD = -lopengl32 -lgdi32 libvlc_opengl.la

libglwin32_plugin_la_CFLAGS = $(AM_CFLAGS) $(OPENGL_COMMONCFLAGS)
libwgl_plugin_la_CFLAGS = $(AM_CFLAGS) $(OPENGL_COMMONCFLAGS)

libglwin32_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(vout_RPATH)
libwgl_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(vout_RPATH)

if HAVE_WIN32_DESKTOP
vout_PLUGINS += $(LTLIBglwin32) $(LTLIBwgl)
EXTRA_LTLIBRARIES += libglwin32_plugin.la libwgl_plugin.la
endif

libwingdi_plugin_la_SOURCES = video_output/win32/wingdi.c \
	video_output/win32/common.c video_output/win32/common.h \
	video_output/win32/events.c video_output/win32/events.h \
	video_output/win32/win32touch.c video_output/win32/win32touch.h
libwingdi_plugin_la_LIBADD = libchroma_copy.la -lgdi32 $(LIBCOM) -luuid
if HAVE_WIN32_DESKTOP
vout_PLUGINS += libwingdi_plugin.la
endif

libwinhibit_plugin_la_SOURCES = video_output/win32/inhibit.c
if HAVE_WIN32_DESKTOP
vout_PLUGINS += libwinhibit_plugin.la
endif

libegl_win32_plugin_la_SOURCES = video_output/opengl/egl.c
libegl_win32_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_PLATFORM_WIN32=1
libegl_win32_plugin_la_CFLAGS = $(AM_CFLAGS) $(EGL_CFLAGS)
libegl_win32_plugin_la_LIBADD = $(EGL_LIBS)
if HAVE_WIN32
if HAVE_EGL
vout_PLUGINS += libegl_win32_plugin.la
endif
endif

libdrawable_plugin_la_SOURCES = video_output/win32/drawable.c \
	video_output/wasync_resize_compressor.h
if HAVE_WIN32_DESKTOP
vout_PLUGINS += libdrawable_plugin.la
endif

libwin32_window_plugin_la_SOURCES = video_output/win32/window.c \
	video_output/wasync_resize_compressor.h
libwin32_window_plugin_la_LIBADD = -lgdi32
if HAVE_WIN32_DESKTOP
vout_PLUGINS += libwin32_window_plugin.la
endif


### OS/2 ###
libdrawable_os2_plugin_la_SOURCES = video_output/drawable_os2.c
if HAVE_OS2
vout_PLUGINS += libdrawable_os2_plugin.la
endif

libwindow_os2_plugin_la_SOURCES = video_output/window_os2.c
if HAVE_OS2
vout_PLUGINS += libwindow_os2_plugin.la
endif

libkva_plugin_la_SOURCES = video_output/kva.c
libkva_plugin_la_LIBADD = $(KVA_LIBS)
if HAVE_KVA
vout_PLUGINS += libkva_plugin.la
endif


### Android ###
libandroid_utils_la_SOURCES = video_output/android/utils.c video_output/android/utils.h
libandroid_utils_la_CFLAGS = $(AM_CFLAGS) $(GLES2_CFLAGS) $(EGL_CFLAGS)
libandroid_utils_la_LIBADD = $(GLES2_LIBS) $(EGL_LIBS)
if HAVE_ANDROID
noinst_LTLIBRARIES += libandroid_utils.la
endif

libegl_android_plugin_la_SOURCES = video_output/opengl/egl.c
libegl_android_plugin_la_CFLAGS = $(AM_CFLAGS) $(EGL_CFLAGS) -DUSE_PLATFORM_ANDROID=1 -DUSE_OPENGL_ES2
libegl_android_plugin_la_LIBADD = $(EGL_LIBS)

libandroid_window_plugin_la_SOURCES = video_output/android/window.c \
	video_output/wasync_resize_compressor.h
libandroid_window_plugin_la_LIBADD = libandroid_utils.la $(LIBDL)

libandroid_display_plugin_la_SOURCES = video_output/android/display.c
libandroid_display_plugin_la_LIBADD = libandroid_utils.la $(LIBDL)

libglinterop_android_plugin_la_SOURCES = video_output/opengl/interop_android.c \
	video_output/opengl/interop.h
libglinterop_android_plugin_la_CFLAGS = $(AM_CFLAGS) -DUSE_OPENGL_ES2
libglinterop_android_plugin_la_LIBADD = libandroid_utils.la

if HAVE_ANDROID
vout_PLUGINS += libandroid_window_plugin.la libandroid_display_plugin.la
if HAVE_EGL
vout_PLUGINS += \
	libegl_android_plugin.la \
	libglinterop_android_plugin.la
endif
endif


### Direct Rendering Manager (DRM) ###
libkms_plugin_la_SOURCES = video_output/kms.c
libkms_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBDRM_CFLAGS)
libkms_plugin_la_LIBADD = $(LIBDRM_LIBS)

libdrm_display_plugin_la_SOURCES = \
	video_output/drm/vlc_drm.h \
	video_output/drm/fourcc.c \
	video_output/drm/buffers.c \
	video_output/drm/planes.c \
	video_output/drm/display.c
libdrm_display_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
if HAVE_DRM
vout_PLUGINS += libdrm_display_plugin.la
endif
if HAVE_LIBDRM
libdrm_display_plugin_la_CPPFLAGS += $(LIBDRM_CFLAGS) -DHAVE_LIBDRM
vout_PLUGINS += libkms_plugin.la libdrm_display_plugin.la
endif

### Coloured ASCII art ###
libcaca_plugin_la_SOURCES = video_output/caca.c
libcaca_plugin_la_CFLAGS = $(AM_CFLAGS) $(CACA_CFLAGS)
libcaca_plugin_la_LIBADD = $(CACA_LIBS)
if !HAVE_WIN32
if !HAVE_DARWIN
libcaca_plugin_la_LIBADD += $(X_LIBS) $(X_PRE_LIBS) -lX11
endif
endif
libcaca_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(vout_RPATH)
EXTRA_LTLIBRARIES += libcaca_plugin.la
vout_PLUGINS += $(LTLIBcaca)

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

vout_PLUGINS += \
	libflaschen_plugin.la \
	libvdummy_plugin.la \
	libvideo_splitter_plugin.la \
	libvmem_plugin.la \
	libwdummy_plugin.la \
	libwextern_plugin.la \
	libvgl_plugin.la \
	libyuv_plugin.la
