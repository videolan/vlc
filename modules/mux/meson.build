# muxer modules

vlc_modules += {
    'name': 'mux_dummy',
    'sources': files('dummy.c'),
}

vlc_modules += {
    'name': 'mux_asf',
    'sources': files('asf.c'),
}

vlc_modules += {
    'name': 'mux_avi',
    'sources': files('avi.c'),
}

vlc_modules += {
    'name': 'mux_mp4',
    'sources': files(
        'mp4/mp4.c',
        'mp4/libmp4mux.c',
        'extradata.c',
        '../packetizer/av1_obu.c'),
    'link_with': [hxxxhelper_lib],
}

vlc_modules += {
    'name': 'mux_mpjpeg',
    'sources': files('mpjpeg.c'),
}

vlc_modules += {
    'name': 'mux_ogg',
    'sources': files('ogg.c'),
    'dependencies': [ ogg_dep ],
    'enabled': ogg_dep.found(),
}

vlc_modules += {
    'name': 'mux_ps',
    'sources': files(
        'mpeg/pes.c',
        'mpeg/repack.c',
        'mpeg/ps.c'),
}

vlc_modules += {
    'name': 'mux_ts',
    'sources': files(
        'mpeg/pes.c',
        'mpeg/repack.c',
        'mpeg/csa.c',
        'mpeg/tables.c',
        'mpeg/tsutil.c',
        'mpeg/ts.c',
    ),
    'dependencies': [ libdvbpsi_dep, libdvbcsa_dep ],
    'enabled': libdvbpsi_dep.found(),
}

vlc_modules += {
    'name': 'mux_wav',
    'sources': files('wav.c'),
}

