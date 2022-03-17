visudir = $(pluginsdir)/visualization
visu_PLUGINS =
visu_LTLIBRARIES =
visu_RPATH =

if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(visu_PLUGINS)
else
visu_LTLIBRARIES += $(visu_PLUGINS)
visu_RPATH += -rpath '$(visudir)'
endif

libglspectrum_plugin_la_SOURCES = \
	visualization/glspectrum.c \
	visualization/visual/fft.c visualization/visual/fft.h \
	visualization/visual/window.c visualization/visual/window.h \
	visualization/visual/window_presets.h
libglspectrum_plugin_la_LIBADD = $(GL_LIBS) $(LIBM)
if HAVE_GL
visu_PLUGINS += libglspectrum_plugin.la
endif

libgoom_plugin_la_SOURCES = visualization/goom.c
libgoom_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(GOOM_CFLAGS)
libgoom_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(visu_RPATH)
libgoom_plugin_la_LIBADD = $(GOOM_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libgoom_plugin.la
visu_PLUGINS += $(LTLIBgoom)

libprojectm_plugin_la_SOURCES = visualization/projectm.cpp
libprojectm_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(PROJECTM_CFLAGS) -std=gnu++14
libprojectm_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(visu_RPATH)
libprojectm_plugin_la_LIBADD = $(PROJECTM_LIBS)
EXTRA_LTLIBRARIES += libprojectm_plugin.la
visu_PLUGINS += $(LTLIBprojectm)

libvisual_plugin_la_SOURCES = \
	visualization/visual/visual.c visualization/visual/visual.h \
	visualization/visual/effects.c \
	visualization/visual/fft.c visualization/visual/fft.h \
	visualization/visual/window.c visualization/visual/window.h \
	visualization/visual/window_presets.h
libvisual_plugin_la_LIBADD = $(LIBM)
visu_PLUGINS += libvisual_plugin.la

libvsxu_plugin_la_SOURCES = visualization/vsxu.cpp visualization/cyclic_buffer.h
libvsxu_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(VSXU_CFLAGS)
libvsxu_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(visu_RPATH)
libvsxu_plugin_la_LIBADD = $(VSXU_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libvsxu_plugin.la
visu_PLUGINS += $(LTLIBvsxu)
