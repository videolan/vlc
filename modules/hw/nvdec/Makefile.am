nvdecdir = $(pluginsdir)/nvdec

libnvdec_plugin_la_SOURCES = \
	hw/nvdec/nvdec.c codec/hxxx_helper.c codec/hxxx_helper.h \
	packetizer/hxxx_nal.h packetizer/hxxx_nal.c \
	packetizer/h264_nal.c packetizer/h264_nal.h \
	packetizer/hevc_nal.c packetizer/hevc_nal.h \
	hw/nvdec/nvdec_fmt.h
libnvdec_plugin_la_LIBADD = $(LIBDL)
if HAVE_NVDEC
codec_LTLIBRARIES += libnvdec_plugin.la
endif

libnvdec_chroma_plugin_la_SOURCES = hw/nvdec/chroma.c hw/nvdec/nvdec_fmt.h
libnvdec_chroma_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(nvdecdir)'
if HAVE_NVDEC
nvdec_LTLIBRARIES = libnvdec_chroma_plugin.la
endif

libglconv_nvdec_plugin_la_SOURCES = hw/nvdec/nvdec_gl.c \
	video_output/opengl/converter.h hw/nvdec/nvdec_fmt.h
libglconv_nvdec_plugin_la_LIBADD = $(LIBDL)
if HAVE_GL
if HAVE_NVDEC
nvdec_LTLIBRARIES += libglconv_nvdec_plugin.la
endif
endif
