rust_macros_extension = ''
if build_machine.system() == 'darwin'
    rust_macros_extension = 'dylib'
elif build_machine.system() == 'windows'
    rust_macros_extension = 'dll'
else
    rust_macros_extension = 'so'
endif

vlcrs_core_macros = custom_target('vlcrs_macros-cargo',
    capture: false,
    console: true,
    build_by_default: true,
    input: files(
        'Cargo.toml',
        'src/lib.rs',
        'src/module.rs',
    ),
    depfile: 'libvlcrs_macros.d',
    output: 'libvlcrs_macros.@0@'.format(rust_macros_extension),
    depends: module_cargo_depends,
    env: {
      'RUSTFLAGS': rust_flags,
      'CARGO_TARGET_DIR': cargo_target_dir
    },
    command: [cargo_output, '--output', '@OUTDIR@', '--depfile',
        cargo_bin, 'build', '--locked', extra_cargo_args,
        '--manifest-path', files('Cargo.toml'),
        '-p', 'vlcrs-macros']
)
