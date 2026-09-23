libvlc_sources = [
    'core.c',
    'dialog.c',
    'downloader.c',
    'renderer_discoverer.c',
    'error.c',
    'log.c',
    'video.c',
    'audio.c',
    'media.c',
    'media_track.c',
    'media_player.c',
    'media_list.c',
    'media_list_path.h',
    'media_list_player.c',
    'media_discoverer.c',
    'parser.c',
    'picture.c'
]

libvlc_cargs = []
if host_system in ['windows', 'os/2']
    libvlc_cargs += ['-DLIBVLC_DLL_EXPORT']
endif


if host_system == 'windows'
    libvlc_rc = configure_file(
        input: 'libvlc_win32_rc.rc.in',
        output: 'libvlc_win32_rc.rc',
        configuration: rc_conf,
        install: false,
    )
    libvlc_sources += windows_rc.compile_resources(
        libvlc_rc,
        include_directories: [
            vlc_src_root / 'share',
            vlc_src_root / 'extras/package/win32',
        ],
        depend_files: files('../extras/package/win32/libvlc.dll.manifest'),
    )
endif

libvlc_kwargs = {}
if vlc_lib_versioning
    libvlc_kwargs += {'version': libvlc_abi_version}
endif

libvlc = library('vlc',
    libvlc_sources, rev_target,
    include_directories: [vlc_include_dirs],
    c_args: libvlc_cargs,
    link_with: [vlc_libcompat],
    dependencies: [libvlccore_dep, m_lib, threads_dep],
    install: true,
    install_dir: libdir_path,
    kwargs: libvlc_kwargs,
)

# Publicly exported dependency object. Used when libvlc is wrapped in a meson
# subproject.
libvlc_dep = declare_dependency(
    link_with: libvlc,
    include_directories: include_directories('../include'),
)

pkg = import('pkgconfig')
pkg.generate(libvlc,
    name: 'libvlc', # name of the .pc file in addition to Name: variable / 'LibVLC control API',
    description: 'VLC media player external control library',
    version: meson.project_version(),
    install_dir: vlccore_lib_path / 'pkgconfig')

meson.override_dependency('libvlc', libvlc_dep)
