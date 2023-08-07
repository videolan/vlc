//! Utilities functions for vlcrs crates

/// Function name getter.
///
/// This macro returns the name of the enclosing function.
/// As the internal implementation is based on the [`std::any::type_name`], this macro derives
/// all the limitations of this function.
//
// Originate from stdext-rs (MIT-License: "Copyright (c) 2020 Igor Aleksanov"):
//  - https://github.com/popzxc/stdext-rs/blob/a79153387aa3f08d08dcaff08e214a17851d61c4/src/macros.rs#L63-L74
#[macro_export]
macro_rules! func {
    () => {{
        // Okay, this is ugly, I get it. However, this is the best we can get on a stable rust.
        fn f() {}
        fn type_name_of<T>(_: T) -> &'static str {
            std::any::type_name::<T>()
        }
        let name = type_name_of(f);
        // `3` is the length of the `::f`.
        &name[..name.len() - 3]
    }};
}
