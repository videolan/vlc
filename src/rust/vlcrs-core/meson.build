vlcrs_core = custom_target('vlcrs_core-cargo',
    capture: false,
    console: true,
    build_by_default: true,
    input: files(
        'Cargo.toml',
        'src/object/mod.rs',
        'src/object/sys.rs',
        'src/plugin/mod.rs',
        'src/plugin/sys.rs',
        'src/tracer/mod.rs',
        'src/tracer/sys.rs',
        'src/lib.rs',
    ),
    output: 'libvlcrs_core.rlib',
    depfile: 'libvlcrs_core.d',
    depends: module_cargo_depends,
    env: {
      'RUSTFLAGS': rust_flags,
      'CARGO_TARGET_DIR': cargo_target_dir
    },
    command: [cargo_output, '--output', '@OUTDIR@', '--depfile',
        cargo_bin, 'build', '--locked', extra_cargo_args,
        '--manifest-path', files('Cargo.toml'),
        '-p', 'vlcrs-core']
)
