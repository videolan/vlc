# Do we build the main VLC binary?
build_vlc = get_option('vlc')

if build_vlc and (host_system != 'darwin' or have_osx) and not have_win_store and host_system != 'android'
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
        vlc_deps += cc.find_library('ole32', required: true)
    else
        vlc_sources += ['vlc.c', 'override.c']
        if x11_dep.found()
            vlc_deps += [x11_dep]
        endif
    endif

    vlc_win_extra = []
    if host_system == 'windows'
        vlc_rc = configure_file(
            input: 'vlc_win32_rc.rc.in',
            output: 'vlc_win32_rc.rc',
            configuration: rc_conf,
            install: false,
        )
        vlc_win_extra = windows_rc.compile_resources(
            vlc_rc,
            include_directories: [
                vlc_src_root / 'share/icons',
                vlc_src_root / 'extras/package/win32',
            ],
            depend_files: files('../extras/package/win32/vlc.exe.manifest'),
        )
    endif

    executable('vlc',
        vlc_sources + vlc_win_extra,
        link_with: [libvlc, libvlccore],
        include_directories: [vlc_include_dirs],
        dependencies: vlc_deps,
        install: true,
        install_dir: bindir_path,
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
        ['cachegen.c'] + vlc_win_extra,
        link_with: [vlc_libcompat, libvlc],
        include_directories: [vlc_include_dirs],
        install: true,
        install_dir: pkglibexecdir_path,
    )

    plugins_dat  = custom_target('plugins.dat',
        # `vlc_cache_gen_dep.full_path()` doesn't work since the executable is not built yet.
        # `vlc_cache_gen_dep` creates a build order dependency.
        command: [vlc_cache_gen_dep, meson.global_build_root() / 'modules'],
        # ninja command will be `ninja bin/plugins.dat`
        output: 'plugins.dat',
        depends: vlc_plugins_targets_array,
        build_by_default: not meson.is_cross_build(),
    )

    vlc_preparser_sources = [
      'preparser/main.c',
      'preparser/cmdline.c',
      'preparser/cmdline.h',
      '../src/config/getopt.c',
      '../src/config/jaro_winkler.c',
    ]

    executable('vlc-preparser',
        vlc_preparser_sources + vlc_win_extra,
        link_with: [libvlc, libvlccore, vlc_libcompat],
        include_directories: [vlc_include_dirs],
        dependencies: vlc_deps,
        install: true,
        install_dir: pkglibexecdir_path,
        win_subsystem: 'windows'
    )

    vlc_preparser_static_dep = executable('vlc-preparser-static',
        vlc_preparser_sources + vlc_win_extra,
        link_with: [libvlc, libvlccore, vlc_libcompat],
        include_directories: [vlc_include_dirs],
        dependencies: vlc_deps,
        c_args: [vlc_top_builddir_def, vlc_top_srcdir_def],
        objc_args: [vlc_top_builddir_def, vlc_top_srcdir_def],
        install_dir: pkglibexecdir_path,
        win_subsystem: 'windows'
    )

endif
