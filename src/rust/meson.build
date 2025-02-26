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

if get_option('rust').enabled()
    if meson.is_cross_build()
        rust_target = ['--target=' + meson.get_external_property('rust_triplet')]
        extra_cargo_args += rust_target
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
