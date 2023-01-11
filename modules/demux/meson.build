#
# Demux modules
#

# Common Xiph metadata library
xiph_meta_lib = static_library('xiph_metadata',
    sources: files('xiph_metadata.c'),
    include_directories: [vlc_include_dirs],
    install: false,
    pic: true
)

# FLAC demux
vlc_modules += {
    'name' : 'flacsys',
    'sources' : files('flac.c'),
    'include_directories' : include_directories('../packetizer'),
    'link_with' : [xiph_meta_lib]
}

# OGG demux
ogg_dep = dependency('ogg', version: '>= 1.0', required: get_option('ogg'))
if ogg_dep.found()
    vlc_modules += {
        'name' : 'ogg',
        'sources' : files('ogg.c', 'oggseek.c', 'ogg_granule.c'),
        'link_with' : [xiph_meta_lib],
        'dependencies' : [ogg_dep]
    }
endif

# Xiph test
xiph_test = executable('xiph_test',
    files('xiph_test.c'),
    include_directories: [vlc_include_dirs])

test('xiph_test', hpack_test, suite: 'demux')

# Demux dump
vlc_modules += {
    'name' : 'demuxdump',
    'sources' : files('demuxdump.c')
}

# Raw DV demux
vlc_modules += {
    'name' : 'rawdv',
    'sources' : files('rawdv.c')
}

# Raw Vid demux
vlc_modules += {
    'name' : 'rawvid',
    'sources' : files('rawvid.c')
}

# au demux
vlc_modules += {
    'name' : 'au',
    'sources' : files('au.c')
}

# Raw AUD demux
vlc_modules += {
    'name' : 'rawaud',
    'sources' : files('rawaud.c')
}

# WAV demux module
vlc_modules += {
    'name' : 'wav',
    'sources' : files('wav.c'),
}

# NSV demux
vlc_modules += {
    'name' : 'nsv',
    'sources' : files('nsv.c')
}

# HX raw video/audio IP camera demuxer
vlc_modules += {
    'name' : 'hx',
    'sources' : files('hx.c'),
}

# MPEG PS demux
vlc_modules += {
    'name' : 'ps',
    'sources' : files('mpeg/ps.c')
}

# libmodplug
libmodplug_dep = dependency('libmodplug', version: ['>= 0.8.4', '!= 0.8.8'],
    required: get_option('libmodplug'))
if libmodplug_dep.found()
    vlc_modules += {
        'name' : 'mod',
        'sources' : files('mod.c'),
        'dependencies' : [libmodplug_dep]
    }
endif

# PVA demux
vlc_modules += {
    'name' : 'pva',
    'sources' : files('pva.c')
}

# AIFF demux
vlc_modules += {
    'name' : 'aiff',
    'sources' : files('aiff.c')
}

# MJPEG demux
vlc_modules += {
    'name' : 'mjpeg',
    'sources' : files('mjpeg.c')
}

# Subtitle demux
vlc_modules += {
    'name' : 'subtitle',
    'sources' : files('subtitle.c'),
    'dependencies' : [m_lib]
}

# ty demux
vlc_modules += {
    'name' : 'ty',
    'sources' : files('ty.c')
}

# vobsub demux
vlc_modules += {
    'name' : 'vobsub',
    'sources' : files('vobsub.c')
}

# voc demux
vlc_modules += {
    'name' : 'voc',
    'sources' : files('voc.c')
}

# xa demux
vlc_modules += {
    'name' : 'xa',
    'sources' : files('xa.c')
}

# nuv demux
vlc_modules += {
    'name' : 'nuv',
    'sources' : files('nuv.c')
}

# tta demux
vlc_modules += {
    'name' : 'tta',
    'sources' : files('tta.c')
}

# VC1 demux
vlc_modules += {
    'name' : 'vc1',
    'sources' : files('vc1.c')
}

# CDG demux
vlc_modules += {
    'name' : 'demux_cdg',
    'sources' : files('cdg.c')
}

# SMF demux
vlc_modules += {
    'name' : 'smf',
    'sources' : files('smf.c')
}

