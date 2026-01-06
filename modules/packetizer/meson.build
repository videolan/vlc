# AV1 packetizer
vlc_modules += {
    'name' : 'packetizer_av1',
    'sources' : files('av1.c', 'av1_obu.c'),
    'shortname' : 'p_av1',
}

# Copy packetizer
vlc_modules += {
    'name' : 'packetizer_copy',
    'sources' : files('copy.c'),
    'shortname' : 'p_copy',
}

# MPEG-I/II video packetizer
vlc_modules += {
    'name' : 'packetizer_mpegvideo',
    'sources' : files('mpegvideo.c'),
    'shortname' : 'p_mpegvi',
}

# MPEG audio layer I/II/III packetizer
vlc_modules += {
    'name' : 'packetizer_mpegaudio',
    'sources' : files('mpegaudio.c'),
    'shortname' : 'p_mpegau',
}

# MPEG4 video packetizer
vlc_modules += {
    'name' : 'packetizer_mpeg4video',
    'sources' : files('mpeg4video.c'),
    'shortname' : 'p_mpeg4v',
}

# MPEG4 audio packetizer
vlc_modules += {
    'name' : 'packetizer_mpeg4audio',
    'sources' : files('mpeg4audio.c'),
    'shortname' : 'p_mpeg4a',
}

# MJPEG packetizer
vlc_modules += {
    'name' : 'packetizer_mjpeg',
    'sources' : files('mjpeg.c'),
    'shortname' : 'p_mjpeg',
}

# H.264 packetizer
vlc_modules += {
    'name' : 'packetizer_h264',
    'sources' : files(
        'h264.c',
        'h264_nal.c',
        'h264_slice.c',
        'hxxx_sei.c',
        'hxxx_common.c'
    ),
    'shortname' : 'p_h264',
}

# VC-1 packetizer
vlc_modules += {
    'name' : 'packetizer_vc1',
    'sources' : files('vc1.c'),
    'shortname' : 'p_vc1',
}

# Meridian Lossless Packing packetizer
vlc_modules += {
    'name' : 'packetizer_mlp',
    'sources' : files('mlp.c'),
    'shortname' : 'p_mlp',
}

# FLAC packetizer
vlc_modules += {
    'name' : 'packetizer_flac',
    'sources' : files('flac.c'),
    'shortname' : 'p_flac',
}

# HEVC packetizer
vlc_modules += {
    'name' : 'packetizer_hevc',
    'sources' : files(
        'hevc.c',
        'hevc_nal.c',
        'hxxx_sei.c',
        'hxxx_common.c'
        ),
    'shortname' : 'p_hevc',
}

# A/52 audio packetizer
vlc_modules += {
    'name' : 'packetizer_a52',
    'sources' : files('a52.c'),
    'shortname' : 'p_a52',
}

# DTS audio packetizer
vlc_modules += {
    'name' : 'packetizer_dts',
    'sources' : files('dts.c', 'dts_header.c'),
    'shortname' : 'p_dts',
}

# FFmpeg-based packetizer
vlc_modules += {
    'name' : 'packetizer_avparser',
    'sources' : files('avparser.c'),
    'dependencies' : [avutil_dep, avcodec_dep],
    'link_with' : [libavcodec_common],
    'link_args' : symbolic_linkargs,
    'enabled' : avcodec_dep.found() and not get_option('merge-ffmpeg'),
    'shortname' : 'p_avpars',
}
