vlc_tests += {
    'name' : 'test_libvlc_core',
    'sources' : files('core.c'),
    'suite' : ['libvlc'],
    'link_with' : [libvlc],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_libvlc_equalizer',
    'sources' : files('equalizer.c'),
    'suite' : ['libvlc'],
    'link_with' : [libvlc]
}

vlc_tests += {
    'name' : 'test_libvlc_media',
    'sources' : files('media.c'),
    'link_with' : [libvlc, libvlccore],
    'module_depends': vlc_plugins_targets.keys()
}

test_libvlc_media_thumbnail_modules = ['demux_mock']
if avcodec_dep.found()
    test_libvlc_media_thumbnail_modules += ['avcodec']
endif
if swscale_dep.found()
    test_libvlc_media_thumbnail_modules += ['swscale']
endif
if jpeg_dep.found()
    test_libvlc_media_thumbnail_modules += ['jpeg']
endif
if png_dep.found()
    test_libvlc_media_thumbnail_modules += ['png']
endif
if vpx_dep.found()
    test_libvlc_media_thumbnail_modules += ['vpx']
endif

vlc_tests += {
    'name' : 'test_libvlc_media_thumbnail_argb',
    'sources' : files('media_thumbnail.c'),
    'link_with' : [libvlc, libvlccore],
    'suite' : ['libvlc', 'test_libvlc_media_thumbnail'],
    'c_args' : [
        '-DTEST_THUMB_TYPE=libvlc_picture_Argb',
        '-DTEST_REQUIRED_MODULES="avcodec"',
    ],
    'module_depends': test_libvlc_media_thumbnail_modules,
}

vlc_tests += {
    'name' : 'test_libvlc_media_thumbnail_jpeg',
    'sources' : files('media_thumbnail.c'),
    'link_with' : [libvlc, libvlccore],
    'suite' : ['libvlc', 'test_libvlc_media_thumbnail'],
    'c_args' : [
        '-DTEST_THUMB_TYPE=libvlc_picture_Jpg',
        '-DTEST_REQUIRED_MODULES="jpeg"',
    ],
    'module_depends': test_libvlc_media_thumbnail_modules,
}

vlc_tests += {
    'name' : 'test_libvlc_media_thumbnail_png',
    'sources' : files('media_thumbnail.c'),
    'link_with' : [libvlc, libvlccore],
    'suite' : ['libvlc', 'test_libvlc_media_thumbnail'],
    'c_args' : [
        '-DTEST_THUMB_TYPE=libvlc_picture_Png',
        '-DTEST_REQUIRED_MODULES="png"',
    ],
    'module_depends': test_libvlc_media_thumbnail_modules,
}

vlc_tests += {
    'name' : 'test_libvlc_media_thumbnail_webp',
    'sources' : files('media_thumbnail.c'),
    'link_with' : [libvlc, libvlccore],
    'suite' : ['libvlc', 'test_libvlc_media_thumbnail'],
    'c_args' : [
        '-DTEST_THUMB_TYPE=libvlc_picture_WebP',
        '-DTEST_REQUIRED_MODULES="vpx"',
    ],
    'module_depends': test_libvlc_media_thumbnail_modules,
}

# vlc_tests += {
#     'name' : 'test_libvlc_media_list_player',
#     'sources' : files('libvlc/media_list_player.c'),
#     'link_with' : [libvlc]
# }

vlc_tests += {
    'name' : 'test_libvlc_media_list',
    'sources' : files('media_list.c'),
    'suite' : ['libvlc'],
    'link_with' : [libvlc]
}

vlc_tests += {
    'name' : 'test_libvlc_media_player',
    'sources' : files('media_player.c'),
    'suite' : ['libvlc'],
    'link_with' : [libvlc, libvlccore],
    'module_depends': ['demux_mock']
}

vlc_tests += {
    'name' : 'test_libvlc_media_player_record',
    'sources' : files('media_player_record.c'),
    'suite' : ['libvlc'],
    'link_with' : [libvlc, libvlccore],
    'module_depends': ['demux_mock', 'stream_out_record']
}

vlc_tests += {
    'name' : 'test_libvlc_media_discoverer',
    'sources' : files('media_discoverer.c'),
    'suite' : ['libvlc'],
    'link_with' : [libvlc]
}

vlc_tests += {
    'name' : 'test_libvlc_renderer_discoverer',
    'sources' : files('renderer_discoverer.c'),
    'suite' : ['libvlc'],
    'link_with' : [libvlc]
}

vlc_tests += {
    'name' : 'test_libvlc_slaves',
    'sources' : files('slaves.c'),
    'suite' : ['libvlc'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_libvlc_meta',
    'sources' : files('meta.c'),
    'suite' : ['libvlc'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}
