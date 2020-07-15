demuxdir = $(pluginsdir)/demux
demux_LTLIBRARIES =

libxiph_metadata_la_SOURCES = demux/xiph_metadata.h demux/xiph_metadata.c
libxiph_metadata_la_LDFLAGS = -static
noinst_LTLIBRARIES += libxiph_metadata.la

libflacsys_plugin_la_SOURCES = demux/flac.c packetizer/flac.h
libflacsys_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libflacsys_plugin_la_LIBADD = libxiph_metadata.la
demux_LTLIBRARIES += libflacsys_plugin.la

libogg_plugin_la_SOURCES = demux/ogg.c demux/ogg.h \
                           demux/oggseek.c demux/oggseek.h \
                           demux/ogg_granule.c demux/ogg_granule.h \
                           demux/xiph.h demux/opus.h
libogg_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(LIBVORBIS_CFLAGS) $(OGG_CFLAGS)
libogg_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libogg_plugin_la_LIBADD = $(LIBVORBIS_LIBS) $(OGG_LIBS) libxiph_metadata.la
EXTRA_LTLIBRARIES += libogg_plugin.la
demux_LTLIBRARIES += $(LTLIBogg)

xiph_test_SOURCES = demux/xiph_test.c demux/xiph.h
check_PROGRAMS += xiph_test
TESTS += xiph_test

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

libps_plugin_la_SOURCES = demux/mpeg/ps.c demux/mpeg/ps.h demux/mpeg/pes.h
demux_LTLIBRARIES += libps_plugin.la

libmod_plugin_la_SOURCES = demux/mod.c
libmod_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_mod)
libmod_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libmod_plugin_la_LIBADD = $(LIBS_mod)
EXTRA_LTLIBRARIES += libmod_plugin.la
demux_LTLIBRARIES += $(LTLIBmod)

libpva_plugin_la_SOURCES = demux/pva.c demux/mpeg/pes.h demux/mpeg/timestamps.h
demux_LTLIBRARIES += libpva_plugin.la

libaiff_plugin_la_SOURCES = demux/aiff.c
demux_LTLIBRARIES += libaiff_plugin.la

libmjpeg_plugin_la_SOURCES = demux/mjpeg.c demux/mxpeg_helper.h
demux_LTLIBRARIES += libmjpeg_plugin.la

libsubtitle_plugin_la_SOURCES = demux/subtitle.c
libsubtitle_plugin_la_LIBADD = $(LIBM)
demux_LTLIBRARIES += libsubtitle_plugin.la

libty_plugin_la_SOURCES = demux/ty.c codec/cc.h \
                          demux/mpeg/pes.h demux/mpeg/timestamps.h
demux_LTLIBRARIES += libty_plugin.la

libvobsub_plugin_la_SOURCES = demux/vobsub.c demux/vobsub.h \
                              demux/mpeg/ps.h demux/mpeg/pes.h \
                              demux/subtitle_helper.h
demux_LTLIBRARIES += libvobsub_plugin.la

libvoc_plugin_la_SOURCES = demux/voc.c
demux_LTLIBRARIES += libvoc_plugin.la

libxa_plugin_la_SOURCES = demux/xa.c
demux_LTLIBRARIES += libxa_plugin.la

libnuv_plugin_la_SOURCES = demux/nuv.c
libnuv_plugin_la_LIBADD = $(LIBM)
demux_LTLIBRARIES += libnuv_plugin.la

libnsc_plugin_la_SOURCES = demux/nsc.c
demux_LTLIBRARIES += libnsc_plugin.la

libmpc_plugin_la_SOURCES = demux/mpc.c
libmpc_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libmpc_plugin_la_LIBADD = $(LIBS_mpc) $(LIBM)
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

libdmxmus_plugin_la_SOURCES = demux/dmxmus.c
demux_LTLIBRARIES += libdmxmus_plugin.la

libgme_plugin_la_SOURCES = demux/gme.c
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

libimage_plugin_la_SOURCES = demux/image.c demux/mxpeg_helper.h
demux_LTLIBRARIES += libimage_plugin.la