# DMX Audio Music demux
vlc_modules += {
    'name' : 'dmxmus',
    'sources' : files('dmxmus.c')
}

# Image demux
vlc_modules += {
    'name' : 'image',
    'sources' : files('image.c')
}

# STL demux
vlc_modules += {
    'name' : 'demux_stl',
    'sources' : files('stl.c')
}

# ASF demux
vlc_modules += {
    'name' : 'asf',
    'sources' : files('asf/asf.c', 'asf/libasf.c', 'asf/asfpacket.c')
}

# AVI demux
vlc_modules += {
    'name' : 'avi',
    'sources' : files('avi/avi.c', 'avi/libavi.c')
}

# CAF demux
vlc_modules += {
    'name' : 'caf',
    'sources' : files('caf.c'),
    'dependencies' : [m_lib]
}

# FFmpeg AVFormat demuxer
if avformat_dep.found()
    avformat_extra_sources = []
    if get_option('stream_outputs')
        avformat_extra_sources += 'avformat/mux.c'
    endif

    vlc_modules += {
        'name' : 'avformat',
        'sources' : files(
            'avformat/demux.c',
            'avformat/avformat.c',
            avformat_extra_sources
        ),
        'dependencies' : [avformat_dep, avutil_dep],
        'link_with' : [libavcodec_common]
    }
endif

# Directory demuxer
vlc_modules += {
    'name' : 'directory_demux',
    'sources' : files('directory.c')
}

# ES demux
vlc_modules += {
    'name' : 'es',
    'sources' : files('mpeg/es.c', '../packetizer/dts_header.c')
}

# h.26x demux
vlc_modules += {
    'name' : 'h26x',
    'sources' : files('mpeg/h26x.c', '../packetizer/h264_nal.c')
}

# MKV demux
libebml_dep = dependency('libebml', required: get_option('matroska'))
libmatroska_dep = dependency('libmatroska', required: get_option('matroska'))
if libebml_dep.found() and libmatroska_dep.found()
    vlc_modules += {
        'name' : 'mkv',
        'sources' : files(
            'mkv/mkv.cpp',
            'mkv/util.cpp',
            'mkv/virtual_segment.cpp',
            'mkv/matroska_segment.cpp',
            'mkv/matroska_segment_parse.cpp',
            'mkv/matroska_segment_seeker.cpp',
            'mkv/demux.cpp',
            'mkv/events.cpp',
            'mkv/Ebml_parser.cpp',
            'mkv/chapters.cpp',
            'mkv/chapter_command.cpp',
            'mkv/stream_io_callback.cpp',
            'mp4/libmp4.c',
            '../packetizer/dts_header.c',
        ),
        'dependencies' : [libebml_dep, libmatroska_dep, z_lib]
    }
endif

# MP4 demux
vlc_modules += {
    'name' : 'mp4',
    'sources' : files(
        'mp4/mp4.c',
        'mp4/fragments.c',
        'mp4/libmp4.c',
        'mp4/heif.c',
        'mp4/essetup.c',
        'mp4/meta.c',
        'asf/asfpacket.c',
        'mp4/attachments.c',
    ),
    'dependencies' : [m_lib, z_lib]
}


# mpgv demux
vlc_modules += {
    'name' : 'mpgv',
    'sources' : files('mpeg/mpgv.c')
}

# Playlist demux
vlc_modules += {
    'name' : 'playlist',
    'sources' : files(
        'playlist/asx.c',
        'playlist/b4s.c',
        'playlist/bdmv.c',
        'playlist/dvb.c',
        'playlist/ifo.c',
        'playlist/itml.c',
        'playlist/m3u.c',
        'playlist/pls.c',
        'playlist/podcast.c',
        'playlist/qtl.c',
        'playlist/ram.c',
        'playlist/sgimb.c',
        'playlist/wpl.c',
        'playlist/wms.c',
        'playlist/xspf.c',
        'playlist/playlist.c')
}

