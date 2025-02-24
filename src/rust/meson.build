rust_optimization_args = {
    'plain': [],
    '0': [],
    'g': ['-C', 'opt-level=0'],
    '1': ['-C', 'opt-level=1'],
    '2': ['-C', 'opt-level=2'],
    '3': ['-C', 'opt-level=3'],
    's': ['-C', 'opt-level=s'],
}

print_static_libs_rustc = run_command([cargo_rustc_static_libs, cargo_bin],
    check: true, capture: true)
rust_common_link_args = print_static_libs_rustc.stdout().split()

# Do not try to reorder those two operations, they may trigger a bug in
# meson where array += element is not equal to array += array
rust_flags = get_option('extra_rust_flags')
rust_flags += rust_optimization_args[get_option('optimization')]

cargo_target_dir = join_paths(meson.current_build_dir(), 'cargo_target')

extra_cargo_args = []
module_cargo_depends = []

subdir('vlcrs-core')
subdir('vlcrs-macros')

module_cargo_depends += [vlcrs_core, vlcrs_core_macros]
