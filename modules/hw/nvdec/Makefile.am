nvdecdir = $(pluginsdir)/nvdec

libnvdec_plugin_la_SOURCES = \
	hw/nvdec/nvdec.c hw/nvdec/nvdec_fmt.h
libnvdec_plugin_la_LIBADD = $(LIBDL) libvlc_hxxxhelper.la
if HAVE_NVDEC
codec_LTLIBRARIES += libnvdec_plugin.la
endif

libnvdec_chroma_plugin_la_SOURCES = hw/nvdec/chroma.c hw/nvdec/nvdec_fmt.h
libnvdec_chroma_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(nvdecdir)'
if HAVE_NVDEC
nvdec_LTLIBRARIES = libnvdec_chroma_plugin.la
endif

libglinterop_nvdec_plugin_la_SOURCES = hw/nvdec/nvdec_gl.c \
	video_output/opengl/interop.h hw/nvdec/nvdec_fmt.h
libglinterop_nvdec_plugin_la_LIBADD = $(LIBDL)
if HAVE_GL
if HAVE_NVDEC
nvdec_LTLIBRARIES += libglinterop_nvdec_plugin.la
endif
endif
