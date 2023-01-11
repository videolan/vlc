#
# VLC plugins
# This file contains the top-level build definition for VLC
# plugins, in the subdirectories are the actual build definitions
# for the individual plugins.
#

# Module dependencies
# The following section contains dependencies that are needed
# by multiple module types. (There is no need to put dependencies
# here that are used by multiple modules in the same subdirectory)

# Check for X C bindings (XCB)
if (host_system != 'darwin' and host_system != 'windows') or get_option('xcb').enabled()
    xcb_dep = dependency('xcb', version: '>= 1.6', required: get_option('xcb'))
    xcb_composite_dep = dependency('xcb-composite', required: get_option('xcb'))
    xcb_randr_dep = dependency('xcb-randr', version: '>= 1.3', required: get_option('xcb'))
    xcb_render_dep = dependency('xcb-render', required: get_option('xcb'))
    xcb_shm_dep = dependency('xcb-shm', version: '>= 1.9.2', required: get_option('xcb'))
    xcb_xkb_dep = dependency('xcb-xkb', required: get_option('xcb'))
    xcb_keysyms_dep = dependency('xcb-keysyms', version: '>= 0.3.4', required: get_option('xcb'))
    xproto_dep = dependency('xproto', required: get_option('xcb'))
    xcb_damage_dep = dependency('xcb-damage', required: get_option('xcb'))
    xcb_xfixes_dep = dependency('xcb-xfixes', required: get_option('xcb'))
else
    xcb_dep = dependency('', required: false)
endif

# Check for Wayland
if (host_system != 'darwin' and host_system != 'windows') or get_option('xcb').enabled()
    wayland_scanner_dep = dependency('wayland-scanner', version: '>= 1.15',
        required: get_option('wayland'), native: true)

    wayland_protocols_dep = dependency('wayland-protocols', version: '>= 1.15',
        required: get_option('wayland'))

    wayland_deps = [
        dependency('wayland-client', version: '>= 1.5.91', required: get_option('wayland')),
        dependency('wayland-cursor', required: get_option('wayland')),
        dependency('wayland-egl', required: get_option('wayland')),
        wayland_scanner_dep,
        wayland_protocols_dep
    ]

    have_wayland = true
    foreach iter_wayland_dep : wayland_deps
        if (iter_wayland_dep.found() == false)
            message('Wayland disabled. (not all needed dependencies found)')
            have_wayland = false
            break
        endif
    endforeach

    if have_wayland
        wayland_scanner = find_program(
            wayland_scanner_dep.get_pkgconfig_variable('wayland_scanner'),
            native: true
        )
        wayland_protocols_dir = wayland_protocols_dep.get_pkgconfig_variable('pkgdatadir')
    endif
else
    have_wayland = false
endif

# Pulse audio
pulse_dep = dependency('libpulse', version: '>= 1.0', required: get_option('pulse'))

# ALSA
alsa_dep = dependency('alsa', version: '>= 1.0.24', required: get_option('alsa'))

# JACK (TODO: set required based on a get_option())
jack_dep = dependency('jack', version: '>= 1.9.7', required: false)
if not jack_dep.found()
    # Try jack1 instead
    jack_dep = dependency('jack', version: ['>= 0.120.1', '< 1.0'],
        required: false)
endif

# Darwin-specific dependencies
if host_system == 'darwin'
    security_dep = dependency('Security', required: true)
    coremedia_dep = dependency('CoreMedia', required: true)
    avfoundation_dep = dependency('AVFoundation', required: true)
    corevideo_dep = dependency('CoreVideo', required: true)
    videotoolbox_dep = dependency('VideoToolbox', required: true)
    audiounit_dep = dependency('AudioUnit', required: true)
    audiotoolbox_dep = dependency('AudioToolbox', required: true)
    iokit_dep = dependency('IOKit', required: true)
    quartz_dep = dependency('QuartzCore', required: true)
    coretext_dep = dependency('CoreText', required: true)
    coreaudio_dep = dependency('CoreAudio', required: true)
    coreimage_dep = dependency('CoreImage', required: true)
    coregraphics_dep = dependency('CoreGraphics', required: true)
endif

# macOS specific dependencies
if have_osx
    cocoa_dep = dependency('Cocoa', required: true)
endif

# iOS/tvOS specific dependencies
if have_ios or have_tvos
    uikit_dep = dependency('UIKit', required: true)
endif

# Windows-specific dependencies
if host_system == 'windows'

    # WASAPI-related dependency
    ksuser_lib = cc.find_library('ksuser',
                                 has_headers: ['audioclient.h'],
                                 required: get_option('wasapi'))
endif

