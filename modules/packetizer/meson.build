# AV1 packetizer
vlc_modules += {
    'name' : 'packetizer_av1',
    'sources' : files('av1.c', 'av1_obu.c'),
    'shortname' : 'p_av1',
    'plugin_dir' : 'packetizer',
}

# Copy packetizer
vlc_modules += {
    'name' : 'packetizer_copy',
    'sources' : files('copy.c'),
    'shortname' : 'p_copy',
    'plugin_dir' : 'packetizer',
}

# MPEG-I/II video packetizer
vlc_modules += {
    'name' : 'packetizer_mpegvideo',
    'sources' : files('mpegvideo.c'),
    'shortname' : 'p_mpegvi',
    'plugin_dir' : 'packetizer',
}

# MPEG audio layer I/II/III packetizer
vlc_modules += {
    'name' : 'packetizer_mpegaudio',
    'sources' : files('mpegaudio.c'),
    'shortname' : 'p_mpegau',
    'plugin_dir' : 'packetizer',
}

# MPEG4 video packetizer
vlc_modules += {
    'name' : 'packetizer_mpeg4video',
    'sources' : files('mpeg4video.c'),
    'shortname' : 'p_mpeg4v',
    'plugin_dir' : 'packetizer',
}

# MPEG4 audio packetizer
vlc_modules += {
    'name' : 'packetizer_mpeg4audio',
    'sources' : files('mpeg4audio.c'),
    'shortname' : 'p_mpeg4a',
    'plugin_dir' : 'packetizer',
}

# MJPEG packetizer
vlc_modules += {
    'name' : 'packetizer_mjpeg',
    'sources' : files('mjpeg.c'),
    'shortname' : 'p_mjpeg',
    'plugin_dir' : 'packetizer',
}

# H.264 packetizer
packetizer_h264_c_args = []
if host_system == 'os/2'
    packetizer_h264_c_args += '-mstackrealign'
endif

vlc_modules += {
    'name' : 'packetizer_h264',
    'sources' : files(
        'h264.c',
        'h264_nal.c',
        'h264_slice.c',
        'hxxx_sei.c',
        'hxxx_common.c'
    ),
    'c_args' : packetizer_h264_c_args,
    'shortname' : 'p_h264',
    'plugin_dir' : 'packetizer',
}

# VC-1 packetizer
vlc_modules += {
    'name' : 'packetizer_vc1',
    'sources' : files('vc1.c'),
    'shortname' : 'p_vc1',
    'plugin_dir' : 'packetizer',
}

# Meridian Lossless Packing packetizer
vlc_modules += {
    'name' : 'packetizer_mlp',
    'sources' : files('mlp.c'),
    'shortname' : 'p_mlp',
    'plugin_dir' : 'packetizer',
}

# FLAC packetizer
vlc_modules += {
    'name' : 'packetizer_flac',
    'sources' : files('flac.c'),
    'shortname' : 'p_flac',
    'plugin_dir' : 'packetizer',
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
    'plugin_dir' : 'packetizer',
}

# VVC packetizer
vlc_modules += {
    'name' : 'packetizer_h266',
    'sources' : files(
        'h266.c',
        'h266_nal.c',
        'hxxx_sei.c',
        'hxxx_common.c'
    ),
    'plugin_dir' : 'packetizer',
}

# A/52 audio packetizer
vlc_modules += {
    'name' : 'packetizer_a52',
    'sources' : files('a52.c'),
    'shortname' : 'p_a52',
    'plugin_dir' : 'packetizer',
}

# DTS audio packetizer
vlc_modules += {
    'name' : 'packetizer_dts',
    'sources' : files('dts.c', 'dts_header.c'),
    'shortname' : 'p_dts',
    'plugin_dir' : 'packetizer',
}

# FFmpeg-based packetizer
vlc_modules += {
    'name' : 'packetizer_avparser',
    'sources' : files('avparser.c'),
    'dependencies' : [avutil_dep, avcodec_dep],
    'link_with' : [libavcodec_common],
    'link_args' : symbolic_linkargs,
    'plugin_dir' : 'packetizer',
    'enabled' : avcodec_dep.found() and not get_option('merge-ffmpeg'),
    'shortname' : 'p_avpars',
}
