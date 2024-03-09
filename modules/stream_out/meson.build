# Stream output modules

# dummy
vlc_modules += {
    'name' : 'stream_out_dummy',
    'sources' : files('dummy.c')
}

# cycle
vlc_modules += {
    'name' : 'stream_out_cycle',
    'sources' : files('cycle.c')
}

# delay
vlc_modules += {
    'name' : 'stream_out_delay',
    'sources' : files('delay.c')
}

# stats
vlc_modules += {
    'name' : 'stream_out_stats',
    'sources' : files('stats.c')
}

# standard
vlc_modules += {
    'name' : 'stream_out_standard',
    'sources' : files(
      'standard.c',
      'sdp_helper.c'
    ),
    'dependencies' : [socket_libs]
}

# duplicate
vlc_modules += {
    'name' : 'stream_out_duplicate',
    'sources' : files('duplicate.c')
}

# es
vlc_modules += {
    'name' : 'stream_out_es',
    'sources' : files('es.c')
}

# display
vlc_modules += {
    'name' : 'stream_out_display',
    'sources' : files('display.c')
}

# gather
vlc_modules += {
    'name' : 'stream_out_gather',
    'sources' : files('gather.c')
}

# bridge
vlc_modules += {
    'name' : 'stream_out_bridge',
    'sources' : files('bridge.c')
}

# mosaic_bridge
vlc_modules += {
    'name' : 'stream_out_mosaic_bridge',
    'sources' : files('mosaic_bridge.c')
}

# autodel
vlc_modules += {
    'name' : 'stream_out_autodel',
    'sources' : files('autodel.c')
}

# record
vlc_modules += {
    'name' : 'stream_out_record',
    'sources' : files('record.c')
}

# smem
vlc_modules += {
    'name' : 'stream_out_smem',
    'sources' : files('smem.c')
}

# setid
vlc_modules += {
    'name' : 'stream_out_setid',
    'sources' : files('setid.c')
}

# transcode
vlc_modules += {
    'name' : 'stream_out_transcode',
    'sources' : files(
        'transcode/transcode.c',
        'transcode/encoder/encoder.c',
        'transcode/encoder/audio.c',
        'transcode/encoder/spu.c',
        'transcode/encoder/video.c',
        'transcode/pcr_sync.c',
        'transcode/pcr_helper.c',
        'transcode/spu.c',
        'transcode/audio.c',
        'transcode/video.c'
    ),
    'dependencies' : [m_lib]
}

# trace
vlc_modules += {
    'name' : 'stream_out_trace',
    'sources' : files('trace.c')
}

# UDP
vlc_modules += {
    'name' : 'stream_out_udp',
    'sources' : files('sdp_helper.c', 'udp.c'),
    'dependencies' : [socket_libs]
}

# RTP
vlc_modules += {
    'name' : 'stream_out_rtp',
    'sources' : files(
        'sdp_helper.c',
        'rtp.c',
        'rtpfmt.c',
        'rtcp.c',
        'rtsp.c',
    ),
    'dependencies' : [socket_libs]
    # TODO: Add optional GCrypt dependency
}

# Chromaprint module
libchromaprint_dep = dependency('libchromaprint', version: '>= 0.6.0', required: get_option('libchromaprint'))
vlc_modules += {
    'name' : 'stream_out_chromaprint',
    'sources' : files('chromaprint.c'),
    'dependencies' : [libchromaprint_dep],
    'enabled' : libchromaprint_dep.found(),
}

# Chromecast module
subdir('chromecast')
