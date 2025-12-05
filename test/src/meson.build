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
    'name' : 'test_src_misc_viewpoint',
    'sources' : files('misc/viewpoint.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlccore],
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
    'name' : 'test_src_clock_start',
    'sources' : files(
        'clock/clock_start.c',
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

if gcrypt_dep.found() and get_option('update-check').allowed()
    vlc_tests += {
        'name' : 'test_src_crypto_update',
        'sources' : files('crypto/update.c'),
        'suite' : ['src', 'test_src'],
        'link_with' : [libvlccore],
        'dependencies' : [gcrypt_dep],
        'module_depends' : vlc_plugins_targets.keys()
    }
endif

if not(host_system == 'windows')
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
endif

vlc_tests += {
    'name' : 'test_src_input_stream_fifo',
    'sources' : files('input/stream_fifo.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
}

vlc_tests += {
    'name' : 'test_src_preparser_thumbnail',
    'sources' : files('preparser/thumbnail.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : ['demux_mock', 'rawvideo']
}

vlc_tests += {
    'name' : 'test_src_preparser_thumbnail_to_files',
    'sources' : files('preparser/thumbnail_to_files.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : ['demux_mock', 'rawvideo']
}

vlc_tests += {
    'name' : 'test_src_player_abloop',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/timers.h',
        'player/abloop.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_attachments',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/attachments.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_capabilities',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/capabilities.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_discontinuities',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/discontinuities.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_eof',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/eof.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_errors',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/errors.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_es_selection',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/es_selection.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_lifecycle',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/lifecycle.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_loudness',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/loudness.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_media',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/media.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_next_prev',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/next_prev.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_outputs',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/outputs.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_pause',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/pause.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_programs',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/programs.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_seeks',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/seeks.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_teletext',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/teletext.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_timers',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/timers.h',
        'player/timers.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_titles',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/titles.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_tracks',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/tracks.c'),
    'suite' : ['src', 'test_src'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_src_player_tracks_ids',
    'sources' : files(
        'player/common.h',
        'player/modules.c',
        'player/tracks_ids.c'),
    'suite' : ['src', 'test_src'],
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
