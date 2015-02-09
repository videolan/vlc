packetizerdir = $(pluginsdir)/packetizer

libpacketizer_copy_plugin_la_SOURCES = packetizer/copy.c
libpacketizer_mpegvideo_plugin_la_SOURCES = packetizer/mpegvideo.c
libpacketizer_mpeg4video_plugin_la_SOURCES = packetizer/mpeg4video.c
libpacketizer_mpeg4audio_plugin_la_SOURCES = packetizer/mpeg4audio.c
libpacketizer_h264_plugin_la_SOURCES = \
	codec/h264_nal.c codec/h264_nal.h \
	packetizer/h264.c
libpacketizer_vc1_plugin_la_SOURCES = packetizer/vc1.c
libpacketizer_mlp_plugin_la_SOURCES = packetizer/mlp.c
libpacketizer_dirac_plugin_la_SOURCES = packetizer/dirac.c
libpacketizer_flac_plugin_la_SOURCES = packetizer/flac.c
libpacketizer_hevc_plugin_la_SOURCES = packetizer/hevc.c

noinst_HEADERS += packetizer/packetizer_helper.h

packetizer_LTLIBRARIES = \
	libpacketizer_mpegvideo_plugin.la \
	libpacketizer_mpeg4video_plugin.la \
	libpacketizer_mpeg4audio_plugin.la \
	libpacketizer_h264_plugin.la \
	libpacketizer_vc1_plugin.la \
	libpacketizer_mlp_plugin.la \
	libpacketizer_dirac_plugin.la \
	libpacketizer_flac_plugin.la \
	libpacketizer_hevc_plugin.la \
	$(NULL)
if ENABLE_SOUT
packetizer_LTLIBRARIES += libpacketizer_copy_plugin.la
endif