libdemux_stl_plugin_la_SOURCES = demux/stl.c
libdemux_stl_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
demux_LTLIBRARIES += libdemux_stl_plugin.la

libasf_plugin_la_SOURCES = demux/asf/asf.c demux/asf/libasf.c demux/asf/libasf.h \
                           demux/asf/asfpacket.c demux/asf/asfpacket.h \
                           demux/asf/libasf_guid.h
demux_LTLIBRARIES += libasf_plugin.la

libavi_plugin_la_SOURCES = demux/avi/avi.c demux/avi/libavi.c demux/avi/libavi.h \
                           demux/avi/bitmapinfoheader.h
demux_LTLIBRARIES += libavi_plugin.la

libcaf_plugin_la_SOURCES = demux/caf.c
libcaf_plugin_la_LIBADD = $(LIBM)
demux_LTLIBRARIES += libcaf_plugin.la

libavformat_plugin_la_SOURCES = demux/avformat/demux.c \
	demux/vobsub.h \
	demux/avformat/avformat.c demux/avformat/avformat.h
if ENABLE_SOUT
libavformat_plugin_la_SOURCES += demux/avformat/mux.c
endif
libavformat_plugin_la_CFLAGS = $(AM_CFLAGS) $(AVFORMAT_CFLAGS) $(AVUTIL_CFLAGS)
libavformat_plugin_la_LIBADD = $(AVFORMAT_LIBS) $(AVUTIL_LIBS) $(LIBM) libavcodec_common.la
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
noinst_LTLIBRARIES += libavcodec_common.la
endif
endif

libdirectory_demux_plugin_la_SOURCES = demux/directory.c
demux_LTLIBRARIES += libdirectory_demux_plugin.la

libes_plugin_la_SOURCES  = demux/mpeg/es.c \
                           meta_engine/ID3Tag.h \
                           meta_engine/ID3Text.h \
                           packetizer/dts_header.c packetizer/dts_header.h
demux_LTLIBRARIES += libes_plugin.la

libh26x_plugin_la_SOURCES = demux/mpeg/h26x.c \
                            packetizer/h264_nal.c packetizer/hevc_nal.h
libh26x_plugin_la_LIBADD = $(LIBM)
demux_LTLIBRARIES += libh26x_plugin.la

libmkv_plugin_la_SOURCES = \
	demux/mkv/util.hpp demux/mkv/util.cpp \
	demux/mkv/dvd_types.hpp \
	demux/mkv/virtual_segment.hpp demux/mkv/virtual_segment.cpp \
	demux/mkv/matroska_segment.hpp demux/mkv/matroska_segment.cpp \
	demux/mkv/matroska_segment_parse.cpp \
	demux/mkv/matroska_segment_seeker.hpp demux/mkv/matroska_segment_seeker.cpp \
	demux/mkv/demux.hpp demux/mkv/demux.cpp \
	demux/mkv/events.hpp demux/mkv/events.cpp \
	demux/mkv/dispatcher.hpp \
	demux/mkv/string_dispatcher.hpp \
	demux/mkv/Ebml_parser.hpp demux/mkv/Ebml_parser.cpp \
	demux/mkv/Ebml_dispatcher.hpp \
	demux/mkv/chapters.hpp demux/mkv/chapters.cpp \
	demux/mkv/chapter_command.hpp demux/mkv/chapter_command.cpp \
	demux/mkv/stream_io_callback.hpp demux/mkv/stream_io_callback.cpp \
	demux/mp4/libmp4.c demux/vobsub.h \
	demux/mkv/mkv.hpp demux/mkv/mkv.cpp \
        demux/av1_unpack.h codec/webvtt/helpers.h \
	demux/windows_audio_commons.h
libmkv_plugin_la_SOURCES += packetizer/dts_header.h packetizer/dts_header.c
libmkv_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CFLAGS_mkv)
libmkv_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
libmkv_plugin_la_LIBADD = $(LIBS_mkv)
if HAVE_ZLIB
libmkv_plugin_la_LIBADD += -lz
endif
demux_LTLIBRARIES += $(LTLIBmkv)
EXTRA_LTLIBRARIES += libmkv_plugin.la

