demuxdir = $(pluginsdir)/demux
demux_LTLIBRARIES =

libflacsys_plugin_la_SOURCES = demux/flac.c demux/xiph_metadata.h demux/xiph_metadata.c
libflacsys_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
demux_LTLIBRARIES += libflacsys_plugin.la

libogg_plugin_la_SOURCES = demux/ogg.c demux/ogg.h demux/oggseek.c demux/oggseek.h \
	demux/xiph_metadata.h demux/xiph.h demux/xiph_metadata.c
libogg_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_ogg) $(LIBVORBIS_CFLAGS)
libogg_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libogg_plugin_la_LIBADD = $(LIBS_ogg) $(LIBVORBIS_LIBS)
EXTRA_LTLIBRARIES += libogg_plugin.la
demux_LTLIBRARIES += $(LTLIBogg)

libdemuxdump_plugin_la_SOURCES = demux/demuxdump.c
demux_LTLIBRARIES += libdemuxdump_plugin.la

librawdv_plugin_la_SOURCES = demux/rawdv.c demux/rawdv.h
demux_LTLIBRARIES += librawdv_plugin.la

librawvid_plugin_la_SOURCES = demux/rawvid.c
demux_LTLIBRARIES += librawvid_plugin.la

libau_plugin_la_SOURCES = demux/au.c
demux_LTLIBRARIES += libau_plugin.la

librawaud_plugin_la_SOURCES = demux/rawaud.c
demux_LTLIBRARIES += librawaud_plugin.la

libwav_plugin_la_SOURCES = demux/wav.c demux/windows_audio_commons.h
demux_LTLIBRARIES += libwav_plugin.la

libnsv_plugin_la_SOURCES = demux/nsv.c
demux_LTLIBRARIES += libnsv_plugin.la

libreal_plugin_la_SOURCES = demux/real.c
demux_LTLIBRARIES += libreal_plugin.la

libps_plugin_la_SOURCES = demux/ps.c demux/ps.h
demux_LTLIBRARIES += libps_plugin.la

libmod_plugin_la_SOURCES = demux/mod.c
libmod_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_mod)
libmod_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libmod_plugin_la_LIBADD = $(LIBS_mod)
EXTRA_LTLIBRARIES += libmod_plugin.la
demux_LTLIBRARIES += $(LTLIBmod)

libpva_plugin_la_SOURCES = demux/pva.c
demux_LTLIBRARIES += libpva_plugin.la

libaiff_plugin_la_SOURCES = demux/aiff.c
demux_LTLIBRARIES += libaiff_plugin.la

libmjpeg_plugin_la_SOURCES = demux/mjpeg.c demux/mxpeg_helper.h
demux_LTLIBRARIES += libmjpeg_plugin.la

libsubtitle_plugin_la_SOURCES = demux/subtitle.c
demux_LTLIBRARIES += libsubtitle_plugin.la

libty_plugin_la_SOURCES = demux/ty.c codec/cc.h
demux_LTLIBRARIES += libty_plugin.la

libvobsub_plugin_la_SOURCES = demux/vobsub.c demux/vobsub.h
demux_LTLIBRARIES += libvobsub_plugin.la

libvoc_plugin_la_SOURCES = demux/voc.c
demux_LTLIBRARIES += libvoc_plugin.la

libxa_plugin_la_SOURCES = demux/xa.c
demux_LTLIBRARIES += libxa_plugin.la

libnuv_plugin_la_SOURCES = demux/nuv.c
demux_LTLIBRARIES += libnuv_plugin.la

libnsc_plugin_la_SOURCES = demux/nsc.c
demux_LTLIBRARIES += libnsc_plugin.la

libmpc_plugin_la_SOURCES = demux/mpc.c
libmpc_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libmpc_plugin_la_LIBADD = $(LIBS_mpc)
EXTRA_LTLIBRARIES += libmpc_plugin.la
demux_LTLIBRARIES += $(LTLIBmpc)

libtta_plugin_la_SOURCES = demux/tta.c
demux_LTLIBRARIES += libtta_plugin.la

libvc1_plugin_la_SOURCES = demux/vc1.c
demux_LTLIBRARIES += libvc1_plugin.la

libdemux_cdg_plugin_la_SOURCES = demux/cdg.c
libdemux_cdg_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
demux_LTLIBRARIES += libdemux_cdg_plugin.la

libsmf_plugin_la_SOURCES = demux/smf.c
demux_LTLIBRARIES += libsmf_plugin.la

libgme_plugin_la_SOURCES = demux/gme.c dummy.cpp
libgme_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libgme_plugin_la_LIBADD = $(LIBS_gme)
EXTRA_LTLIBRARIES += libgme_plugin.la
demux_LTLIBRARIES += $(LTLIBgme)

libsid_plugin_la_SOURCES = demux/sid.cpp
libsid_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_sid)
libsid_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libsid_plugin_la_LIBADD = $(LIBS_sid)
EXTRA_LTLIBRARIES += libsid_plugin.la
demux_LTLIBRARIES += $(LTLIBsid)

libdiracsys_plugin_la_SOURCES = demux/dirac.c
libdiracsys_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
demux_LTLIBRARIES += libdiracsys_plugin.la

libimage_plugin_la_SOURCES = demux/image.c demux/mxpeg_helper.h
demux_LTLIBRARIES += libimage_plugin.la

libdemux_stl_plugin_la_SOURCES = demux/stl.c
libdemux_stl_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
demux_LTLIBRARIES += libdemux_stl_plugin.la

