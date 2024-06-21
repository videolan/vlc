# Rust bindings for vlccore #{rust_bindings}

_libvlccore_ provide [Rust] bindings so as to interface with the core
implementation in C and implement VLC plugins in [Rust] as a crate.

The bindings are located in the `src/rust/` folder for everything which
is mapping headers from `include/vlc_*.h`. There's no module-specific
crate for now, but those would be located in `modules/`.

[Rust]: https://www.rust-lang.org/

## Creating a new core crate

New core crate needs to be placed into the `src/rust/` directory. They
usually match with another C `vlc_*.h` header. For instance `vlc_tick.h`
will match with the `src/rust/vlcrs-tick` crate.

Cargo manifest for the workspace at `src/rust/Cargo.toml` needs to be
updated to account for the new crate.

```diff
diff --git a/src/rust/Cargo.toml b/src/rust/Cargo.toml
index b1a9555a23..72ca86951d 100644
--- a/src/rust/Cargo.toml
+++ b/src/rust/Cargo.toml
@@ -1,5 +1,6 @@
 [workspace]
 members = [
+    "vlcrs-newcrate",
     "vlcrs-messages",
     "vlcrs-sys-generator"
 ]
```

A core crate might depend on another core crate, in particular when a
`vlc_*.h` header is depending on another header or uses forward
declarations.

In that case, the Cargo.toml from this crate should directly reference
the other crate using a path locator.

```toml
[package]
name = "vlcrs-messages"
edition = "2021"
version.workspace = true
license.workspace = true

[dependencies]
vlcrs-utils = { path = "../vlcrs-utils" }
```

Finally, if the new crate is meant to be available from modules, it
should also be added to the `modules/Cargo.toml` workspace file using a
path locator, inside the workspace dependencies.

```toml
# modules/Cargo.toml
[workspace]
resolver = "2"
members = ["/*-rs"]

[workspace.dependencies]
vlcrs-tick = { path = "../src/rust/vlcrs-tick" }
vlcrs-messages = { path = "../src/rust/vlcrs-messages" }
# ...
```

## Module creation

To create a new module, create a new crate inside the relevant
`moduledir` folder from `modules/` folder using Cargo, ending with `-rs`
and without version control creation given that VLC sources are already
versioned:

```bash
cd modules/moduledir && cargo new --lib --vcs none mynewmodule-rs
```

For instance, a new video filter would be created with:

```bash
cd modules/video_filter && cargo new --lib --vcs none mynewfilter-rs
```

Then, you can modify the Cargo.toml file to include the different crates
the module will be using from the workspace.

```toml
[package]
name = "mynewmodule-rs"
edition = "2021"
version = "0.1.0"

[dependencies]
vlcrs-messages.workspace = true
```

The module will need to be listed into `modules/Cargo.toml` members:

```toml
[workspace]
# ...
members = [
    # ...
    "moduledir/mynewmodule-rs"
]
```

Then you can add the plugin to the buildsystems. For automake, check the
parent Makefile.am or create a new one depending on how crowded it is,
and add:

```make
libmynewmodule_rs.la:
	@$(LIBTOOL_CARGO) $(srcdir)/moduledir/mynewmodule-rs/ $@

libmynewmodule_rs_plugin_la_SOURCES = \
	mynewmodule-rs/Cargo.toml \
	mynewmodule-rs/src/lib.rs 
libmynewmodule_rs_plugin_la_LIBADD = libmynewmodule_rs.la

if HAVE_RUST
moduledir_LTLIBRARIES += libmynewmodule_rs_plugin.la
# Example: video_filter_LTLIBRARIES += libmynewfilter_rs_plugin.la
endif
```

For meson, you will use the `vlc_rust_modules` dictionary from the
parent meson.build file:

```
vlc_rust_modules += {
    'name' : 'mynewmodule_rs',
    'sources' : files('mynewmodule-rs/src/lib.rs'),
    'cargo_toml' : files('mynewmodule-rs/Cargo.toml'),
}
```

Creating a new plugin requires the definition of the plugin manifest to
expose the different modules from there. This is done through the
`module!{}` macro:

```rust
use vlcrs_plugin::module;
use vlcrs_video_filter::FilterModule;

pub struct MyNewModule {};

impl FilterModule for MyNewModule {
    fn open<'a> (filter: Filter<'a>) -> Result<()> {
        todo!();
    }
}

module! {
    type: MyNewModule,
    capability: "video filter" @ 0,
    category: SUBCAT_VIDEO_FILTER,
    description: "A new module",
    shortname: "mynewmodule",
    shortcuts: ["mynewmodule_filter"],
}
```
