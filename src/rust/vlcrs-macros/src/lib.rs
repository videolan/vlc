//! vlcrs-core macros

use proc_macro::TokenStream;

mod module;

/// Module macro
///
/// ```no_run
/// # use vlcrs_core_macros::module;
/// module! {
///     type: MyModule,
///     shortname: "infrs",
///     shortcuts: ["mp4", "MP4A"],
///     description: "This a Rust Module - inflate-rs",
///     help: "This is a dummy help text",
///     category: SUBCAT_INPUT_STREAM_FILTER,
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
/// # use vlcrs_core_macros::module;
/// module! {
///     type: Inflate,
///     shortcuts: ["mp4", "MP4A"],
///     shortname: "infrs",
///     description: "This a Rust Module - inflate-rs",
///     help: "This is a dummy help text",
///     category: SUBCAT_INPUT_STREAM_FILTER,
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