libmp4_plugin_la_SOURCES = demux/mp4/mp4.c demux/mp4/mp4.h \
                           demux/mp4/fragments.c demux/mp4/fragments.h \
                           demux/mp4/libmp4.c demux/mp4/libmp4.h \
                           demux/mp4/attachments.c demux/mp4/attachments.h \
                           demux/mp4/languages.h \
                           demux/mp4/heif.c demux/mp4/heif.h \
                           demux/mp4/avci.h \
                           demux/mp4/essetup.c \
                           demux/mp4/meta.c \
                           demux/mp4/mpeg4.h \
                           demux/mp4/coreaudio.h \
                           demux/av1_unpack.h \
                           demux/asf/asfpacket.c demux/asf/asfpacket.h \
                           packetizer/iso_color_tables.h \
                           meta_engine/ID3Genres.h
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
	demux/playlist/bdmv.c \
	demux/playlist/dvb.c \
	demux/playlist/ifo.c \
	demux/playlist/itml.c \
	demux/playlist/itml.h \
	demux/playlist/m3u.c \
	demux/playlist/pls.c \
	demux/playlist/podcast.c \
	demux/playlist/qtl.c \
	demux/playlist/ram.c \
	demux/playlist/sgimb.c \
	demux/playlist/wms.c \
	demux/playlist/wpl.c \
	demux/playlist/xspf.c \
	demux/playlist/playlist.c demux/playlist/playlist.h
demux_LTLIBRARIES += libplaylist_plugin.la

libts_plugin_la_SOURCES = demux/mpeg/ts.c demux/mpeg/ts.h \
        demux/mpeg/ts_pid.h demux/mpeg/ts_pid_fwd.h demux/mpeg/ts_pid.c \
        demux/mpeg/ts_psi.h demux/mpeg/ts_psi.c \
        demux/mpeg/ts_si.h demux/mpeg/ts_si.c \
        demux/mpeg/ts_psip.h demux/mpeg/ts_psip.c \
        demux/mpeg/ts_psip_dvbpsi_fixes.h demux/mpeg/ts_psip_dvbpsi_fixes.c \
        demux/mpeg/ts_decoders.h demux/mpeg/ts_decoders.c \
        demux/mpeg/ts_streams.h demux/mpeg/ts_streams.c \
        demux/mpeg/ts_scte.h demux/mpeg/ts_scte.c \
        demux/mpeg/sections.c demux/mpeg/sections.h \
        demux/mpeg/mpeg4_iod.c demux/mpeg/mpeg4_iod.h \
        demux/mpeg/ts_arib.c demux/mpeg/ts_arib.h \
        demux/mpeg/ts_sl.c demux/mpeg/ts_sl.h \
        demux/mpeg/ts_metadata.c demux/mpeg/ts_metadata.h \
        demux/mpeg/ts_hotfixes.c demux/mpeg/ts_hotfixes.h \
        demux/mpeg/ts_strings.h demux/mpeg/ts_streams_private.h \
        demux/mpeg/ts_pes.c demux/mpeg/ts_pes.h \
        demux/mpeg/pes.h \
        demux/mpeg/timestamps.h \
	demux/mpeg/ts_descriptions.h \
        demux/dvb-text.h \
        demux/opus.h \
	mux/mpeg/csa.c \
        mux/mpeg/dvbpsi_compat.h \
	mux/mpeg/streams.h \
        mux/mpeg/tables.c mux/mpeg/tables.h \
	mux/mpeg/tsutil.c mux/mpeg/tsutil.h \
        access/dtv/en50221_capmt.h \
        codec/jpeg2000.h codec/scte18.h \
        codec/atsc_a65.c codec/atsc_a65.h \
	codec/opus_header.c
libts_plugin_la_CFLAGS = $(AM_CFLAGS) $(DVBPSI_CFLAGS)
libts_plugin_la_LIBADD = $(DVBPSI_LIBS) $(SOCKET_LIBS)
if HAVE_ARIBB24
libts_plugin_la_CFLAGS += $(ARIBB24_CFLAGS)
libts_plugin_la_LIBADD += $(ARIBB24_LIBS)
endif
if HAVE_DVBPSI
demux_LTLIBRARIES += libts_plugin.la
endif

