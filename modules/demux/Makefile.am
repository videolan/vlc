include ../common.am
demuxdir = $(pluginsdir)/demux
demux_LTLIBRARIES =
EXTRA_LTLIBRARIES =

libflacsys_plugin_la_SOURCES = flac.c xiph_metadata.h xiph_metadata.c
demux_LTLIBRARIES += libflacsys_plugin.la

libogg_plugin_la_SOURCES = ogg.c ogg.h oggseek.c oggseek.h \
	xiph_metadata.h xiph.h xiph_metadata.c
libogg_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_ogg)
libogg_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libogg_plugin_la_LIBADD = $(LIBS_ogg)
EXTRA_LTLIBRARIES += libogg_plugin.la
demux_LTLIBRARIES += $(LTLIBogg)

libdemuxdump_plugin_la_SOURCES = demuxdump.c
demux_LTLIBRARIES += libdemuxdump_plugin.la

librawdv_plugin_la_SOURCES = rawdv.c rawdv.h
demux_LTLIBRARIES += librawdv_plugin.la

librawvid_plugin_la_SOURCES = rawvid.c
demux_LTLIBRARIES += librawvid_plugin.la

libau_plugin_la_SOURCES = au.c
demux_LTLIBRARIES += libau_plugin.la

librawaud_plugin_la_SOURCES = rawaud.c
demux_LTLIBRARIES += librawaud_plugin.la

libwav_plugin_la_SOURCES = wav.c
demux_LTLIBRARIES += libwav_plugin.la

libnsv_plugin_la_SOURCES = nsv.c
demux_LTLIBRARIES += libnsv_plugin.la

libreal_plugin_la_SOURCES = real.c
demux_LTLIBRARIES += libreal_plugin.la

libps_plugin_la_SOURCES = ps.c ps.h
demux_LTLIBRARIES += libps_plugin.la

libmod_plugin_la_SOURCES = mod.c
libmod_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS_mod)
libmod_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libmod_plugin_la_LIBADD = $(LIBS_mod)
EXTRA_LTLIBRARIES += libmod_plugin.la
demux_LTLIBRARIES += $(LTLIBmod)

libpva_plugin_la_SOURCES = pva.c
demux_LTLIBRARIES += libpva_plugin.la

libaiff_plugin_la_SOURCES = aiff.c
demux_LTLIBRARIES += libaiff_plugin.la

libmjpeg_plugin_la_SOURCES = mjpeg.c mxpeg_helper.h
demux_LTLIBRARIES += libmjpeg_plugin.la

libsubtitle_plugin_la_SOURCES = subtitle.c
demux_LTLIBRARIES += libsubtitle_plugin.la

libty_plugin_la_SOURCES = ty.c ../codec/cc.h
demux_LTLIBRARIES += libty_plugin.la

libvobsub_plugin_la_SOURCES = vobsub.c vobsub.h
demux_LTLIBRARIES += libvobsub_plugin.la

libvoc_plugin_la_SOURCES = voc.c
demux_LTLIBRARIES += libvoc_plugin.la

libxa_plugin_la_SOURCES = xa.c
demux_LTLIBRARIES += libxa_plugin.la

libnuv_plugin_la_SOURCES = nuv.c
demux_LTLIBRARIES += libnuv_plugin.la

libnsc_plugin_la_SOURCES = nsc.c
demux_LTLIBRARIES += libnsc_plugin.la

libmpc_plugin_la_SOURCES = mpc.c
libmpc_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libmpc_plugin_la_LIBADD = $(LIBS_mpc)
EXTRA_LTLIBRARIES += libmpc_plugin.la
demux_LTLIBRARIES += $(LTLIBmpc)

libtta_plugin_la_SOURCES = tta.c
libtta_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
demux_LTLIBRARIES += libtta_plugin.la

libvc1_plugin_la_SOURCES = vc1.c
libvc1_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
demux_LTLIBRARIES += libvc1_plugin.la

libdemux_cdg_plugin_la_SOURCES = cdg.c
libdemux_cdg_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
demux_LTLIBRARIES += libdemux_cdg_plugin.la

libsmf_plugin_la_SOURCES = smf.c
libsmf_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
demux_LTLIBRARIES += libsmf_plugin.la

libgme_plugin_la_SOURCES = gme.c dummy.cpp
libgme_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libgme_plugin_la_LIBADD = $(LIBS_gme)
EXTRA_LTLIBRARIES += libgme_plugin.la
demux_LTLIBRARIES += $(LTLIBgme)

libsid_plugin_la_SOURCES = sid.cpp
libsid_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_sid)
libsid_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libsid_plugin_la_LIBADD = $(LIBS_sid)
EXTRA_LTLIBRARIES += libsid_plugin.la
demux_LTLIBRARIES += $(LTLIBsid)

