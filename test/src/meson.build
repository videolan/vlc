vlc_tests += {
    'name' : 'test_src_config_chain',
    'sources' : files('config/chain.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlccore],
}

vlc_tests += {
    'name' : 'test_src_misc_ancillary',
    'sources' : files('misc/ancillary.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
}

vlc_tests += {
    'name' : 'test_src_misc_bits',
    'sources' : files('misc/bits.c'),
    'suite' : ['src', 'test_src'],
}

vlc_tests += {
    'name' : 'test_src_clock_clock',
    'sources' : files(
        'clock/clock.c',
        '../../src/clock/clock.c',
        '../../src/clock/clock_internal.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
}

vlc_tests += {
    'name' : 'test_src_misc_variables',
    'sources' : files('misc/variables.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
}

if gcrypt_dep.found()
    vlc_tests += {
        'name' : 'test_src_crypto_update',
        'sources' : files('crypto/update.c'),
        'suite' : ['src', 'test_src'],
        'link_with' : [libvlccore],
        'dependencies' : [gcrypt_dep],
        'module_depends' : vlc_plugins_targets.keys()
    }
endif

vlc_tests += {
    'name' : 'test_src_input_stream',
    'sources' : files('input/stream.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_input_stream_net',
    'sources' : files('input/stream.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'c_args' : ['-DTEST_NET'],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_input_stream_fifo',
    'sources' : files('input/stream_fifo.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
}

vlc_tests += {
    'name' : 'test_src_input_thumbnail',
    'sources' : files('input/thumbnail.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : ['demux_mock', 'rawvideo']
}

vlc_tests += {
    'name' : 'test_src_player',
    'sources' : files('player/player.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
    #['demux_mock', 'araw', 'rawvideo', 'tdummy', 'dummy',
    # 'wdummy', 'vdummy', 'adummy', 'ebur128', 'subsdec',
    # 'zvbi', ...],
}

vlc_tests += {
    'name' : 'test_src_player_monotonic_clock',
    'sources' : files('player/player.c'),
    'suite' : ['src', 'test_src'],
    'c_args' : ['-DTEST_CLOCK_MONOTONIC'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_video_output',
    'sources' : files(
        'video_output/video_output.c',
        'video_output/video_output.h',
        'video_output/video_output_scenarios.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_video_output_opengl',
    'sources' : files('video_output/opengl.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_input_decoder',
    'sources' : files(
        'input/decoder/input_decoder.c',
        'input/decoder/input_decoder.h',
        'input/decoder/input_decoder_scenarios.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_misc_image',
    'sources' : files('misc/image.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
}

vlc_tests += {
    'name' : 'test_src_misc_epg',
    'sources' : files('misc/epg.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
}

vlc_tests += {
    'name' : 'test_src_misc_keystore',
    'sources' : files('misc/keystore.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : ['memory_keystore', 'file_keystore'],
}

if host_system == 'darwin'
    vlc_tests += {
        'name' : 'test_src_misc_image_cvpx',
        'sources' : files('misc/image_cvpx.c'),
        'suite' : ['src', 'test_src'],
        'link_with' : [libvlc, libvlccore, libvlc_vtutils],
    }
endif

vlc_tests += {
    'name' : 'test_src_interface_dialog',
    'sources' : files('interface/dialog.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
}

vlc_tests += {
    'name' : 'test_src_media_source',
    'sources' : files('media_source/media_source.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
}
