rust_optimization_args = {
    'plain': [],
    '0': [],
    'g': ['-C', 'opt-level=0'],
    '1': ['-C', 'opt-level=1'],
    '2': ['-C', 'opt-level=2'],
    '3': ['-C', 'opt-level=3'],
    's': ['-C', 'opt-level=s'],
}

extra_cargo_args = []
module_cargo_depends = []
rust_common_link_args = []
rust_target = []

rust_triplet = ''
if get_option('rust').enabled()
    if meson.is_cross_build()
        rust_triplet = meson.get_external_property('rust_triplet')
        rust_target = ['--target=' + rust_triplet]
        extra_cargo_args += rust_target
    else
        rustc_version = run_command('rustc', '-vV', check: true, capture: true)
        foreach line : rustc_version.stdout().split('\n')
            if line.startswith('host:')
                rust_triplet = line.split('host:')[1].strip()
            endif
        endforeach
    endif

    print_static_libs_rustc = run_command(cargo_rustc_static_libs, rust_target, cargo_bin,
        check: true, capture: true)
    rust_common_link_args = print_static_libs_rustc.stdout().split()
endif

# Do not try to reorder those two operations, they may trigger a bug in
# meson where array += element is not equal to array += array
rust_flags = get_option('extra_rust_flags')
rust_flags += rust_optimization_args[get_option('optimization')]

cargo_target_dir = join_paths(meson.current_build_dir(), 'cargo_target')

vlcrs_core = disabler()
if get_option('rust').enabled()
    subdir('vlcrs-core')
endif

vlcrs_core_macros = disabler()
if get_option('rust').enabled()
    subdir('vlcrs-macros')
endif

module_cargo_depends += [vlcrs_core, vlcrs_core_macros]

if get_option('rust').enabled() and not meson.is_cross_build()
    vlc_build_lib_dir = join_paths(meson.project_build_root(), 'src')

    rust_test_env = environment()
    rust_test_env.prepend('LD_LIBRARY_PATH', vlc_build_lib_dir)
    rust_test_env.prepend('DYLD_LIBRARY_PATH', vlc_build_lib_dir)
    rust_test_env.set('CARGO_TARGET_DIR', cargo_target_dir)

    cargo_link_override = [
        '--config', 'target.@0@.vlccore.rustc-link-lib=["vlccore"]'.format(rust_triplet),
        '--config', 'target.@0@.vlccore.rustc-link-search=["@1@"]'.format(rust_triplet, vlc_build_lib_dir),
        '--config', 'target.@0@.vlccore.rustc-link-arg=["-Wl,-rpath,@1@"]'.format(rust_triplet, vlc_build_lib_dir),
    ]

    rust_test_args = ['test', '--locked'] + extra_cargo_args + cargo_link_override + \
        ['--manifest-path', meson.project_source_root() / 'Cargo.toml']

    foreach crate : ['vlcrs-core', 'vlcrs-macros', 'vlcrs-messages', 'vlcrs-utils']
        test('rust-' + crate,
            cargo_bin,
            args: rust_test_args + ['-p', crate],
            env: rust_test_env,
            depends: [libvlccore, vlcrs_core, vlcrs_core_macros],
            timeout: 60,
            suite: 'rust',
        )
    endforeach
endif