# TS demux
aribb24_dep = dependency('aribb24', version: '>= 1.0.1', required: get_option('aribb24'))
libdvbpsi_dep = dependency('libdvbpsi', version: '>= 1.2.0', required: get_option('libdvbpsi'))
if libdvbpsi_dep.found()
    arrib24_define = []
    if aribb24_dep.found()
        arrib24_define += '-DHAVE_ARIBB24=1'
    endif

    vlc_modules += {
        'name' : 'ts',
        'sources' : files(
            'mpeg/ts.c',
            'mpeg/ts_pes.c',
            'mpeg/ts_pid.c',
            'mpeg/ts_psi.c',
            'mpeg/ts_si.c',
            'mpeg/ts_psip.c',
            'mpeg/ts_psip_dvbpsi_fixes.c',
            'mpeg/ts_decoders.c',
            'mpeg/ts_streams.c',
            'mpeg/ts_scte.c',
            'mpeg/sections.c',
            'mpeg/mpeg4_iod.c',
            'mpeg/ts_arib.c',
            'mpeg/ts_sl.c',
            'mpeg/ts_metadata.c',
            'mpeg/ts_hotfixes.c',
            '../mux/mpeg/csa.c',
            '../mux/mpeg/tables.c',
            '../mux/mpeg/tsutil.c',
            '../codec/atsc_a65.c',
            '../codec/opus_header.c',
        ),
        'dependencies' : [libdvbpsi_dep, aribb24_dep],
        'c_args' : arrib24_define
    }
endif

# Adaptive streams demuxer

