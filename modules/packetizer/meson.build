# AV1 packetizer
vlc_modules += {
    'name' : 'packetizer_av1',
    'sources' : files('av1.c', 'av1_obu.c')
}

# Copy packetizer
vlc_modules += {
    'name' : 'packetizer_copy',
    'sources' : files('copy.c')
}

# MPEG-I/II video packetizer
vlc_modules += {
    'name' : 'packetizer_mpegvideo',
    'sources' : files('mpegvideo.c')
}

# MPEG audio layer I/II/III packetizer
vlc_modules += {
    'name' : 'packetizer_mpegaudio',
    'sources' : files('mpegaudio.c')
}

# MPEG4 video packetizer
vlc_modules += {
    'name' : 'packetizer_mpeg4video',
    'sources' : files('mpeg4video.c')
}

# MPEG4 audio packetizer
vlc_modules += {
    'name' : 'packetizer_mpeg4audio',
    'sources' : files('mpeg4audio.c')
}

# MJPEG packetizer
vlc_modules += {
    'name' : 'packetizer_mjpeg',
    'sources' : files('mjpeg.c')
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
    )
}

# VC-1 packetizer
vlc_modules += {
    'name' : 'packetizer_vc1',
    'sources' : files('vc1.c')
}

# Meridian Lossless Packing packetizer
vlc_modules += {
    'name' : 'packetizer_mlp',
    'sources' : files('mlp.c')
}

# FLAC packetizer
vlc_modules += {
    'name' : 'packetizer_flac',
    'sources' : files('flac.c')
}

# HEVC packetizer
vlc_modules += {
    'name' : 'packetizer_hevc',
    'sources' : files(
        'hevc.c',
        'hevc_nal.c',
        'hxxx_sei.c',
        'hxxx_common.c'
        )
}

# A/52 audio packetizer
vlc_modules += {
    'name' : 'packetizer_a52',
    'sources' : files('a52.c')
}

# DTS audio packetizer
vlc_modules += {
    'name' : 'packetizer_dts',
    'sources' : files('dts.c', 'dts_header.c')
}

# FFmpeg-based packetizer
if avcodec_dep.found()
    vlc_modules += {
        'name' : 'packetizer_avparser',
        'sources' : files('avparser.c'),
        'dependencies' : [avutil_dep, avcodec_dep],
        'link_with' : [libavcodec_common]
    }
endif
