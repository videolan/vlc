#
# VLC revision file generation
#
git_dir = vlc_src_root / '.git'
rev_target = vcs_tag(command: ['git',
    '--git-dir', git_dir,
    'describe', '--tags', '--long',
    '--match', '?.*.*', '--always'],
    input: 'revision.c.in',
    output: 'revision.c')

#
# FourCC preprocessor
#

fourcc_gen = executable('fourcc_gen',
    ['misc/fourcc_gen.c'],
    include_directories: vlc_include_dirs,
    native: true
)

fourcc = custom_target('fourcc_tables.h',
                        output:  ['fourcc_tables.h'],
                        depend_files: ['misc/fourcc_list.h', '../include/vlc_fourcc.h'],
                        capture: true,
                        command: [fourcc_gen])


#
# libvlccore dependencies
#
anl_lib = cc.find_library('anl', required: false)

libvlccore_deps = [
    m_lib, dl_lib, threads_dep, intl_dep, socket_libs, iconv_dep, anl_lib, idn_dep
]

vlccore_cargs = [ '-DMODULE_STRING="core"', '-DHAVE_DYNAMIC_PLUGINS' ]

if host_system == 'darwin'
    libvlccore_deps += foundation_dep
    libvlccore_deps += corefoundation_dep
    libvlccore_deps += dependency('CFNetwork', required: true)
elif host_system == 'windows'
min_windows_version_test = '''
#include <sdkddkver.h>
#if !defined(@0@)
# error SDK too old for @0@
#elif _WIN32_WINNT < @0@
# error building for older version than @0@
#endif
'''

    libvlccore_deps += cc.find_library('bcrypt')
    libvlccore_deps += cc.find_library('normaliz')
    if not have_win_store
        libvlccore_deps += cc.find_library('winmm')
    else
        libvlccore_deps += cc.find_library('runtimeobject')
    endif
    if cc.compiles(min_windows_version_test.format('_WIN32_WINNT_WIN8'), name: 'Compiling for Win8+')
        libvlccore_deps += cc.find_library('synchronization')
    endif
    vlccore_cargs += ['-DVLC_DLL_EXPORT']
endif

#
# libvlccore library
#

libvlccore_sources_base = files(
    'libvlc.c',
    'libvlc.h',
    'libvlc-module.c',
    'missing.c',
    'version.c',
    'config/cat.c',
    'config/configuration.h',
    'config/core.c',
    'config/chain.c',
    'config/dirs.c',
    'config/file.c',
    'config/help.c',
    'config/intf.c',
    'config/cmdline.c',
    'config/getopt.c',
    'config/vlc_getopt.h',
    'config/jaro_winkler.c',
    'extras/libc.c',
    'media_source/media_source.c',
    'media_source/media_source.h',
    'media_source/media_tree.c',
    'modules/modules.h',
    'modules/modules.c',
    'modules/bank.c',
    'modules/cache.c',
    'modules/entry.c',
    'modules/textdomain.c',
    'interface/dialog.c',
    'interface/interface.c',
    'playlist/content.c',
    'playlist/content.h',
    'playlist/control.c',
    'playlist/control.h',
    'playlist/export.c',
    'playlist/item.c',
    'playlist/item.h',
    'playlist/notify.c',
    'playlist/notify.h',
    'playlist/player.c',
    'playlist/player.h',
    'playlist/playlist.c',
    'playlist/playlist.h',
    'playlist/preparse.c',
    'playlist/preparse.h',
    'playlist/randomizer.c',
    'playlist/randomizer.h',
    'playlist/request.c',
    'playlist/shuffle.c',
    'playlist/sort.c',
    'preparser/art.c',
    'preparser/art.h',
    'preparser/fetcher.c',
    'preparser/fetcher.h',
    'preparser/preparser.c',
    'input/item.c',
    'input/access.c',
    'clock/clock_internal.c',
    'clock/input_clock.c',
    'clock/clock.c',
    'input/decoder.c',
    'input/decoder_device.c',
    'input/decoder_helpers.c',
    'input/demux.c',
    'input/demux_chained.c',
    'input/es_out.c',
    'input/es_out_source.c',
    'input/es_out_timeshift.c',
    'input/input.c',
    'input/info.h',
    'input/meta.c',
    'input/parse.c',
    'input/attachment.c',
    'input/source.c',
    'input/source.h',
    'player/player.c',
    'player/player.h',
    'player/input.c',
    'player/timer.c',
    'player/track.c',
    'player/title.c',
    'player/aout.c',
    'player/vout.c',
    'player/osd.c',
    'player/medialib.c',
    'player/metadata.c',
    'clock/input_clock.h',
    'clock/clock.h',
    'clock/clock_internal.h',
    'input/decoder.h',
    'input/demux.h',
    'input/es_out.h',
    'input/event.h',
    'input/item.h',
    'input/mrl_helpers.h',
    'input/stream.h',
    'input/input_internal.h',
    'input/input_interface.h',
    'input/vlm_internal.h',
    'input/vlm_event.h',
    'input/resource.h',
    'input/resource.c',
    'input/services_discovery.c',
    'input/stats.c',
    'input/stream.c',
    'input/stream_fifo.c',
    'input/stream_extractor.c',
    'input/stream_filter.c',
    'input/stream_memory.c',
    'input/subtitles.c',
    'input/thumbnailer.c',
    'input/var.c',
    'audio_output/aout_internal.h',
    'audio_output/common.c',
    'audio_output/dec.c',
    'audio_output/filters.c',
    'audio_output/meter.c',
    'audio_output/output.c',
    'audio_output/volume.c',
    'video_output/chrono.h',
    'video_output/control.c',
    'video_output/control.h',
    'video_output/display.c',
    'video_output/display.h',
    'video_output/inhibit.c',
    'video_output/inhibit.h',
    'video_output/interlacing.c',
    'video_output/snapshot.c',
    'video_output/snapshot.h',
    'video_output/statistic.h',
    'video_output/video_output.c',
    'video_output/video_text.c',
    'video_output/video_epg.c',
    'video_output/video_widgets.c',
    'video_output/vout_subpictures.c',
    'video_output/vout_spuregion_helper.h',
    'video_output/vout_wrapper.h',
    'video_output/video_window.c',
    'video_output/video_window.h',
    'video_output/opengl.c',
    'video_output/vout_intf.c',
    'video_output/vout_internal.h',
    'video_output/vout_private.h',
    'video_output/vout_wrapper.c',
    'video_output/window.c',
    'network/getaddrinfo.c',
    'network/http_auth.c',
    'network/httpd.c',
    'network/io.c',
    'network/tcp.c',
    'network/udp.c',
    'network/rootbind.c',
    'network/stream.c',
    'network/tls.c',
    'text/charset.c',
    'text/memstream.c',
    'text/strings.c',
    'text/unicode.c',
    'text/url.c',
    'text/filesystem.c',
    'text/iso_lang.c',
    'text/iso-639_def.h',
    'misc/actions.c',
    'misc/ancillary.c',
    'misc/executor.c',
    'misc/md5.c',
    'misc/probe.c',
    'misc/rand.c',
    'misc/mtime.c',
    'misc/frame.c',
    'misc/fifo.c',
    'misc/filesystem.c',
    'misc/fourcc.c',
    'misc/fourcc_list.h',
    'misc/es_format.c',
    'misc/picture.c',
    'misc/picture.h',
    'misc/picture_fifo.c',
    'misc/picture_pool.c',
    'misc/interrupt.h',
    'misc/interrupt.c',
    'misc/keystore.c',
    'misc/renderer_discovery.c',
    'misc/threads.c',
    'misc/cpu.c',
    'misc/diffutil.c',
    'misc/epg.c',
    'misc/exit.c',
    'misc/image.c',
    'misc/messages.c',
    'misc/mime.c',
    'misc/objects.c',
    'misc/objres.c',
    'misc/queue.c',
    'misc/variables.h',
    'misc/variables.c',
    'misc/xml.c',
    'misc/addons.c',
    'misc/filter.c',
    'misc/filter_chain.c',
    'misc/httpcookies.c',
    'misc/fingerprinter.c',
    'misc/text_style.c',
    'misc/sort.c',
    'misc/subpicture.c',
    'misc/subpicture.h',
    'misc/medialibrary.c',
    'misc/viewpoint.c',
    'misc/rcu.c',
    'misc/tracer.c',
)

