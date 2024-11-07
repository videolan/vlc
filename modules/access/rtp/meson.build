rtp_lib = static_library('rtp',
    files('rtpfmt.c', 'session.c', 'rtp.h'),
    include_directories: [vlc_include_dirs],
    install: false,
    pic: true)

if gcrypt_dep.found()
    srtp_lib = static_library('srtp',
                    files('srtp.c', 'srtp.h'),
                    dependencies: [gcrypt_dep],
                    include_directories: [vlc_include_dirs])
    srtp_dep = declare_dependency(
        link_with: [srtp_lib],
        include_directories: [include_directories('.')],
        dependencies: [gcrypt_dep],
        compile_args: ['-DHAVE_SRTP'])
else
    srtp_dep = disabler()
endif

vlc_modules += {
    'name': 'rtp',
    'sources':
        files(
            'input.c',
            'sdp.c',
            'sdp.h',
            'datagram.c',
            'vlc_dtls.h',
            'rtp.c',
            'rtp.h',
        ),
    'dependencies': [socket_libs, srtp_dep],
    'link_with': [rtp_lib],
}

vlc_modules += {
    'name': 'rtp_ac3',
    'sources': files('ac3.c')
}

vlc_modules += {
    'name': 'rtp_mpeg12',
    'sources': files('mpeg12.c')
}

vlc_modules += {
    'name': 'rtp_pcm',
    'sources': files('pcm.c')
}

vlc_modules += {
    'name': 'rtp_raw',
    'sources': files('raw.c')
}

vlc_modules += {
    'name': 'rtp_h264',
    'sources': files('h264.c', 'h26x.h'),
}

vlc_modules += {
    'name': 'rtp_h265',
    'sources': files('h265.c', 'h26x.h'),
}

vlc_modules += {
    'name': 'rtp_opus',
    'sources': files('opus.c')
}

vlc_modules += {
    'name': 'rtp_xiph',
    'sources': files('xiph.c')
}
