spudir = $(pluginsdir)/spu
spu_PLUGINS =
spu_LTLIBRARIES =
if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(spu_PLUGINS)
else
spu_LTLIBRARIES += $(spu_PLUGINS)
endif

noinst_HEADERS += spu/common.h

# sub filters
libsubsdelay_plugin_la_SOURCES = spu/subsdelay.c
spu_PLUGINS += libsubsdelay_plugin.la

# sub sources
libaudiobargraph_v_plugin_la_SOURCES = spu/audiobargraph_v.c
libaudiobargraph_v_plugin_la_LIBADD = $(LIBM)
liblogo_plugin_la_SOURCES = spu/logo.c
libmarq_plugin_la_SOURCES = spu/marq.c
libmosaic_plugin_la_SOURCES = spu/mosaic.c spu/mosaic.h
libmosaic_plugin_la_LIBADD = $(LIBM)
librss_plugin_la_SOURCES = spu/rss.c

spu_PLUGINS += \
	libaudiobargraph_v_plugin.la \
	liblogo_plugin.la \
	libmarq_plugin.la \
	libmosaic_plugin.la \
	librss_plugin.la

libdynamicoverlay_plugin_la_SOURCES = \
	spu/dynamicoverlay/dynamicoverlay_buffer.c \
	spu/dynamicoverlay/dynamicoverlay_queue.c \
	spu/dynamicoverlay/dynamicoverlay_list.c \
	spu/dynamicoverlay/dynamicoverlay_commands.c \
	spu/dynamicoverlay/dynamicoverlay.c spu/dynamicoverlay/dynamicoverlay.h
if !HAVE_WIN32
spu_PLUGINS += libdynamicoverlay_plugin.la
endif
