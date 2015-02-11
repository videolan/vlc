visudir = $(pluginsdir)/visualization
visu_LTLIBRARIES =

libglspectrum_plugin_la_SOURCES = \
	visualization/glspectrum.c \
	visualization/visual/fft.c visualization/visual/fft.h \
	visualization/visual/window.c visualization/visual/window.h \
	visualization/visual/window_presets.h
libglspectrum_plugin_la_LIBADD = $(GL_LIBS) $(LIBM)
if HAVE_GL
visu_LTLIBRARIES += libglspectrum_plugin.la
endif

libgoom_plugin_la_SOURCES = visualization/goom.c
libgoom_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(GOOM_CFLAGS)
libgoom_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(visudir)'
libgoom_plugin_la_LIBADD = $(GOOM_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libgoom_plugin.la
visu_LTLIBRARIES += $(LTLIBgoom)

libprojectm_plugin_la_SOURCES = visualization/projectm.cpp
libprojectm_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(PROJECTM_CFLAGS)
libprojectm_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(visudir)'
libprojectm_plugin_la_LIBADD = $(PROJECTM_LIBS) $(LIBPTHREAD)
EXTRA_LTLIBRARIES += libprojectm_plugin.la
visu_LTLIBRARIES += $(LTLIBprojectm)

libvisual_plugin_la_SOURCES = \
	visualization/visual/visual.c visualization/visual/visual.h \
	visualization/visual/effects.c \
	visualization/visual/fft.c visualization/visual/fft.h \
	visualization/visual/window.c visualization/visual/window.h \
	visualization/visual/window_presets.h
libvisual_plugin_la_LIBADD = $(LIBM)
visu_LTLIBRARIES += libvisual_plugin.la

libvsxu_plugin_la_SOURCES = visualization/vsxu.cpp visualization/cyclic_buffer.h
libvsxu_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(VSXU_CFLAGS)
libvsxu_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(visudir)'
libvsxu_plugin_la_LIBADD = $(VSXU_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libvsxu_plugin.la
visu_LTLIBRARIES += $(LTLIBvsxu)