libasf_plugin_la_SOURCES = demux/asf/asf.c demux/asf/libasf.c demux/asf/libasf.h demux/asf/libasf_guid.h
demux_LTLIBRARIES += libasf_plugin.la

libavi_plugin_la_SOURCES = demux/avi/avi.c demux/avi/libavi.c demux/avi/libavi.h
demux_LTLIBRARIES += libavi_plugin.la

libcaf_plugin_la_SOURCES = demux/caf.c
libcaf_plugin_la_LIBADD = $(LIBM)
demux_LTLIBRARIES += libcaf_plugin.la

libavformat_plugin_la_SOURCES = demux/avformat/demux.c \
	codec/avcodec/fourcc.c \
	codec/avcodec/chroma.c \
	codec/avcodec/avcommon.h \
	codec/avcodec/avcommon_compat.h \
	demux/vobsub.h \
	demux/avformat/avformat.c demux/avformat/avformat.h
if ENABLE_SOUT
libavformat_plugin_la_SOURCES += demux/avformat/mux.c
endif
libavformat_plugin_la_CFLAGS = $(AM_CFLAGS) $(AVFORMAT_CFLAGS) $(AVUTIL_CFLAGS)
libavformat_plugin_la_LIBADD = $(AVFORMAT_LIBS) $(AVUTIL_LIBS) $(LIBM)
libavformat_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(SYMBOLIC_LDFLAGS)
if HAVE_ZLIB
libavformat_plugin_la_LIBADD += -lz
endif
if HAVE_DARWIN
libavformat_plugin_la_LDFLAGS += -Wl,-read_only_relocs,suppress
endif
if HAVE_AVFORMAT
if !MERGE_FFMPEG
demux_LTLIBRARIES += libavformat_plugin.la
endif
endif

libes_plugin_la_SOURCES  = demux/mpeg/es.c codec/dts_header.c codec/dts_header.h
demux_LTLIBRARIES += libes_plugin.la

libh264_plugin_la_SOURCES = demux/mpeg/h264.c
demux_LTLIBRARIES += libh264_plugin.la

libhevc_plugin_la_SOURCES = demux/mpeg/hevc.c demux/mpeg/mpeg_parser_helpers.h
demux_LTLIBRARIES += libhevc_plugin.la

libmkv_plugin_la_SOURCES = \
	demux/mkv/util.hpp demux/mkv/util.cpp \
	demux/mkv/virtual_segment.hpp demux/mkv/virtual_segment.cpp \
	demux/mkv/matroska_segment.hpp demux/mkv/matroska_segment.cpp \
	demux/mkv/matroska_segment_parse.cpp \
	demux/mkv/demux.hpp demux/mkv/demux.cpp \
	demux/mkv/Ebml_parser.hpp demux/mkv/Ebml_parser.cpp \
	demux/mkv/chapters.hpp demux/mkv/chapters.cpp \
	demux/mkv/chapter_command.hpp demux/mkv/chapter_command.cpp \
	demux/mkv/stream_io_callback.hpp demux/mkv/stream_io_callback.cpp \
	demux/mp4/libmp4.c demux/vobsub.h \
	demux/mkv/mkv.hpp demux/mkv/mkv.cpp \
	demux/windows_audio_commons.h
libmkv_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libmkv_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libmkv_plugin_la_LIBADD = $(LIBS_mkv)
if HAVE_ZLIB
libmkv_plugin_la_LIBADD += -lz
endif
demux_LTLIBRARIES += $(LTLIBmkv)
EXTRA_LTLIBRARIES += libmkv_plugin.la

libmp4_plugin_la_SOURCES = demux/mp4/mp4.c demux/mp4/mp4.h \
                           demux/mp4/libmp4.c demux/mp4/libmp4.h demux/mp4/id3genres.h
libmp4_plugin_la_LIBADD = $(LIBM)
libmp4_plugin_la_LDFLAGS = $(AM_LDFLAGS)
if HAVE_ZLIB
libmp4_plugin_la_LIBADD += -lz
endif
demux_LTLIBRARIES += libmp4_plugin.la

libmpgv_plugin_la_SOURCES = demux/mpeg/mpgv.c
demux_LTLIBRARIES += libmpgv_plugin.la

libplaylist_plugin_la_SOURCES = \
	demux/playlist/asx.c \
	demux/playlist/b4s.c \
	demux/playlist/dvb.c \
	demux/playlist/gvp.c \
	demux/playlist/ifo.c \
	demux/playlist/itml.c \
	demux/playlist/itml.h \
	demux/playlist/m3u.c \
	demux/playlist/pls.c \
	demux/playlist/podcast.c \
	demux/playlist/qtl.c \
	demux/playlist/ram.c \
	demux/playlist/sgimb.c \
	demux/playlist/shoutcast.c \
	demux/playlist/wpl.c \
	demux/playlist/xspf.c \
	demux/playlist/zpl.c \
	demux/playlist/directory.c \
	demux/playlist/playlist.c demux/playlist/playlist.h
demux_LTLIBRARIES += libplaylist_plugin.la

libts_plugin_la_SOURCES = demux/ts.c mux/mpeg/csa.c mux/mpeg/dvbpsi_compat.h demux/dvb-text.h
libts_plugin_la_CFLAGS = $(AM_CFLAGS) $(DVBPSI_CFLAGS)
libts_plugin_la_LIBADD = $(DVBPSI_LIBS) $(SOCKET_LIBS)
if HAVE_DVBPSI
demux_LTLIBRARIES += libts_plugin.la
endif
