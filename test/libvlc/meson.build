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
