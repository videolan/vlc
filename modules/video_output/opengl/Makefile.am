OPENGL_COMMONSOURCES = video_output/opengl/vout_helper.c \
       video_output/opengl/gl_api.c \
       video_output/opengl/gl_api.h \
       video_output/opengl/gl_common.h \
       video_output/opengl/gl_util.c \
       video_output/opengl/gl_util.h \
       video_output/opengl/interop.h \
       video_output/opengl/vout_helper.h \
       video_output/opengl/internal.h \
       video_output/opengl/interop.c video_output/opengl/interop_sw.c \
       video_output/opengl/renderer.c \
       video_output/opengl/renderer.h \
       video_output/opengl/sampler.c \
       video_output/opengl/sampler.h \
       video_output/opengl/sampler_priv.h \
       video_output/opengl/sub_renderer.c \
       video_output/opengl/sub_renderer.h
if HAVE_LIBPLACEBO
OPENGL_COMMONSOURCES += video_output/placebo_utils.c video_output/placebo_utils.h
endif

OPENGL_COMMONCFLAGS = $(LIBPLACEBO_CFLAGS)
OPENGL_COMMONLIBS = $(LIBPLACEBO_LIBS)

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
libgles2_plugin_la_SOURCES = video_output/opengl/display.c
libgles2_plugin_la_CFLAGS = $(AM_CFLAGS) $(GLES2_CFLAGS) -DUSE_OPENGL_ES2
libgles2_plugin_la_LIBADD = $(GLES2_LIBS) libvlc_opengles.la
libgles2_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'
if HAVE_GLES2
vout_LTLIBRARIES += libgles2_plugin.la
endif

libgl_plugin_la_SOURCES = video_output/opengl/display.c
libgl_plugin_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS) $(OPENGL_COMMONCFLAGS)
libgl_plugin_la_LIBADD = libvlc_opengl.la
if HAVE_WIN32
libgl_plugin_la_CFLAGS += -DHAVE_GL_CORE_SYMBOLS
libgl_plugin_la_LIBADD += $(GL_LIBS)
endif

if HAVE_GL
vout_LTLIBRARIES += libgl_plugin.la
endif # HAVE_GL