# Helper libraries for modules
# These are helper libraries used by some modules
if pulse_dep.found()
    libvlc_pulse = library('vlc_pulse',
        files('audio_output/vlcpulse.c'),
        include_directories: [include_directories('audio_output'), vlc_include_dirs],
        dependencies: [libvlccore_dep, pulse_dep],
        link_with: [vlc_libcompat],
    )
endif

# SRT
srt_dep = dependency('srt', version: '>=1.3.0', required: get_option('srt'))

# libRist
librist_dep = dependency('librist', required: get_option('rist'))

# MTP
mtp_dep = dependency('libmtp', version: '>=1.0.0', required: get_option('mtp'))

# FFmpeg
avformat_dep = dependency('libavformat', version: '>= 53.21.0', required: get_option('avformat'))
avcodec_dep  = dependency('libavcodec', version: '>= 57.37.100', required: get_option('avcodec'))
avutil_dep   = dependency('libavutil', version: '>= 55.22.101', required: false)

# Freetype, used by text_renderer and ASS access
freetype_dep = dependency('freetype2', required: get_option('freetype'))

# ZVBI, used by Linsys SDI access and VBI/Teletext decoders
zvbi_dep = dependency('zvbi-0.2', version: '>= 0.2.28', required: false)

# rsvg, used by SVG codec and text renderer
rsvg_dep = dependency('librsvg-2.0', version: '>= 2.9.0', required: get_option('rsvg'))

# Array that holds all enabled VLC module dicts
vlc_modules = []

# codec modules
subdir('codec')

# access modules
subdir('access')

# demux modules
subdir('demux')

# access_output modules
subdir('access_output')

# audio filter modules
subdir('audio_filter')

# audio mixer
subdir('audio_mixer')

# audio output modules
subdir('audio_output')

# control modules
subdir('control')

# gui modules
subdir('gui')

# keystore modules
subdir('keystore')

# logger modules
subdir('logger')

# misc modules
subdir('misc')

# meta engine modules
subdir('meta_engine')

# packetizer modules
subdir('packetizer')

# service discovery modules
subdir('services_discovery')

# stream output modules
subdir('stream_out')

# stream filter modules
subdir('stream_filter')

# spu modules
subdir('spu')

# text renderer modules
subdir('text_renderer')

# video chroma modules
subdir('video_chroma')

# video filter modules
subdir('video_filter')

# video output modules
subdir('video_output')

# video splitter modules
subdir('video_splitter')

# visualization modules
subdir('visualization')

# lua module
subdir('lua')

# Qt check executable
# This has to be declared here as it needs to end up
# in the modules folder, not in gui/qt/ subfolder as
# vlc-static would not find it there.
if qt5_dep.found()
    executable('vlc-qt-check',
        files('gui/qt/vlc-qt-check.cpp'),
        dependencies: [qt5_dep, qt_extra_deps],
        include_directories: [vlc_include_dirs],
        build_by_default: true,
        install: true,
        install_dir: get_option('libexecdir'),
        c_args : qt_extra_flags,
        cpp_args : qt_extra_flags,
    )
endif

#
# Common module handling code
#
# In order to reduce boilerplate code, plugins are defined by a
# dictionary which is added to the `vlc_modules` array. This
# array is then iterated over and a library is defined for each
# entry with the needed build definition for a VLC plugin.
#
foreach module : vlc_modules
    if not module.has_key('name')
        error('Got invalid vlc_modules entry without \'name\' key')
    endif
    if not module.has_key('sources')
        error('The vlc_modules entry for ' + module['name'] + ' has no sources')
    endif

    # This list MUST be kept in sync with the keys used below!
    valid_dict_keys = [
        'name',
        'sources',
        'link_with',
        'link_args',
        'link_depends',
        'link_language',
        'include_directories',
        'dependencies',
        'c_args',
        'cpp_args',
        'objc_args',
    ]
    foreach key : module.keys()
        if key not in valid_dict_keys
            error('Invalid key \'@0@\' found in vlc_modules entry for \'@1@\''
                .format(key, module['name']))
        endif
    endforeach

    common_args = [
        '-DMODULE_STRING="@0@"'.format(module['name']),
        '-D__PLUGIN__'
    ]

    library(module['name'] + '_plugin',
        module['sources'],
        link_with: [module.get('link_with', []), vlc_libcompat],
        link_args: module.get('link_args', []),
        link_depends: module.get('link_depends', []),
        link_language: module.get('link_language', []),
        include_directories: [module.get('include_directories', []), vlc_include_dirs],
        dependencies: [module.get('dependencies', []), libvlccore_dep],
        c_args: [module.get('c_args', []), common_args],
        cpp_args: [module.get('cpp_args', []), common_args],
        objc_args: [module.get('objc_args', []), common_args],
        build_by_default: true,
        install: true,
        install_dir: get_option('libdir') / 'vlc/plugins'
    )
endforeach
