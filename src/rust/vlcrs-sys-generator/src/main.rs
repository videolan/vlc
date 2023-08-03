//! Helper crate around bindgen for use in VLC

use std::{env, fmt::Write, path::PathBuf};

struct BindingsGenerator {
    include_path: String,
}

impl BindingsGenerator {
    /// Generate the raw bindings for a given list of headers.
    ///
    /// # Example
    ///
    /// ```rust,no_run
    /// # let bindings_gen = BindingsGenerator { include_path: ".".to_string() };
    /// bindings_gen.generate_bindings_for("vlcrs-tick", &["vlc_tick.h"], |builder| {
    ///     builder
    ///         .allowlist_function("date_.*")
    ///         .allowlist_function("vlc_tick_.*")
    ///         .allowlist_type("date_.*")
    ///         .allowlist_type("vlc_tick_.*")
    /// });
    /// ```
    fn generate_bindings_for(
        &self,
        for_: &str,
        headers: &[&str],
        builder: impl FnOnce(bindgen::Builder) -> bindgen::Builder,
    ) {
        println!("Generating bindings for {for_}...");

        let mut out_path = PathBuf::from(for_);
        out_path.push("src");
        out_path.push("sys.rs");

        // We need to generate the header file at runtime that will be passed to
        // bindgen to generate the requested bindings.
        //
        // We always include vlc_common.h since it's always required. for VLC include.
        let header_contents = ["vlc_common.h"].iter().chain(headers).fold(
            String::with_capacity(255),
            |mut contents, header| {
                writeln!(contents, "#include <{header}>").unwrap();
                contents
            },
        );

        let bindings = bindgen::Builder::default()
            // Since we're not integrated into the build system, because
            // bindgen depends on libclang, we need to pass the include path
            // manually.
            .clang_arg("-I")
            .clang_arg(&self.include_path)
            // The generated bindings won't respect the recommended Rust
            // code style, as they use C-style naming and descriptions, and
            // will therefore trigger warnings. We need to disable them for
            // these generated files.
            .raw_line("#![allow(rustdoc::bare_urls)]")
            .raw_line("#![allow(rustdoc::broken_intra_doc_links)]")
            .raw_line("#![allow(non_upper_case_globals)]")
            .raw_line("#![allow(non_camel_case_types)]")
            .raw_line("#![allow(non_snake_case)]")
            // Since we are generating the input header content at runtime,
            // specify wrapper.h as a fake name.
            .header_contents("wrapper.h", &header_contents);

        // Apply "user" configurations, ie allowlist_function, allowlist_type, ...
        // So that they can pit-point what they want.
        let bindings = builder(bindings);

        bindings
            .generate()
            .unwrap_or_else(|err| panic!("unable to generate the bindings for {for_}: {err:?}"))
            .write_to_file(&out_path)
            .unwrap_or_else(|err| {
                panic!(
                    "unable to write the generated the bindings for {for_} to {out_path:?}: {err:?}"
                )
            })
    }
}

fn main() {
    let bindings_gen = BindingsGenerator {
        include_path: env::var("INCLUDE_PATH").unwrap_or_else(|_| "../../include".to_string()),
    };

    bindings_gen.generate_bindings_for("vlcrs-messages", &["vlc_messages.h"], |builder| {
        builder
            .allowlist_function("vlc_Log")
            .allowlist_type("vlc_logger")
            .allowlist_type("vlc_log_type")
            .default_enum_style(bindgen::EnumVariation::Rust {
                non_exhaustive: true,
            })
    });
}
