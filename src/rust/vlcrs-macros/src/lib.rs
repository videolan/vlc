//! vlcrs-core macros

use proc_macro::TokenStream;

mod module;

/// Module macro
///
/// ```no_run
/// # #![feature(associated_type_defaults)]
/// # use vlcrs_macros::module;
/// # use vlcrs_core::plugin::ModuleProtocol;
/// # use std::ffi::{c_int, c_void};
/// # type ActivateFunction = unsafe extern "C" fn() -> c_int;
/// # type DeactivateFunction = unsafe extern "C" fn() -> c_void;
/// # pub trait CapabilityTrait {}
/// # extern "C" fn activate() -> c_int{ 0 }
/// # pub struct CapabilityTraitLoader;
/// # impl<T: CapabilityTrait> ModuleProtocol<T> for CapabilityTraitLoader {
/// #     type Activate = ActivateFunction;
/// #     type Deactivate = DeactivateFunction;
/// #     fn activate_function() -> ActivateFunction { activate }
/// #     fn deactivate_function() -> Option<DeactivateFunction> { None }
/// # }
/// struct MyModule {}
/// impl CapabilityTrait for MyModule {}
/// module! {
///     type: MyModule (CapabilityTraitLoader),
///     shortname: "infrs",
///     shortcuts: ["mp4", "MP4A"],
///     description: "This a Rust Module - inflate-rs",
///     help: "This is a dummy help text",
///     category: INPUT_STREAM_FILTER,
///     capability: "stream_filter" @ 330,
///     #[prefix = "infrs"]
///     params: {
///         #[deprecated]
///         my_bool: i64 {
///             default: 5,
///             range: 0..=6,
///             text: "My bool",
///             long_text: "Explain the purpose!",
///         },
///     }
/// }
/// ```
///
/// ## Parameters attribute
///
/// - `#[rgb]`
/// - `#[font]`
/// - `#[savefile]`
/// - `#[loadfile]`
/// - `#[password]`
/// - `#[directory]`
/// - `#[deprecated]`
///
/// There is also `section` attribute:
/// `#![section(name = "My Section", description = "The description")]`
///
/// ## Complete example
///
/// ```no_run
/// # #![feature(associated_type_defaults)]
/// # use vlcrs_macros::module;
/// # use vlcrs_core::plugin::ModuleProtocol;
/// # use std::ffi::{c_int, c_void};
/// # type ActivateFunction = unsafe extern "C" fn() -> c_int;
/// # type DeactivateFunction = unsafe extern "C" fn() -> c_void;
/// # pub trait CapabilityTrait {}
/// # extern "C" fn activate() -> c_int{ 0 }
/// # pub struct CapabilityTraitLoader;
/// # impl<T: CapabilityTrait> ModuleProtocol<T>
/// # for CapabilityTraitLoader {
/// #     type Activate = ActivateFunction;
/// #     type Deactivate = DeactivateFunction;
/// #     fn activate_function() -> ActivateFunction { activate }
/// #     fn deactivate_function() -> Option<DeactivateFunction> { None }
/// # }
/// struct Inflate {}
/// impl CapabilityTrait for Inflate {}
/// module! {
///     type: Inflate (CapabilityTraitLoader),
///     shortcuts: ["mp4", "MP4A"],
///     shortname: "infrs",
///     description: "This a Rust Module - inflate-rs",
///     help: "This is a dummy help text",
///     category: INPUT_STREAM_FILTER,
///     capability: "stream_filter" @ 330,
///     #[prefix = "infrs"]
///     params: {
///         #![section(name = "", description = "kk")]
///         #[deprecated]
///         my_bool: bool {
///             default: false,
///             text: "",
///             long_text: "",
///         },
///         my_i32: i64 {
///             default: 5,
///             range: -2..=2,
///             text: "",
///             long_text: "",
///         },
///         my_f32: f32 {
///             default: 5.,
///             range: -2.0..=2.0,
///             text: "",
///             long_text: "",
///         },
///         my_str: str {
///             default: "aaa",
///             text: "",
///             long_text: "",
///         },
///         #[loadfile]
///         my_loadfile: str {
///             default: "aaa",
///             text: "",
///             long_text: "",
///         },
///     }
/// }
/// ```
#[proc_macro]
pub fn module(input: TokenStream) -> TokenStream {
    module::module(input)
}
