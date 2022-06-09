OPENGL_COMMONSOURCES = \
       video_output/opengl/filter.c \
       video_output/opengl/filter.h \
       video_output/opengl/filter_priv.h \
       video_output/opengl/filters.c \
       video_output/opengl/filters.h \
       video_output/opengl/gl_api.c \
       video_output/opengl/gl_api.h \
       video_output/opengl/gl_common.h \
       video_output/opengl/gl_util.c \
       video_output/opengl/gl_util.h \
       video_output/opengl/importer.c \
       video_output/opengl/importer.h \
       video_output/opengl/importer_priv.h \
       video_output/opengl/interop.h \
       video_output/opengl/interop.c \
       video_output/opengl/interop_sw.c \
       video_output/opengl/interop_sw.h \
       video_output/opengl/picture.c \
       video_output/opengl/picture.h \
       video_output/opengl/sampler.c \
       video_output/opengl/sampler.h

OPENGL_COMMONCFLAGS = $(LIBPLACEBO_CFLAGS)
OPENGL_COMMONLIBS = $(LIBPLACEBO_LIBS)

if HAVE_LIBPLACEBO
OPENGL_COMMONLIBS += libplacebo_utils.la
endif

OPENGL_VOUT_COMMONSOURCES = \
    video_output/opengl/gl_scale.h \
    video_output/opengl/renderer.c \
    video_output/opengl/renderer.h \
    video_output/opengl/sub_renderer.c \
    video_output/opengl/sub_renderer.h \
    video_output/opengl/vout_helper.c \
    video_output/opengl/vout_helper.h

# Convenience library for OpenGL components -- OpenGL only
libvlc_opengl_la_SOURCES = $(OPENGL_COMMONSOURCES)
libvlc_opengl_la_CFLAGS = $(OPENGL_COMMONCFLAGS)
libvlc_opengl_la_LIBADD = $(OPENGL_COMMONLIBS) $(LIBM)
libvlc_opengl_la_LDFLAGS = -static -no-undefined

if HAVE_GL
noinst_LTLIBRARIES += libvlc_opengl.la
endif

# Convenience library for OpenGL components -- OpenGL ES only
libvlc_opengles_la_SOURCES = $(OPENGL_COMMONSOURCES)
libvlc_opengles_la_CFLAGS = $(OPENGL_COMMONCFLAGS) -DUSE_OPENGL_ES2
libvlc_opengles_la_LIBADD = $(OPENGL_COMMONLIBS) $(LIBM)
libvlc_opengles_la_LDFLAGS = -static -no-undefined

libvlc_opengles_la_CFLAGS += $(GLES2_CFLAGS)
libvlc_opengles_la_LIBADD += $(GLES2_LIBS)
if HAVE_GLES2
noinst_LTLIBRARIES += libvlc_opengles.la
endif

### OpenGL ###
libgles2_plugin_la_SOURCES = video_output/opengl/display.c \
    $(OPENGL_VOUT_COMMONSOURCES)
libgles2_plugin_la_CFLAGS = $(AM_CFLAGS) $(GLES2_CFLAGS) -DUSE_OPENGL_ES2
libgles2_plugin_la_LIBADD = libvlc_opengles.la
libgles2_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
if HAVE_GLES2
vout_LTLIBRARIES += libgles2_plugin.la
endif

libgl_plugin_la_SOURCES = video_output/opengl/display.c \
    $(OPENGL_VOUT_COMMONSOURCES)
libgl_plugin_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS) $(OPENGL_COMMONCFLAGS)
libgl_plugin_la_LIBADD = libvlc_opengl.la
if HAVE_GL
vout_LTLIBRARIES += libgl_plugin.la
endif # HAVE_GL

if HAVE_OSX
vout_LTLIBRARIES += libgl_plugin.la
endif

libglfilter_draw_plugin_la_SOURCES = video_output/opengl/filter_draw.c
libglfilter_draw_plugin_la_LDFLAGS = $(AM_LDFLAGS)
if HAVE_GL
libglfilter_draw_plugin_la_LIBADD = libvlc_opengl.la $(GL_LIBS)
vout_LTLIBRARIES += libglfilter_draw_plugin.la
endif

if HAVE_DARWIN
vout_LTLIBRARIES += libglfilter_draw_plugin.la
if HAVE_OSX
libglfilter_draw_plugin_la_LIBADD = libvlc_opengl.la
else
libglfilter_draw_plugin_la_LIBADD = libvlc_opengles.la $(GLES2_LIBS)
libglfilter_draw_plugin_la_CFLAGS = -DUSE_OPENGL_ES2=1
endif
endif

if HAVE_ANDROID
libglfilter_draw_plugin_la_LIBADD = libvlc_opengles.la $(GLES2_LIBS)
libglfilter_draw_plugin_la_CFLAGS = -DUSE_OPENGL_ES2=1
vout_LTLIBRARIES += libglfilter_draw_plugin.la
endif