libadaptive_plugin_la_SOURCES = \
    demux/adaptive/playlist/AbstractPlaylist.cpp \
    demux/adaptive/playlist/AbstractPlaylist.hpp \
    demux/adaptive/playlist/BaseAdaptationSet.cpp \
    demux/adaptive/playlist/BaseAdaptationSet.h \
    demux/adaptive/playlist/BasePeriod.cpp \
    demux/adaptive/playlist/BasePeriod.h \
    demux/adaptive/playlist/BaseRepresentation.cpp \
    demux/adaptive/playlist/BaseRepresentation.h \
    demux/adaptive/playlist/CommonAttributesElements.cpp \
    demux/adaptive/playlist/CommonAttributesElements.h \
    demux/adaptive/playlist/ICanonicalUrl.hpp \
    demux/adaptive/playlist/Inheritables.hpp \
    demux/adaptive/playlist/Inheritables.cpp \
    demux/adaptive/playlist/Role.hpp \
    demux/adaptive/playlist/Role.cpp \
    demux/adaptive/playlist/Segment.cpp \
    demux/adaptive/playlist/Segment.h \
    demux/adaptive/playlist/SegmentBase.cpp \
    demux/adaptive/playlist/SegmentBase.h \
    demux/adaptive/playlist/SegmentChunk.cpp \
    demux/adaptive/playlist/SegmentChunk.hpp \
    demux/adaptive/playlist/SegmentInfoCommon.cpp \
    demux/adaptive/playlist/SegmentInfoCommon.h \
    demux/adaptive/playlist/SegmentList.cpp \
    demux/adaptive/playlist/SegmentList.h \
    demux/adaptive/playlist/SegmentTimeline.cpp \
    demux/adaptive/playlist/SegmentTimeline.h \
    demux/adaptive/playlist/SegmentInformation.cpp \
    demux/adaptive/playlist/SegmentInformation.hpp \
    demux/adaptive/playlist/SegmentTemplate.cpp \
    demux/adaptive/playlist/SegmentTemplate.h \
    demux/adaptive/playlist/Url.cpp \
    demux/adaptive/playlist/Url.hpp \
    demux/adaptive/playlist/Templates.hpp \
    demux/adaptive/encryption/CommonEncryption.cpp \
    demux/adaptive/encryption/CommonEncryption.hpp \
    demux/adaptive/encryption/Keyring.cpp \
    demux/adaptive/encryption/Keyring.hpp \
    demux/adaptive/logic/AbstractAdaptationLogic.cpp \
    demux/adaptive/logic/AbstractAdaptationLogic.h \
    demux/adaptive/logic/AlwaysBestAdaptationLogic.cpp \
    demux/adaptive/logic/AlwaysBestAdaptationLogic.h \
    demux/adaptive/logic/AlwaysLowestAdaptationLogic.cpp \
    demux/adaptive/logic/AlwaysLowestAdaptationLogic.hpp \
    demux/adaptive/logic/BufferingLogic.cpp \
    demux/adaptive/logic/BufferingLogic.hpp \
    demux/adaptive/logic/IDownloadRateObserver.h \
    demux/adaptive/logic/NearOptimalAdaptationLogic.cpp \
    demux/adaptive/logic/NearOptimalAdaptationLogic.hpp \
    demux/adaptive/logic/PredictiveAdaptationLogic.hpp \
    demux/adaptive/logic/PredictiveAdaptationLogic.cpp \
    demux/adaptive/logic/RateBasedAdaptationLogic.h \
    demux/adaptive/logic/RateBasedAdaptationLogic.cpp \
    demux/adaptive/logic/Representationselectors.hpp \
    demux/adaptive/logic/Representationselectors.cpp \
    demux/adaptive/logic/RoundRobinLogic.cpp \
    demux/adaptive/logic/RoundRobinLogic.hpp \
    demux/adaptive/mp4/AtomsReader.cpp \
    demux/adaptive/mp4/AtomsReader.hpp \
    demux/adaptive/http/AuthStorage.cpp \
    demux/adaptive/http/AuthStorage.hpp \
    demux/adaptive/http/BytesRange.cpp \
    demux/adaptive/http/BytesRange.hpp \
    demux/adaptive/http/Chunk.cpp \
    demux/adaptive/http/Chunk.h \
    demux/adaptive/http/ConnectionParams.cpp \
    demux/adaptive/http/ConnectionParams.hpp \
    demux/adaptive/http/Downloader.cpp \
    demux/adaptive/http/Downloader.hpp \
    demux/adaptive/http/HTTPConnection.cpp \
    demux/adaptive/http/HTTPConnection.hpp \
    demux/adaptive/http/HTTPConnectionManager.cpp \
    demux/adaptive/http/HTTPConnectionManager.h \
    demux/adaptive/http/Transport.hpp \
    demux/adaptive/http/Transport.cpp \
    demux/adaptive/plumbing/CommandsQueue.cpp \
    demux/adaptive/plumbing/CommandsQueue.hpp \
    demux/adaptive/plumbing/Demuxer.cpp \
    demux/adaptive/plumbing/Demuxer.hpp \
    demux/adaptive/plumbing/FakeESOut.cpp \
    demux/adaptive/plumbing/FakeESOut.hpp \
    demux/adaptive/plumbing/FakeESOutID.cpp \
    demux/adaptive/plumbing/FakeESOutID.hpp \
    demux/adaptive/plumbing/SourceStream.cpp \
    demux/adaptive/plumbing/SourceStream.hpp \
    demux/adaptive/AbstractSource.hpp \
    demux/adaptive/ID.hpp \
    demux/adaptive/ID.cpp \
    demux/adaptive/PlaylistManager.cpp \
    demux/adaptive/PlaylistManager.h \
    demux/adaptive/SegmentTracker.cpp \
    demux/adaptive/SegmentTracker.hpp \
    demux/adaptive/SharedResources.cpp \
    demux/adaptive/SharedResources.hpp \
    demux/adaptive/StreamFormat.cpp \
    demux/adaptive/StreamFormat.hpp \
    demux/adaptive/Streams.cpp \
    demux/adaptive/Streams.hpp \
    demux/adaptive/Time.hpp \
    demux/adaptive/tools/Conversions.hpp \
    demux/adaptive/tools/Conversions.cpp \
    demux/adaptive/tools/Debug.hpp \
    demux/adaptive/tools/FormatNamespace.cpp \
    demux/adaptive/tools/FormatNamespace.hpp \
    demux/adaptive/tools/Helper.cpp \
    demux/adaptive/tools/Helper.h \
    demux/adaptive/tools/MovingAverage.hpp \
    demux/adaptive/tools/Properties.hpp \
    demux/adaptive/tools/Retrieve.cpp \
    demux/adaptive/tools/Retrieve.hpp \
    demux/adaptive/xml/DOMHelper.cpp \
    demux/adaptive/xml/DOMHelper.h \
    demux/adaptive/xml/DOMParser.cpp \
    demux/adaptive/xml/DOMParser.h \
    demux/adaptive/xml/Node.cpp \
    demux/adaptive/xml/Node.h