libvlccore_sout_sources = [
    'stream_output/sap.c',
    'stream_output/stream_output.c',
    'stream_output/stream_output.h'
]

libvlccore_vlm_sources = [
    'input/vlm.c',
    'input/vlm_event.c',
    'input/vlmshell.c'
]

libvlccore_sources = [
    libvlccore_sources_base,
    libvlccore_sout_sources,
]

if get_option('videolan_manager')
    libvlccore_sources += libvlccore_vlm_sources
endif

libvlccore_link_args = []

if host_system == 'darwin'
    libvlccore_sources += [
        'darwin/dirs.m',
        'darwin/error.c',
        'darwin/netconf.m',
        'darwin/specific.c',

        'posix/filesystem.c',
        'posix/plugin.c',
        'posix/rand.c',
        'posix/timer.c',
        'posix/sort.c',
        'posix/thread.c',
        'posix/wait.c',
    ]
    libvlccore_link_args += '-Wl,-U,_vlc_static_modules'
elif host_system == 'windows'
    libvlccore_sources += [
        'win32/error.c',
        'win32/filesystem.c',
        'win32/netconf.c',
        'win32/plugin.c',
        'win32/rand.c',
        'win32/specific.c',
        'win32/thread.c',
    ]

    if have_win_store
        libvlccore_sources += [
            'posix/timer.c',
            'win32/dirs-uap.c',
        ]
    else
        libvlccore_sources += [
            'win32/timer.c',
            'win32/dirs.c',
            'win32/spawn.c',
        ]
    endif
elif host_system == 'linux'
    libvlccore_sources += [
        'posix/dirs.c',
        'posix/error.c',
        'posix/netconf.c',
        'posix/spawn.c',
        'posix/picture.c',
        'posix/specific.c',
        'posix/thread.c',
        'posix/filesystem.c',
        'posix/plugin.c',
        'posix/rand.c',
        'posix/timer.c',
        'posix/sort.c',
        'linux/cpu.c',
        'linux/dirs.c',
        'linux/filesystem.c',
        'linux/thread.c',
    ]

    if anl_lib.found()
        libvlccore_sources += 'linux/getaddrinfo.c'
    else
        libvlccore_sources += 'posix/getaddrinfo.c'
    endif
endif

libvlccore = library(
    'vlccore',
    libvlccore_sources, vlc_about, fourcc, rev_target,
    include_directories: vlc_include_dirs,
    version: '9.0.0',
    c_args: vlccore_cargs,
    link_args: libvlccore_link_args,
    link_with: [vlc_libcompat],
    dependencies: libvlccore_deps,
    install: true
)

libvlccore_dep = declare_dependency(link_with: libvlccore, sources: vlc_about)
