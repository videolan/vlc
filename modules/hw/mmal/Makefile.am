include $(top_srcdir)/modules/common.am
mmaldir = $(pluginsdir)/mmal

libmmal_vout_plugin_la_SOURCES = vout.c
libmmal_vout_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_mmal)
libmmal_vout_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(mmaldir)' $(LDFLAGS_mmal) -lm
libmmal_vout_plugin_la_LIBADD = $(LIBS_mmal)
mmal_LTLIBRARIES = libmmal_vout_plugin.la