libadaptive_plugin_la_SOURCES += \
     demux/mp4/libmp4.c \
     demux/mp4/libmp4.h \
     meta_engine/ID3Tag.h

libadaptive_dash_SOURCES = \
    demux/dash/mpd/AdaptationSet.cpp \
    demux/dash/mpd/AdaptationSet.h \
    demux/dash/mpd/DASHCommonAttributesElements.cpp \
    demux/dash/mpd/DASHCommonAttributesElements.h \
    demux/dash/mpd/DASHSegment.cpp \
    demux/dash/mpd/DASHSegment.h \
    demux/dash/mpd/ContentDescription.cpp \
    demux/dash/mpd/ContentDescription.h \
    demux/dash/mpd/IsoffMainParser.cpp \
    demux/dash/mpd/IsoffMainParser.h \
    demux/dash/mpd/MPD.cpp \
    demux/dash/mpd/MPD.h \
    demux/dash/mpd/Period.cpp \
    demux/dash/mpd/Period.h \
    demux/dash/mpd/Profile.cpp \
    demux/dash/mpd/Profile.hpp \
    demux/dash/mpd/ProgramInformation.cpp \
    demux/dash/mpd/ProgramInformation.h \
    demux/dash/mpd/Representation.cpp \
    demux/dash/mpd/Representation.h \
    demux/dash/mpd/TemplatedUri.cpp \
    demux/dash/mpd/TemplatedUri.hpp \
    demux/dash/mp4/IndexReader.cpp \
    demux/dash/mp4/IndexReader.hpp \
    demux/dash/DASHManager.cpp \
    demux/dash/DASHManager.h \
    demux/dash/DASHStream.cpp \
    demux/dash/DASHStream.hpp

