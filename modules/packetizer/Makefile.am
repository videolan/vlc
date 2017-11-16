packetizerdir = $(pluginsdir)/packetizer

libpacketizer_copy_plugin_la_SOURCES = packetizer/copy.c
libpacketizer_mpegvideo_plugin_la_SOURCES = packetizer/mpegvideo.c
libpacketizer_mpeg4video_plugin_la_SOURCES = packetizer/mpeg4video.c
libpacketizer_mpeg4audio_plugin_la_SOURCES = packetizer/mpeg4audio.c
libpacketizer_mpegaudio_plugin_la_SOURCES = packetizer/mpegaudio.c
libpacketizer_h264_plugin_la_SOURCES = \
	packetizer/h264_nal.c packetizer/h264_nal.h \
	packetizer/h264_slice.c packetizer/h264_slice.h \
	packetizer/h264.c packetizer/hxxx_nal.h \
	packetizer/hxxx_sei.c packetizer/hxxx_sei.h \
	packetizer/hxxx_common.c packetizer/hxxx_common.h
libpacketizer_vc1_plugin_la_SOURCES = packetizer/vc1.c \
        packetizer/hxxx_nal.h
libpacketizer_mlp_plugin_la_SOURCES = packetizer/mlp.c
libpacketizer_dirac_plugin_la_SOURCES = packetizer/dirac.c
libpacketizer_flac_plugin_la_SOURCES = packetizer/flac.c
libpacketizer_hevc_plugin_la_SOURCES = packetizer/hevc.c \
	packetizer/hevc_nal.h packetizer/hevc_nal.c \
	packetizer/hxxx_sei.c packetizer/hxxx_sei.h \
	packetizer/hxxx_nal.h \
	packetizer/hxxx_common.c packetizer/hxxx_common.h
libpacketizer_a52_plugin_la_SOURCES = packetizer/a52.c packetizer/a52.h
libpacketizer_dts_plugin_la_SOURCES = packetizer/dts.c \
	packetizer/dts_header.c packetizer/dts_header.h

libpacketizer_avparser_plugin_la_SOURCES = packetizer/avparser.c \
	packetizer/avparser.h \
	codec/avcodec/avcodec.h
libpacketizer_avparser_plugin_la_CFLAGS = $(AVCODEC_CFLAGS) $(AVUTIL_CFLAGS) $(AM_CFLAGS)
libpacketizer_avparser_plugin_la_LIBADD = $(AVCODEC_LIBS) $(AVUTIL_LIBS) $(LIBM) libavcodec_common.la

noinst_HEADERS += packetizer/packetizer_helper.h packetizer/startcode_helper.h

packetizer_LTLIBRARIES = \
	libpacketizer_mpegvideo_plugin.la \
	libpacketizer_mpeg4video_plugin.la \
	libpacketizer_mpeg4audio_plugin.la \
	libpacketizer_mpegaudio_plugin.la \
	libpacketizer_h264_plugin.la \
	libpacketizer_vc1_plugin.la \
	libpacketizer_mlp_plugin.la \
	libpacketizer_dirac_plugin.la \
	libpacketizer_flac_plugin.la \
	libpacketizer_hevc_plugin.la \
	libpacketizer_copy_plugin.la \
	libpacketizer_a52_plugin.la \
	libpacketizer_dts_plugin.la \
	$(NULL)

if HAVE_AVCODEC
if !MERGE_FFMPEG
packetizer_LTLIBRARIES += libpacketizer_avparser_plugin.la
endif
endif

