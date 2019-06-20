vlc_libcompat = []

if libcompat_sources.length() > 0
    vlc_libcompat = static_library(
        'compat',
        libcompat_sources,
        include_directories: [vlc_include_dirs],
        dependencies: [m_lib],
        pic: true,
        install: true,
        install_dir: get_option('libdir') / 'vlc'
    )
endif
