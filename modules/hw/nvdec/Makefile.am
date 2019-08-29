nvdecdir = $(pluginsdir)/nvdec

libnvdec_plugin_la_SOURCES = \
	hw/nvdec/nvdec.c codec/hxxx_helper.c codec/hxxx_helper.h \
	packetizer/hxxx_nal.h packetizer/hxxx_nal.c \
	packetizer/h264_nal.c packetizer/h264_nal.h \
	packetizer/hevc_nal.c packetizer/hevc_nal.h
libnvdec_plugin_la_LIBADD = $(LIBDL)
if HAVE_NVDEC
codec_LTLIBRARIES += libnvdec_plugin.la
endif