libglfilter_mock_plugin_la_SOURCES = video_output/opengl/filter_mock.c
libglfilter_mock_plugin_la_LIBADD = $(LIBM)
libglfilter_mock_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
if HAVE_GL
libglfilter_mock_plugin_la_LIBADD += libvlc_opengl.la $(GL_LIBS)
noinst_LTLIBRARIES += libglfilter_mock_plugin.la
endif

if HAVE_IOS
libglfilter_mock_plugin_la_LIBADD += libvlc_opengles.la $(GLES2_LIBS)
libglfilter_mock_plugin_la_CFLAGS = -DUSE_OPENGL_ES2=1
noinst_LTLIBRARIES += libglfilter_mock_plugin.la
endif

if HAVE_OSX
libglfilter_mock_plugin_la_LIBADD += libvlc_opengl.la
noinst_LTLIBRARIES += libglfilter_mock_plugin.la
endif

if HAVE_ANDROID
libglfilter_mock_plugin_la_LIBADD += libvlc_opengles.la $(GLES2_LIBS)
libglfilter_mock_plugin_la_CFLAGS = -DUSE_OPENGL_ES2=1
noinst_LTLIBRARIES += libglfilter_mock_plugin.la
endif

if HAVE_LIBPLACEBO
if HAVE_LIBPLACEBO_SCALE

libpl_scale_plugin_la_SOURCES = video_output/opengl/pl_scale.c
libpl_scale_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(LIBPLACEBO_CFLAGS)
libpl_scale_plugin_la_LIBADD = $(LIBPLACEBO_LIBS)
if HAVE_GL
libpl_scale_plugin_la_LIBADD += libvlc_opengl.la
video_filter_LTLIBRARIES += libpl_scale_plugin.la
endif

if HAVE_DARWIN
video_filter_LTLIBRARIES += libpl_scale_plugin.la
if HAVE_OSX
libpl_scale_plugin_la_LIBADD += libvlc_opengl.la
else
libpl_scale_plugin_la_LIBADD += libvlc_opengles.la
libpl_scale_plugin_la_CPPFLAGS += -DUSE_OPENGL_ES2=1
endif
endif

if HAVE_ANDROID
libpl_scale_plugin_la_LIBADD += libvlc_opengles.la
libpl_scale_plugin_la_CPPFLAGS += -DUSE_OPENGL_ES2=1
video_filter_LTLIBRARIES += libpl_scale_plugin.la
endif

endif
endif

libegl_display_generic_plugin_la_SOURCES = video_output/opengl/egl_display_generic.c
libegl_display_generic_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(EGL_FLAGS)
libegl_display_generic_plugin_la_LIBADD = $(EGL_LIBS)
if HAVE_EGL
vout_LTLIBRARIES += libegl_display_generic_plugin.la
endif

libegl_display_gbm_plugin_la_SOURCES = video_output/opengl/egl_display_gbm.c
libegl_display_gbm_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(EGL_FLAGS) $(GBM_CFLAGS)
libegl_display_gbm_plugin_la_LIBADD = $(EGL_LIBS) $(GBM_LIBS)
if HAVE_LIBDRM
# DRM headers are bringing defines useful for opening DRM devices
libegl_display_gbm_plugin_la_CPPFLAGS += $(LIBDRM_CFLAGS) -DHAVE_KMS=1
endif
if HAVE_EGL
if HAVE_GBM
vout_LTLIBRARIES += libegl_display_gbm_plugin.la
endif
endif

libegl_pbuffer_filter_plugin_la_SOURCES = video_filter/egl_pbuffer.c \
    video_output/opengl/egl_display.c video_output/opengl/egl_display.h
libegl_pbuffer_filter_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(EGL_FLAGS)
libegl_pbuffer_filter_plugin_la_LIBADD = $(EGL_LIBS)

if HAVE_EGL
if HAVE_GL
libegl_pbuffer_filter_plugin_la_LIBADD += libvlc_opengl.la $(GL_LIBS)
vout_LTLIBRARIES += libegl_pbuffer_filter_plugin.la
endif
if HAVE_ANDROID
libegl_pbuffer_filter_plugin_la_LIBADD += libvlc_opengles.la $(GLES2_LIBS)
libegl_pbuffer_filter_plugin_la_CPPFLAGS += -DUSE_OPENGL_ES2=1
vout_LTLIBRARIES += libegl_pbuffer_filter_plugin.la
endif
endif

libegl_surfacetexture_plugin_la_SOURCES = video_filter/egl_surfacetexture.c
libegl_surfacetexture_plugin_la_CFLAGS = $(AM_CFLAGS) $(EGL_CFLAGS) -DUSE_OPENGL_ES2
libegl_surfacetexture_plugin_la_LIBADD = $(EGL_LIBS) libandroid_utils.la libvlc_opengles.la

if HAVE_ANDROID
if HAVE_EGL
vout_LTLIBRARIES += libegl_surfacetexture_plugin.la
endif
endif