libadaptive_hls_SOURCES = \
    demux/hls/playlist/M3U8.hpp \
    demux/hls/playlist/M3U8.cpp \
    demux/hls/playlist/Parser.hpp \
    demux/hls/playlist/Parser.cpp \
    demux/hls/playlist/Representation.hpp \
    demux/hls/playlist/Representation.cpp \
    demux/hls/playlist/HLSSegment.hpp \
    demux/hls/playlist/HLSSegment.cpp \
    demux/hls/playlist/Tags.hpp \
    demux/hls/playlist/Tags.cpp \
    demux/hls/HLSManager.hpp \
    demux/hls/HLSManager.cpp \
    demux/hls/HLSStreams.hpp \
    demux/hls/HLSStreams.cpp \
    demux/mpeg/timestamps.h
libadaptive_hls_SOURCES += meta_engine/ID3Meta.h

libadaptive_smooth_SOURCES = \
    demux/smooth/mp4/IndexReader.cpp \
    demux/smooth/mp4/IndexReader.hpp \
    demux/smooth/playlist/ForgedInitSegment.hpp \
    demux/smooth/playlist/ForgedInitSegment.cpp \
    demux/smooth/playlist/Manifest.hpp \
    demux/smooth/playlist/Manifest.cpp \
    demux/smooth/playlist/MemoryChunk.hpp \
    demux/smooth/playlist/MemoryChunk.cpp \
    demux/smooth/playlist/Parser.hpp \
    demux/smooth/playlist/Parser.cpp \
    demux/smooth/playlist/Representation.hpp \
    demux/smooth/playlist/Representation.cpp \
    demux/smooth/playlist/SmoothSegment.hpp \
    demux/smooth/playlist/SmoothSegment.cpp \
    demux/smooth/SmoothManager.hpp \
    demux/smooth/SmoothManager.cpp \
    demux/smooth/SmoothStream.hpp \
    demux/smooth/SmoothStream.cpp
libadaptive_smooth_SOURCES += mux/mp4/libmp4mux.c mux/mp4/libmp4mux.h \
			      packetizer/h264_nal.c packetizer/hevc_nal.c

libadaptive_plugin_la_SOURCES += $(libadaptive_hls_SOURCES)
libadaptive_plugin_la_SOURCES += $(libadaptive_dash_SOURCES)
libadaptive_plugin_la_SOURCES += $(libadaptive_smooth_SOURCES)
libadaptive_plugin_la_SOURCES += demux/adaptive/adaptive.cpp
libadaptive_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) -I$(srcdir)/demux/adaptive
libadaptive_plugin_la_LIBADD = $(SOCKET_LIBS) $(LIBM)
if HAVE_ZLIB
libadaptive_plugin_la_LIBADD += -lz
endif
if HAVE_GCRYPT
libadaptive_plugin_la_CXXFLAGS += $(GCRYPT_CFLAGS)
libadaptive_plugin_la_LIBADD += $(GCRYPT_LIBS)
endif
demux_LTLIBRARIES += libadaptive_plugin.la

libnoseek_plugin_la_SOURCES = demux/filter/noseek.c
demux_LTLIBRARIES += libnoseek_plugin.la

libdemux_mock_plugin_la_SOURCES = demux/mock.c
libdemux_mock_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(demuxdir)'
noinst_LTLIBRARIES += libdemux_mock_plugin.la
