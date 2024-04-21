# Do we build the main VLC binary?
build_vlc = get_option('vlc')

if build_vlc and (host_system != 'darwin' or have_osx)
    vlc_sources = []

    vlc_deps = [m_lib, dl_lib, threads_dep]

    if host_system == 'darwin'
        vlc_sources += ['darwinvlc.m']
        vlc_deps += corefoundation_dep
        if have_osx
            vlc_deps += dependency('Cocoa', required: true)
        endif
    elif host_system == 'windows'
        vlc_sources += ['winvlc.c']
    else
        vlc_sources += ['vlc.c', 'override.c']
    endif

    executable('vlc',
        vlc_sources,
        link_with: [libvlc, libvlccore],
        include_directories: [vlc_include_dirs],
        dependencies: vlc_deps,
        install: true,
        win_subsystem: 'windows'
    )

    vlc_top_builddir_def = '-DTOP_BUILDDIR="@0@"'.format(vlc_build_root)
    vlc_top_srcdir_def = '-DTOP_SRCDIR="@0@"'.format(vlc_src_root)

    executable('vlc-static',
        vlc_sources,
        link_with: [libvlc, libvlccore],
        include_directories: [vlc_include_dirs],
        dependencies: vlc_deps,
        c_args: [vlc_top_builddir_def, vlc_top_srcdir_def],
        objc_args: [vlc_top_builddir_def, vlc_top_srcdir_def],
        win_subsystem: 'windows'
    )

    vlc_cache_gen_dep = executable('vlc-cache-gen',
        ['cachegen.c'],
        link_with: [vlc_libcompat, libvlc],
        include_directories: [vlc_include_dirs],
    )

    plugins_dat  = custom_target('plugins.dat',
        # `vlc_cache_gen_dep.full_path()` doesn't work since the executable is not built yet.
        # `vlc_cache_gen_dep` creates a build order dependency.
        command: [vlc_cache_gen_dep, meson.global_build_root() / 'modules'],
        # ninja command will be `ninja bin/plugins.dat`
        output: 'plugins.dat',
        build_by_default: true,
    )

endif
