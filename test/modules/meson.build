vlc_tests += {
    'name' : 'test_modules_lua_extension',
    'sources' : files('lua/extension.c'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

if not (host_system == 'windows') # missing mkdtemp()
vlc_tests += {
    'name' : 'test_modules_misc_medialibrary',
    'sources' : files('misc/medialibrary.c'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys(),
    #'module_depends' : ['medialibrary', 'demux_mock', 'jpeg', 'png', 'rawvid'],
}
endif

vlc_tests += {
    'name' : 'test_modules_packetizer_helpers',
    'sources' : files('packetizer/helpers.c'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_modules_packetizer_hxxx',
    'sources' : files('packetizer/hxxx.c'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_modules_packetizer_h264',
    'sources' : files('packetizer/h264.c'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_modules_packetizer_hevc',
    'sources' : files('packetizer/hevc.c'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_modules_packetizer_mpegvideo',
    'sources' : files('packetizer/mpegvideo.c'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_modules_keystore',
    'sources' : files('keystore/test.c'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

if not(host_system == 'windows')
vlc_tests += {
    'name' : 'test_modules_tls',
    'sources' : files('misc/tls.c'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}
endif

vlc_tests += {
    'name' : 'test_modules_demux_timestamps',
    'sources' : files('demux/timestamps.c'),
    'suite' : ['modules', 'test_modules']
}

vlc_tests += {
    'name' : 'test_modules_demux_timestamps_filter',
    'sources' : files('demux/timestamps_filter.c'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_modules_ts_pes',
    'sources' : files(
        'demux/ts_pes.c',
        '../../modules/demux/mpeg/ts_pes.c',
        '../../modules/demux/mpeg/ts_pes.h'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_modules_codec_hxxx_helper',
    'sources' : files('codec/hxxx_helper.c'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore, hxxxhelper_lib],
    'module_depends' : vlc_plugins_targets.keys()
}

if opengl_dep.found()
vlc_tests += {
    'name' : 'test_modules_video_output_opengl_filters',
    'sources' : files(
        'video_output/opengl/filters.c',
        '../../modules/video_output/opengl/filters.c',
        '../../modules/video_output/opengl/filters.h',
        '../../modules/video_output/opengl/filter.c',
        '../../modules/video_output/opengl/gl_api.c',
        '../../modules/video_output/opengl/gl_api.h',
        '../../modules/video_output/opengl/interop.c',
        '../../modules/video_output/opengl/interop.h',
        '../../modules/video_output/opengl/importer.c',
        '../../modules/video_output/opengl/importer.h'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'c_args' : ['-DVLC_TEST_OPENGL_API=VLC_OPENGL'],
    'module_depends' : vlc_plugins_targets.keys()
}
endif

if opengles2_dep.found()
vlc_tests += {
    'name' : 'test_modules_video_output_opengl_es2_filters',
    'sources' : files(
        'video_output/opengl/filters.c',
        '../../modules/video_output/opengl/filters.c',
        '../../modules/video_output/opengl/filters.h',
        '../../modules/video_output/opengl/filter.c',
        '../../modules/video_output/opengl/gl_api.c',
        '../../modules/video_output/opengl/gl_api.h',
        '../../modules/video_output/opengl/interop.c',
        '../../modules/video_output/opengl/interop.h',
        '../../modules/video_output/opengl/importer.c',
        '../../modules/video_output/opengl/importer.h'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'c_args' : ['-DVLC_TEST_OPENGL_API=VLC_OPENGL_ES2'],
    'module_depends' : vlc_plugins_targets.keys()
}
endif

vlc_tests += {
    'name' : 'test_modules_stream_out_transcode',
    'sources' : files(
        'stream_out/transcode.c',
        'stream_out/transcode.h',
        'stream_out/transcode_scenarios.c'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_modules_stream_out_pcr_sync',
    'sources' : files(
        'stream_out/pcr_sync.c',
        '../../modules/stream_out/transcode/pcr_sync.c',
        '../../modules/stream_out/transcode/pcr_sync.h',
        '../../modules/stream_out/transcode/pcr_helper.c',
        '../../modules/stream_out/transcode/pcr_helper.h'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}

vlc_tests += {
    'name' : 'test_modules_mux_webvtt',
    'sources' : files('mux/webvtt.c'),
    'suite' : ['modules', 'test_modules'],
    'link_with' : [libvlc, libvlccore],
    'module_depends' : vlc_plugins_targets.keys()
}
