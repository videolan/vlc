fn main() {
    // When a build script override is provided via .cargo/config.toml
    // or via `cargo --config`, this script is not compiled or run.
    //
    // Without an override, set a cfg flag so that lib.rs can fail on
    // test compilation and produces a proper error message instead of
    // linker failure. This is needed because we can't test for "test"
    // configuration from the build script directly, and forcing to have
    // the proper parameter to setup link flag here breaks cargo check
    // even though it's not linking anyway.
    println!("cargo::rustc-cfg=vlccore_not_linked");
}