vlc_modules += {
    'name' : 'adaptive',
    'sources' : files(
        # DASH specific sources
        'dash/mpd/AdaptationSet.cpp',
        'dash/mpd/AdaptationSet.h',
        'dash/mpd/DASHCommonAttributesElements.cpp',
        'dash/mpd/DASHCommonAttributesElements.h',
        'dash/mpd/DASHSegment.cpp',
        'dash/mpd/DASHSegment.h',
        'dash/mpd/ContentDescription.cpp',
        'dash/mpd/ContentDescription.h',
        'dash/mpd/IsoffMainParser.cpp',
        'dash/mpd/IsoffMainParser.h',
        'dash/mpd/MPD.cpp',
        'dash/mpd/MPD.h',
        'dash/mpd/Profile.cpp',
        'dash/mpd/Profile.hpp',
        'dash/mpd/ProgramInformation.cpp',
        'dash/mpd/ProgramInformation.h',
        'dash/mpd/Representation.cpp',
        'dash/mpd/Representation.h',
        'dash/mpd/TemplatedUri.cpp',
        'dash/mpd/TemplatedUri.hpp',
        'dash/mp4/IndexReader.cpp',
        'dash/mp4/IndexReader.hpp',
        'dash/DASHManager.cpp',
        'dash/DASHManager.h',
        'dash/DASHStream.cpp',
        'dash/DASHStream.hpp',

        # HLS specific sources
        'hls/playlist/M3U8.hpp',
        'hls/playlist/M3U8.cpp',
        'hls/playlist/Parser.hpp',
        'hls/playlist/Parser.cpp',
        'hls/playlist/HLSRepresentation.hpp',
        'hls/playlist/HLSRepresentation.cpp',
        'hls/playlist/HLSSegment.hpp',
        'hls/playlist/HLSSegment.cpp',
        'hls/playlist/Tags.hpp',
        'hls/playlist/Tags.cpp',
        'hls/HLSManager.hpp',
        'hls/HLSManager.cpp',
        'hls/HLSStreams.hpp',
        'hls/HLSStreams.cpp',
        'mpeg/timestamps.h',

        '../meta_engine/ID3Meta.h',

        # Smooth specific sources
        'smooth/mp4/SmoothIndexReader.cpp',
        'smooth/mp4/SmoothIndexReader.hpp',
        'smooth/playlist/CodecParameters.cpp',
        'smooth/playlist/CodecParameters.hpp',
        'smooth/playlist/ForgedInitSegment.hpp',
        'smooth/playlist/ForgedInitSegment.cpp',
        'smooth/playlist/Manifest.hpp',
        'smooth/playlist/Manifest.cpp',
        'smooth/playlist/MemoryChunk.hpp',
        'smooth/playlist/MemoryChunk.cpp',
        'smooth/playlist/SmoothParser.hpp',
        'smooth/playlist/SmoothParser.cpp',
        'smooth/playlist/QualityLevel.cpp',
        'smooth/playlist/QualityLevel.hpp',
        'smooth/playlist/SmoothSegment.hpp',
        'smooth/playlist/SmoothSegment.cpp',
        'smooth/SmoothManager.hpp',
        'smooth/SmoothManager.cpp',
        'smooth/SmoothStream.hpp',
        'smooth/SmoothStream.cpp',

        '../mux/mp4/libmp4mux.c',
        '../mux/mp4/libmp4mux.h',
        '../packetizer/h264_nal.c',
        '../packetizer/hevc_nal.c',

        # General adaptive sources

        # TODO: Remove this source once the adaptive
        # sources are move to a convenience library
        # (needed for tests)
        'adaptive/adaptive.cpp',

        'adaptive/playlist/BaseAdaptationSet.cpp',
        'adaptive/playlist/BaseAdaptationSet.h',
        'adaptive/playlist/BasePeriod.cpp',
        'adaptive/playlist/BasePeriod.h',
        'adaptive/playlist/BasePlaylist.cpp',
        'adaptive/playlist/BasePlaylist.hpp',
        'adaptive/playlist/BaseRepresentation.cpp',
        'adaptive/playlist/BaseRepresentation.h',
        'adaptive/playlist/CodecDescription.cpp',
        'adaptive/playlist/CodecDescription.hpp',
        'adaptive/playlist/CommonAttributesElements.cpp',
        'adaptive/playlist/CommonAttributesElements.h',
        'adaptive/playlist/ICanonicalUrl.hpp',
        'adaptive/playlist/Inheritables.hpp',
        'adaptive/playlist/Inheritables.cpp',
        'adaptive/playlist/Role.hpp',
        'adaptive/playlist/Role.cpp',
        'adaptive/playlist/Segment.cpp',
        'adaptive/playlist/Segment.h',
        'adaptive/playlist/SegmentBase.cpp',
        'adaptive/playlist/SegmentBase.h',
        'adaptive/playlist/SegmentBaseType.cpp',
        'adaptive/playlist/SegmentBaseType.hpp',
        'adaptive/playlist/SegmentChunk.cpp',
        'adaptive/playlist/SegmentChunk.hpp',
        'adaptive/playlist/SegmentList.cpp',
        'adaptive/playlist/SegmentList.h',
        'adaptive/playlist/SegmentTimeline.cpp',
        'adaptive/playlist/SegmentTimeline.h',
        'adaptive/playlist/SegmentInformation.cpp',
        'adaptive/playlist/SegmentInformation.hpp',
        'adaptive/playlist/SegmentTemplate.cpp',
        'adaptive/playlist/SegmentTemplate.h',
        'adaptive/playlist/Url.cpp',
        'adaptive/playlist/Url.hpp',
        'adaptive/playlist/Templates.hpp',
        'adaptive/encryption/CommonEncryption.cpp',
        'adaptive/encryption/CommonEncryption.hpp',
        'adaptive/encryption/Keyring.cpp',
        'adaptive/encryption/Keyring.hpp',
        'adaptive/logic/AbstractAdaptationLogic.cpp',
        'adaptive/logic/AbstractAdaptationLogic.h',
        'adaptive/logic/AlwaysBestAdaptationLogic.cpp',
        'adaptive/logic/AlwaysBestAdaptationLogic.h',
        'adaptive/logic/AlwaysLowestAdaptationLogic.cpp',
        'adaptive/logic/AlwaysLowestAdaptationLogic.hpp',
        'adaptive/logic/BufferingLogic.cpp',
        'adaptive/logic/BufferingLogic.hpp',
        'adaptive/logic/IDownloadRateObserver.h',
        'adaptive/logic/NearOptimalAdaptationLogic.cpp',
        'adaptive/logic/NearOptimalAdaptationLogic.hpp',
        'adaptive/logic/PredictiveAdaptationLogic.hpp',
        'adaptive/logic/PredictiveAdaptationLogic.cpp',
        'adaptive/logic/RateBasedAdaptationLogic.h',
        'adaptive/logic/RateBasedAdaptationLogic.cpp',
        'adaptive/logic/Representationselectors.hpp',
        'adaptive/logic/Representationselectors.cpp',
        'adaptive/logic/RoundRobinLogic.cpp',
        'adaptive/logic/RoundRobinLogic.hpp',
        'adaptive/mp4/AtomsReader.cpp',
        'adaptive/mp4/AtomsReader.hpp',
        'adaptive/http/AuthStorage.cpp',
        'adaptive/http/AuthStorage.hpp',
        'adaptive/http/BytesRange.cpp',
        'adaptive/http/BytesRange.hpp',
        'adaptive/http/Chunk.cpp',
        'adaptive/http/Chunk.h',
        'adaptive/http/ConnectionParams.cpp',
        'adaptive/http/ConnectionParams.hpp',
        'adaptive/http/Downloader.cpp',
        'adaptive/http/Downloader.hpp',
        'adaptive/http/HTTPConnection.cpp',
        'adaptive/http/HTTPConnection.hpp',
        'adaptive/http/HTTPConnectionManager.cpp',
        'adaptive/http/HTTPConnectionManager.h',
        'adaptive/plumbing/CommandsQueue.cpp',
        'adaptive/plumbing/CommandsQueue.hpp',
        'adaptive/plumbing/Demuxer.cpp',
        'adaptive/plumbing/Demuxer.hpp',
        'adaptive/plumbing/FakeESOut.cpp',
        'adaptive/plumbing/FakeESOut.hpp',
        'adaptive/plumbing/FakeESOutID.cpp',
        'adaptive/plumbing/FakeESOutID.hpp',
        'adaptive/plumbing/SourceStream.cpp',
        'adaptive/plumbing/SourceStream.hpp',
        'adaptive/AbstractSource.hpp',
        'adaptive/ID.hpp',
        'adaptive/ID.cpp',
        'adaptive/PlaylistManager.cpp',
        'adaptive/PlaylistManager.h',
        'adaptive/SegmentTracker.cpp',
        'adaptive/SegmentTracker.hpp',
        'adaptive/SharedResources.cpp',
        'adaptive/SharedResources.hpp',
        'adaptive/StreamFormat.cpp',
        'adaptive/StreamFormat.hpp',
        'adaptive/Streams.cpp',
        'adaptive/Streams.hpp',
        'adaptive/Time.hpp',
        'adaptive/tools/Conversions.hpp',
        'adaptive/tools/Conversions.cpp',
        'adaptive/tools/Debug.hpp',
        'adaptive/tools/FormatNamespace.cpp',
        'adaptive/tools/FormatNamespace.hpp',
        'adaptive/tools/Helper.cpp',
        'adaptive/tools/Helper.h',
        'adaptive/tools/MovingAverage.hpp',
        'adaptive/tools/Properties.hpp',
        'adaptive/tools/Retrieve.cpp',
        'adaptive/tools/Retrieve.hpp',
        'adaptive/xml/DOMHelper.cpp',
        'adaptive/xml/DOMHelper.h',
        'adaptive/xml/DOMParser.cpp',
        'adaptive/xml/DOMParser.h',
        'adaptive/xml/Node.cpp',
        'adaptive/xml/Node.h',

        'mp4/libmp4.c',
        'mp4/libmp4.h',
        '../meta_engine/ID3Tag.h',

    ),
    'include_directories' : include_directories('adaptive', './'),
    'dependencies' : [socket_libs, m_lib, z_lib],
    'link_with': vlc_http_lib,
    # TODO: Add optional GCRYPT dependency!
}

# noseek demux
vlc_modules += {
    'name' : 'noseek',
    'sources' : files('filter/noseek.c')
}