libdiracsys_plugin_la_SOURCES = dirac.c
demux_LTLIBRARIES += libdiracsys_plugin.la

libimage_plugin_la_SOURCES = image.c mxpeg_helper.h
demux_LTLIBRARIES += libimage_plugin.la

libdemux_stl_plugin_la_SOURCES = stl.c
demux_LTLIBRARIES += libdemux_stl_plugin.la

libasf_plugin_la_SOURCES = asf/asf.c asf/libasf.c asf/libasf.h asf/libasf_guid.h
demux_LTLIBRARIES += libasf_plugin.la

libavi_plugin_la_SOURCES = avi/avi.c avi/libavi.c avi/libavi.h
demux_LTLIBRARIES += libavi_plugin.la

libavformat_plugin_la_SOURCES = avformat/demux.c \
	../codec/avcodec/fourcc.c \
	../codec/avcodec/chroma.c \
	../codec/avcodec/cpu.c \
	../codec/avcodec/avcommon.h \
	../codec/avcodec/avcommon_compat.h \
	vobsub.h \
	avformat/avformat.c avformat/avformat.h
if ENABLE_SOUT
libavformat_plugin_la_SOURCES += avformat/mux.c
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
if HAVE_WIN32
libavformat_plugin_la_DEPENDENCIES = libavformat_plugin.rc.o
endif
if HAVE_AVFORMAT
demux_LTLIBRARIES += libavformat_plugin.la
endif

libes_plugin_la_SOURCES  = mpeg/es.c
libes_plugin_la_LIBADD = ../libvlc_dts_header.la
demux_LTLIBRARIES += libes_plugin.la

libh264_plugin_la_SOURCES = mpeg/h264.c
demux_LTLIBRARIES += libh264_plugin.la

libmkv_plugin_la_SOURCES = \
	mkv/util.hpp mkv/util.cpp \
	mkv/virtual_segment.hpp mkv/virtual_segment.cpp \
	mkv/matroska_segment.hpp mkv/matroska_segment.cpp \
	mkv/matroska_segment_parse.cpp \
	mkv/demux.hpp mkv/demux.cpp \
	mkv/Ebml_parser.hpp mkv/Ebml_parser.cpp \
	mkv/chapters.hpp mkv/chapters.cpp \
	mkv/chapter_command.hpp mkv/chapter_command.cpp \
	mkv/stream_io_callback.hpp mkv/stream_io_callback.cpp \
	mp4/libmp4.c vobsub.h \
	mkv/mkv.hpp mkv/mkv.cpp
libmkv_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libmkv_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libmkv_plugin_la_LIBADD = $(LIBS_mkv)
if HAVE_ZLIB
libmkv_plugin_la_LIBADD += -lz
endif
demux_LTLIBRARIES += $(LTLIBmkv)
EXTRA_LTLIBRARIES += libmkv_plugin.la

libmp4_plugin_la_SOURCES = mp4/mp4.c mp4/libmp4.c mp4/libmp4.h mp4/id3genres.h
libmp4_plugin_la_LIBADD = $(LIBM)
libmp4_plugin_la_LDFLAGS = $(AM_LDFLAGS)
if HAVE_ZLIB
libmp4_plugin_la_LIBADD += -lz
endif
demux_LTLIBRARIES += libmp4_plugin.la

libmpgv_plugin_la_SOURCES = mpeg/mpgv.c
demux_LTLIBRARIES += libmpgv_plugin.la

libplaylist_plugin_la_SOURCES = \
	playlist/asx.c \
	playlist/b4s.c \
	playlist/dvb.c \
	playlist/gvp.c \
	playlist/ifo.c \
	playlist/itml.c \
	playlist/itml.h \
	playlist/m3u.c \
	playlist/pls.c \
	playlist/podcast.c \
	playlist/qtl.c \
	playlist/ram.c \
	playlist/sgimb.c \
	playlist/shoutcast.c \
	playlist/wpl.c \
	playlist/xspf.c \
	playlist/zpl.c \
	playlist/playlist.c playlist/playlist.h
demux_LTLIBRARIES += libplaylist_plugin.la

libts_plugin_la_SOURCES = ts.c ../mux/mpeg/csa.c ../mux/mpeg/dvbpsi_compat.h dvb-text.h
libts_plugin_la_CFLAGS = $(AM_CFLAGS) $(DVBPSI_CFLAGS)
libts_plugin_la_LIBADD = $(DVBPSI_LIBS) $(SOCKET_LIBS)
if HAVE_WIN32
libts_plugin_la_DEPENDENCIES = libts_plugin.rc.o
endif
if HAVE_DVBPSI
demux_LTLIBRARIES += libts_plugin.la
endif

BUILT_SOURCES += dummy.cpp

dummy.cpp:
	touch dummy.cpp
