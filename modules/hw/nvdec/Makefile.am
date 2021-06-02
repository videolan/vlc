nvdecdir = $(pluginsdir)/nvdec
nvdec_LTLIBRARIES =
nvdec_PLUGINS =

if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(nvdec_PLUGINS)
else
nvdec_LTLIBRARIES += $(nvdec_PLUGINS)
endif

libnvdec_plugin_la_SOURCES = \
	hw/nvdec/nvdec.c hw/nvdec/nvdec_fmt.h \
	hw/nvdec/hw_pool.c hw/nvdec/hw_pool.h
libnvdec_plugin_la_LIBADD = $(LIBDL) libvlc_hxxxhelper.la
if HAVE_NVDEC
nvdec_PLUGINS += libnvdec_plugin.la
endif

libnvdec_chroma_plugin_la_SOURCES = hw/nvdec/chroma.c hw/nvdec/nvdec_fmt.h
if HAVE_NVDEC
nvdec_PLUGINS += libnvdec_chroma_plugin.la
endif

libglinterop_nvdec_plugin_la_SOURCES = hw/nvdec/nvdec_gl.c \
	video_output/opengl/interop.h hw/nvdec/nvdec_fmt.h
libglinterop_nvdec_plugin_la_LIBADD = $(LIBDL)
if HAVE_GL
if HAVE_NVDEC
codec_PLUGINS += libglinterop_nvdec_plugin.la
endif
endif
