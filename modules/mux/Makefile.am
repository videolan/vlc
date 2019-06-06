muxdir = $(pluginsdir)/mux

extradata_builder_SOURCES = mux/extradata.c mux/extradata.h \
                            packetizer/av1_obu.c

libmux_dummy_plugin_la_SOURCES = mux/dummy.c

libmux_asf_plugin_la_SOURCES = mux/asf.c demux/asf/libasf_guid.h
libmux_avi_plugin_la_SOURCES = mux/avi.c demux/avi/bitmapinfoheader.h
libmux_mp4_plugin_la_SOURCES = mux/mp4/mp4.c \
	mux/mp4/libmp4mux.c mux/mp4/libmp4mux.h \
        demux/mp4/libmp4.h mux/av1_pack.h \
	packetizer/hxxx_nal.c packetizer/hxxx_nal.h \
        packetizer/hevc_nal.c packetizer/hevc_nal.h \
        packetizer/h264_nal.c packetizer/h264_nal.h
libmux_mp4_plugin_la_SOURCES += $(extradata_builder_SOURCES)

libmux_mpjpeg_plugin_la_SOURCES = mux/mpjpeg.c
libmux_ps_plugin_la_SOURCES = \
	mux/mpeg/pes.c mux/mpeg/pes.h \
	mux/mpeg/ps.c mux/mpeg/bits.h \
	demux/mpeg/timestamps.h
libmux_wav_plugin_la_SOURCES = mux/wav.c

mux_LTLIBRARIES = \
	libmux_dummy_plugin.la \
	libmux_asf_plugin.la \
	libmux_avi_plugin.la \
	libmux_mp4_plugin.la \
	libmux_mpjpeg_plugin.la \
	libmux_ps_plugin.la \
	libmux_wav_plugin.la

libmux_ogg_plugin_la_SOURCES = mux/ogg.c
libmux_ogg_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS)
libmux_ogg_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(muxdir)'
libmux_ogg_plugin_la_LIBADD = $(OGG_LIBS)
EXTRA_LTLIBRARIES += libmux_ogg_plugin.la
mux_LTLIBRARIES += $(LTLIBmux_ogg)

libmux_ts_plugin_la_SOURCES = \
	mux/mpeg/pes.c mux/mpeg/pes.h \
	mux/mpeg/csa.c mux/mpeg/csa.h \
	mux/mpeg/streams.h \
	mux/mpeg/tables.c mux/mpeg/tables.h \
	mux/mpeg/tsutil.c mux/mpeg/tsutil.h \
	codec/jpeg2000.h \
	mux/mpeg/ts.c mux/mpeg/bits.h mux/mpeg/dvbpsi_compat.h \
	demux/mpeg/timestamps.h
libmux_ts_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(DVBPSI_CFLAGS)
libmux_ts_plugin_la_LIBADD = $(DVBPSI_LIBS)
if HAVE_DVBPSI
mux_LTLIBRARIES += libmux_ts_plugin.la
endif
