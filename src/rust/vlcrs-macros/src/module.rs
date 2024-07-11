//! Module macros implementation

use proc_macro::TokenStream;
use quote::{quote, quote_spanned, ToTokens};
use syn::{
    braced, bracketed, parse::Parse, parse_macro_input, punctuated::Punctuated, spanned::Spanned,
    Attribute, Error, ExprRange, Ident, Lit, LitByteStr, LitInt, LitStr, MetaNameValue,
    RangeLimits, Token,
};

struct SectionInfo {
    name: LitStr,
    description: Option<LitStr>,
}

struct PrefixInfo {
    prefix: LitStr,
}

struct CapabilityInfo {
    capability: LitStr,
    score: LitInt,
}

struct ParameterInfo {
    name: Ident,
    type_: Ident,
    range: Option<ExprRange>,
    default_: Lit,
    text: LitStr,
    long_text: LitStr,
    prefix: Option<PrefixInfo>,
    section: Option<SectionInfo>,
    local_attrs: Vec<Attribute>,
}

struct ParametersInfo {
    params: Punctuated<ParameterInfo, Token![,]>,
}

struct ModuleInfo {
    type_: Ident,
    category: Ident,
    capability: CapabilityInfo,
    description: LitStr,
    help: Option<LitStr>,
    shortname: Option<LitStr>,
    prefix: Option<PrefixInfo>,
    params: Option<ParametersInfo>,
    shortcuts: Option<Punctuated<LitStr, Token![,]>>,
}

impl Parse for ModuleInfo {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        let mut type_ = None;
        let mut category = None;
        let mut capability = None;
        let mut description = None;
        let mut help = None;
        let mut shortcuts = None;
        let mut shortname = None;
        let mut params = None;
        let mut prefix = None;

        while !input.is_empty() {
            let global_attrs: Vec<Attribute> = input.call(Attribute::parse_inner)?;
            let local_attrs: Vec<Attribute> = input.call(Attribute::parse_outer)?;

            for global_attr in &global_attrs {
                return Err(Error::new_spanned(
                    global_attr,
                    "no global arguments are expected here",
                ));
            }

            // "type" is special because it's a keyword
            if input.peek(Token![type]) {
                input.parse::<Token![type]>()?;
                input.parse::<Token![:]>()?;
                type_ = Some(input.parse()?);
                input.parse::<Token![,]>()?;
                continue;
            }

            let key: Ident = input.parse()?;
            let key_name = key.to_string();
            let mut use_local_args = false;

            match key_name.as_str() {
                "capability" => {
                    input.parse::<Token![:]>()?;
                    capability = Some(input.parse()?);
                }
                "shortcuts" => {
                    input.parse::<Token![:]>()?;

                    let inner;
                    bracketed!(inner in input);
                    shortcuts = Some(inner.parse_terminated(<LitStr as syn::parse::Parse>::parse)?);
                }
                "shortname" => {
                    input.parse::<Token![:]>()?;
                    shortname = Some(input.parse()?);
                }
                "description" => {
                    input.parse::<Token![:]>()?;
                    description = Some(input.parse()?);
                }
                "help" => {
                    input.parse::<Token![:]>()?;
                    help = Some(input.parse()?);
                }
                "category" => {
                    input.parse::<Token![:]>()?;
                    category = Some(input.parse()?);
                }
                "params" => {
                    use_local_args = true;

                    if let [attr] = &local_attrs[..] {
                        match attr.path.get_ident() {
                            Some(ident) if ident == "prefix" => {
                                prefix = Some(attr.parse_meta()?.try_into()?);
                            }
                            Some(ident) => {
                                return Err(Error::new_spanned(
                                    attr,
                                    format!(
                                        "`#[{ident}]` was not expected here, only `#[prefix = \"...\"]` is"
                                    ),
                                ));
                            }
                            _ => {
                                return Err(Error::new_spanned(
                                    attr,
                                    format!("unexcepted, try `#[prefix = \"...\"]`"),
                                ));
                            }
                        }
                    } else {
                        return Err(Error::new_spanned(
                            key,
                            "one (and only one) attribute was expect here, try `#[prefix = \"...\"]`",
                        ));
                    }

                    input.parse::<Token![:]>()?;
                    params = Some(input.parse()?);
                }
                _ => {
                    return Err(Error::new_spanned(key, format!("unknow {key_name} key")));
                }
            }

            if !use_local_args {
                for local_attr in &local_attrs {
                    return Err(Error::new_spanned(
                        local_attr,
                        "no local arguments are expected here",
                    ));
                }
            }

            if !input.is_empty() {
                input.parse::<Token![,]>()?;
            }
        }

        let Some(type_) = type_ else {
            return Err(input.error("missing `type` key"));
        };

        let Some(capability) = capability else {
            return Err(input.error("missing `capability` key"));
        };

        let Some(category) = category else {
            return Err(input.error("missing `category` key"));
        };

        let Some(description) = description else {
            return Err(input.error("missing `description` key"));
        };

        if params.is_some() && prefix.is_none() {
            return Err(input.error("missing `#[prefix = \"...\"]` on `params`"));
        }

        Ok(ModuleInfo {
            type_,
            category,
            capability,
            description,
            help,
            shortcuts,
            shortname,
            params,
            prefix,
        })
    }
}

impl Parse for ParametersInfo {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        let content;
        braced!(content in input);
        Ok(ParametersInfo {
            params: content.parse_terminated(ParameterInfo::parse)?,
        })
    }
}

impl ParameterInfo {
    fn has_local_attr(&self, name: &str) -> bool {
        for attr in &self.local_attrs {
            if attr.path.is_ident(name) {
                return true;
            }
        }
        false
    }
}

impl TryFrom<syn::Meta> for PrefixInfo {
    type Error = syn::Error;

    fn try_from(value: syn::Meta) -> Result<Self, Self::Error> {
        let name_value: MetaNameValue = match value {
            syn::Meta::Path(_) | syn::Meta::List(_) => {
                return Err(Error::new_spanned(value, "expected name-value arguments"))
            }
            syn::Meta::NameValue(nv) => nv,
        };

        let lit_str = match name_value.lit {
            Lit::Str(lit_str) => lit_str,
            Lit::ByteStr(_)
            | Lit::Byte(_)
            | Lit::Char(_)
            | Lit::Int(_)
            | Lit::Float(_)
            | Lit::Bool(_)
            | Lit::Verbatim(_) => return Err(Error::new_spanned(name_value.lit, "expected a str")),
        };

        Ok(PrefixInfo { prefix: lit_str })
    }
}

impl Parse for SectionInfo {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        let mut name = None;
        let mut description = None;

        while !input.is_empty() {
            let key: Ident = input.parse()?;

            if key == "name" {
                input.parse::<Token![=]>()?;
                name = Some(input.parse()?);
            } else if key == "description" {
                input.parse::<Token![=]>()?;
                description = Some(input.parse()?);
            } else {
                let key_name = key.to_string();
                return Err(Error::new_spanned(key, format!("unknow {key_name} key")));
            }

            if !input.is_empty() {
                input.parse::<Token![,]>()?;
            }
        }

        let Some(name) = name else {
            return Err(input.error("missing `name` key"));
        };

        Ok(SectionInfo { name, description })
    }
}

impl Parse for CapabilityInfo {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        let capability = input.parse()?;
        input.parse::<Token![@]>()?;
        let score = input.parse()?;

        Ok(CapabilityInfo { capability, score })
    }
}

impl Parse for ParameterInfo {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        const LOCAL_ACCEPTED: [&str; 7] = [
            "deprecated",
            "rgb",
            "font",
            "savefile",
            "loadfile",
            "password",
            "directory",
        ];

        let global_attrs: Vec<Attribute> = input.call(Attribute::parse_inner)?;
        let local_attrs: Vec<Attribute> = input.call(Attribute::parse_outer)?;

        let mut section = None;
        let mut prefix = None;

        for attr in &global_attrs {
            // Replace with let_chains when stable
            if attr.path.is_ident("section") {
                section = Some(attr.parse_args::<SectionInfo>()?);
            } else {
                return Err(Error::new_spanned(
                    attr,
                    "only `#[section(name=\"\", description=\"\")]` is allowed here",
                ));
            }
        }

        for attr in &local_attrs {
            // Replace with let_chains when stable
            match attr.path.get_ident() {
                Some(ident) if LOCAL_ACCEPTED.contains(&ident.to_string().as_str()) => {
                    if ident == "prefix" {
                        prefix = Some(attr.parse_meta()?.try_into()?);
                    } else if ident != "deprecated" && !attr.tokens.is_empty() {
                        return Err(Error::new_spanned(
                            attr,
                            format!("`#[{ident}]` doesn't take any argument"),
                        ));
                    }
                }
                _ => {
                    return Err(Error::new_spanned(
                        attr,
                        "only `#[font]`, `#[savefile]`, `#[loadfile]`, `#[password]` \
                        and `#[deprecated = ...]` are supported",
                    ));
                }
            }
        }

        let name: Ident = input.parse()?;
        input.parse::<Token![:]>()?;
        let type_: Ident = input.parse()?;

        if type_ != "i64" && type_ != "f32" && type_ != "bool" && type_ != "str" {
            return Err(Error::new_spanned(
                type_,
                "only `i64`, `f32`, `bool` and `str` are supported",
            ));
        }

        let inner;
        braced!(inner in input);

        let mut default_ = None;
        let mut range = None;
        let mut text = None;
        let mut long_text = None;

        while !inner.is_empty() {
            // default is a kayword so we nned to special case it here
            if inner.peek(Token![default]) {
                inner.parse::<Token![default]>()?;
                inner.parse::<Token![:]>()?;
                default_ = Some(inner.parse()?);
                inner.parse::<Token![,]>()?;
                continue;
            }

            let key: Ident = inner.parse()?;
            let key_name = key.to_string();

            match key_name.as_str() {
                "range" => {
                    inner.parse::<Token![:]>()?;
                    range = Some({
                        let range = inner.parse::<ExprRange>()?;
                        if !range.attrs.is_empty() {
                            return Err(Error::new_spanned(range, "no attrs are supported here"));
                        }
                        if !matches!(range.limits, RangeLimits::Closed(_)) {
                            return Err(Error::new_spanned(
                                range,
                                "only half-open ranges (ie. 5..=10) are supported here",
                            ));
                        }
                        if range.from.is_none() || range.to.is_none() {
                            return Err(Error::new_spanned(
                                range,
                                "only half-open ranges (ie. -200..=100) are supported here",
                            ));
                        }
                        range
                    });
                }
                "text" => {
                    inner.parse::<Token![:]>()?;
                    text = Some(inner.parse()?);
                }
                "long_text" => {
                    inner.parse::<Token![:]>()?;
                    long_text = Some(inner.parse()?);
                }
                _ => {
                    return Err(Error::new_spanned(key, format!("unknow {key_name} key")));
                }
            }
            if !inner.is_empty() {
                inner.parse::<Token![,]>()?;
            }
        }

        let Some(default_) = default_ else {
            return Err(input.error("missing `default` key"));
        };

        let Some(text) = text else {
            return Err(input.error("missing `text` key"));
        };

        let Some(long_text) = long_text else {
            return Err(input.error("missing `long_text` key"));
        };

        Ok(ParameterInfo {
            name,
            type_,
            range,
            default_,
            text,
            long_text,
            prefix,
            section,
            local_attrs,
        })
    }
}

pub fn module(input: TokenStream) -> TokenStream {
    let ModuleInfo {
        type_,
        category,
        description,
        help,
        shortcuts,
        shortname,
        prefix,
        params,
        capability: CapabilityInfo { capability, score },
    } = parse_macro_input!(input as ModuleInfo);

    // TODO: Improve this with some kind environment variable passed by the build system
    // like what is done for the C side.
    let name = format!("{}-rs", type_.to_string().to_lowercase());
    let name_len = name.len() + 1;

    let vlc_param_name = |param: &ParameterInfo| -> String {
        let mut name = if let Some(prefix) = &param.prefix {
            prefix.prefix.value()
        } else if let Some(prefix) = &prefix {
            prefix.prefix.value()
        } else {
            unreachable!();
        };

        if !name.ends_with("_") && !name.ends_with("-") {
            name.push('-');
        }

        name.push_str(&param.name.to_string().replace("_", "-"));

        name
    };

    macro_rules! tt_c_str {
        ($span:expr => $value:expr) => {{
            LitByteStr::new(&format!("{}\0", $value).into_bytes(), $span).into_token_stream()
        }};
    }

    let description_with_nul = tt_c_str!(description.span() => description.value());
    let name_with_nul = tt_c_str!(type_.span() => name);

    let module = Ident::new(&capability.value(), capability.span());
    let capability_with_nul = tt_c_str!(capability.span() =>
                                        capability.value());

    let module_name = quote! {
        #[used]
        #[no_mangle]
        #[doc(hidden)]
        pub static vlc_module_name: &[u8; #name_len] = #name_with_nul;
    };

    // Copied from #define VLC_API_VERSION_STRING in include/vlc_plugin.h
    let entry_api_version = quote! {
        #[no_mangle]
        #[doc(hidden)]
        extern "C" fn vlc_entry_api_version() -> *const u8 {
            b"4.0.6\0".as_ptr()
        }
    };

    let entry_copyright = quote! {
        #[no_mangle]
        #[doc(hidden)]
        extern "C" fn vlc_entry_copyright() -> *const u8 {
            ::vlcrs_core::module::capi::VLC_COPYRIGHT_VIDEOLAN.as_ptr()
        }
    };

    let entry_license = quote! {
        #[no_mangle]
        #[doc(hidden)]
        extern "C" fn vlc_entry_license() -> *const u8 {
            ::vlcrs_core::module::capi::VLC_LICENSE_LGPL_2_1_PLUS.as_ptr()
        }
    };

    let module_entry_help = help.map(|help| {
        let help_with_nul = tt_c_str!(help.span() => help.value());
        quote! {
            if unsafe {
                vlc_set(
                    opaque,
                    module as _,
                    ::vlcrs_core::module::capi::ModuleProperties::VLC_MODULE_HELP as _,
                    #help_with_nul,
                )
            } != 0
            {
                return -1;
            }
        }
    });

    let module_entry_shortname = shortname.map(|shortname| {
        let shortname_with_nul = tt_c_str!(shortname.span() => shortname.value());
        quote! {
            if unsafe {
                vlc_set(
                    opaque,
                    module as _,
                    ::vlcrs_core::module::capi::ModuleProperties::VLC_MODULE_SHORTNAME as _,
                    #shortname_with_nul,
                )
            } != 0
            {
                return -1;
            }
        }
    });

    let module_entry_shortcuts = shortcuts.map(|shortcuts| {
        let shortcuts_with_nul: Vec<_> = shortcuts
            .into_iter()
            .map(|shortcut| tt_c_str!(shortcut.span() => shortcut.value()))
            .collect();

        let shortcuts_with_nul_len = shortcuts_with_nul.len();
        quote! {
            const SHORCUTS: [*const [u8]; #shortcuts_with_nul_len] = [#(#shortcuts_with_nul),*];
            if unsafe {
                vlc_set(
                    opaque,
                    module as _,
                    ::vlcrs_core::module::capi::ModuleProperties::VLC_MODULE_SHORTCUT as _,
                    #shortcuts_with_nul_len,
                    SHORCUTS.as_ptr(),
                )
            } != 0
            {
                return -1;
            }
        }
    });

    let vlc_entry_config_subcategory = {
        quote! {
            if unsafe {
                vlc_set(
                    opaque,
                    ::std::ptr::null_mut(),
                    ::vlcrs_core::module::capi::ModuleProperties::VLC_CONFIG_CREATE as _,
                    ::vlcrs_core::module::capi::ConfigModule::CONFIG_SUBCATEGORY as i64,
                    &mut config as *mut *mut ::vlcrs_core::module::capi::vlc_param,
                )
            } != 0
            {
                return -1;
            }
            if unsafe {
                vlc_set(
                    opaque,
                    config as _,
                    ::vlcrs_core::module::capi::ModuleProperties::VLC_CONFIG_VALUE as _,
                    ::vlcrs_core::module::capi::ConfigSubcat::#category as i64,
                )
            } != 0
            {
                return -1;
            }
        }
    };

    let type_params = params.as_ref().map(|params| {
        let struct_name = Ident::new(&format!("{}Args", type_), type_.span());

        let params_def = params.params.iter().map(|param| {
            let rust_name = &param.name;
            let ident_string = Ident::new("String", param.type_.span());
            let rust_type = if param.type_ == "str" {
                &ident_string
            } else {
                &param.type_
            };

            quote! {
                #rust_name: #rust_type,
            }
        });
        let params_assign = params.params.iter().map(|param| {
            let rust_name = &param.name;
            let vlc_name = vlc_param_name(&param);
            let vlc_name_with_nul = tt_c_str!(param.name.span()=> vlc_name);

            let method_name = Ident::new(if param.type_ == "i64" {
                "inherit_integer"
            } else if param.type_ == "f32" {
                "inherit_float"
            } else if param.type_ == "bool" {
                "inherit_bool"
            } else if param.type_ == "str" {
                "inherit_string"
            } else {
                unreachable!("unknown type_: {}", param.type_)
            }, param.type_.span());

            quote! {
                #rust_name: {
                    const VAR_NAME: &::std::ffi::CStr = unsafe { ::std::ffi::CStr::from_bytes_with_nul_unchecked(#vlc_name_with_nul) };
                    module_args.#method_name(VAR_NAME)?
                },
            }
        });

        quote! {
            #[derive(Debug, PartialEq)]
            struct #struct_name {
                #(#params_def)*
            }
            impl ::std::convert::TryFrom<&mut ::vlcrs_core::module::ModuleArgs> for #struct_name {
                type Error = ::vlcrs_core::error::CoreError;

                fn try_from(module_args: &mut ::vlcrs_core::module::ModuleArgs) ->
                        ::std::result::Result<Self, Self::Error> {
                    Ok(#struct_name {
                        #(#params_assign)*
                    })
                }
            }
        }
    });

    let vlc_entry_config_params = params.as_ref().map(|params| {
        let params = params.params.iter().map(|param| {
            let name = vlc_param_name(&param);
            let name_with_nul = tt_c_str!(param.name.span() => name);
            let text_with_nul = tt_c_str!(param.text.span() => param.text.value());
            let long_text_with_nul = tt_c_str!(param.long_text.span() => param.long_text.value());
            let (
                has_rgb,
                has_font,
                has_savefile,
                has_loadfile,
                has_password,
                has_directory,
                has_deprecated,
            ) = (
                param.has_local_attr("rgb"),
                param.has_local_attr("font"),
                param.has_local_attr("savefile"),
                param.has_local_attr("loadfile"),
                param.has_local_attr("password"),
                param.has_local_attr("directory"),
                param.has_local_attr("deprecated"),
            );

            let section = if let Some(section) = &param.section {
                let name_with_nul = tt_c_str!(section.name.span() => section.name.value());
                let description_with_nul = if let Some(description) = &section.description {
                    tt_c_str!(description.span() => description.value())
                } else {
                    quote! { ::std::ptr::null_mut() }
                };

                Some(quote! {
                    if unsafe {
                        vlc_set(
                            opaque,
                            ::std::ptr::null_mut(),
                            ::vlcrs_core::module::capi::ModuleProperties::VLC_CONFIG_CREATE as _,
                            ::vlcrs_core::module::capi::ConfigModule::CONFIG_SECTION as i64,
                            &mut config as *mut *mut ::vlcrs_core::module::capi::vlc_param,
                        )
                    } != 0
                    {
                        return -1;
                    }
                    if unsafe {
                        vlc_set(
                            opaque,
                            config.cast(),
                            ::vlcrs_core::module::capi::ModuleProperties::VLC_CONFIG_DESC as _,
                            #name_with_nul,
                            #description_with_nul,
                        )
                    } != 0
                    {
                        return -1;
                    }
                })
            } else {
                None
            };

            let (value, item_type, range_type) = if param.type_ == "i64" {
                let value = {
                    let default_ = &param.default_;
                    quote_spanned! {default_.span()=>
                        ::std::convert::Into::<i64>::into(#default_)
                    }
                };

                let item_type = if has_rgb {
                    quote! { ::vlcrs_core::module::capi::ConfigModule::CONFIG_ITEM_RGB }
                } else {
                    quote! { ::vlcrs_core::module::capi::ConfigModule::CONFIG_ITEM_INTEGER }
                };
                let range_type = Some(quote! { i64 });

                (value, item_type, range_type)
            } else if param.type_ == "f32" {
                let value = {
                    let default_ = &param.default_;
                    quote_spanned! {default_.span()=>
                        ::std::convert::Into::<::std::ffi::c_double>::into(#default_)
                    }
                };
                let item_type =
                    quote! { ::vlcrs_core::module::capi::ConfigModule::CONFIG_ITEM_FLOAT };
                let range_type = Some(quote! { ::std::ffi::c_double });

                (value, item_type, range_type)
            } else if param.type_ == "bool" {
                let value = {
                    let default_ = &param.default_;
                    quote_spanned! {default_.span()=>
                        ::std::convert::Into::<i64>::into(#default_)
                    }
                };
                let item_type =
                    quote! { ::vlcrs_core::module::capi::ConfigModule::CONFIG_ITEM_BOOL };

                (value, item_type, None)
            } else if param.type_ == "str" {
                let value = match param.default_ {
                    Lit::Str(ref default_) => tt_c_str!(default_.span() => default_.value()),
                    Lit::ByteStr(_)
                    | Lit::Byte(_)
                    | Lit::Char(_)
                    | Lit::Int(_)
                    | Lit::Float(_)
                    | Lit::Bool(_)
                    | Lit::Verbatim(_) => unreachable!(),
                };

                let item_type = if has_font {
                    quote! { ::vlcrs_core::module::capi::ConfigModule::CONFIG_ITEM_FONT }
                } else if has_savefile {
                    quote! { ::vlcrs_core::module::capi::ConfigModule::CONFIG_ITEM_SAVEFILE }
                } else if has_loadfile {
                    quote! { ::vlcrs_core::module::capi::ConfigModule::CONFIG_ITEM_LOADFILE }
                } else if has_password {
                    quote! { ::vlcrs_core::module::capi::ConfigModule::CONFIG_ITEM_PASSWORD }
                } else if has_directory {
                    quote! { ::vlcrs_core::module::capi::ConfigModule::CONFIG_ITEM_DIRECTORY }
                } else {
                    quote! { ::vlcrs_core::module::capi::ConfigModule::CONFIG_ITEM_STRING }
                };

                (value, item_type, None)
            } else {
                unreachable!();
            };

            let setup = quote! {
                if unsafe {
                    vlc_set(
                        opaque,
                        ::std::ptr::null_mut(),
                        ::vlcrs_core::module::capi::ModuleProperties::VLC_CONFIG_CREATE as _,
                        #item_type as i64,
                        &mut config as *mut *mut ::vlcrs_core::module::capi::vlc_param,
                    )
                } != 0
                {
                    return -1;
                }
                if unsafe {
                    vlc_set(
                        opaque,
                        config.cast(),
                        ::vlcrs_core::module::capi::ModuleProperties::VLC_CONFIG_DESC as _,
                        #text_with_nul,
                        #long_text_with_nul,
                    )
                } != 0
                {
                    return -1;
                }
                if unsafe {
                    vlc_set(
                        opaque,
                        config.cast(),
                        ::vlcrs_core::module::capi::ModuleProperties::VLC_CONFIG_NAME as _,
                        #name_with_nul,
                    )
                } != 0
                {
                    return -1;
                }
                if unsafe {
                    vlc_set(
                        opaque,
                        config.cast(),
                        ::vlcrs_core::module::capi::ModuleProperties::VLC_CONFIG_VALUE as _,
                        #value,
                    )
                } != 0
                {
                    return -1;
                }
            };

            let deprecated = if has_deprecated {
                Some(quote! {
                    if unsafe {
                        vlc_set(
                            opaque,
                            config.cast(),
                            ::vlcrs_core::module::capi::ModuleProperties::VLC_CONFIG_REMOVED as _,
                        )
                    } != 0
                    {
                        return -1;
                    }
                })
            } else {
                None
            };

            let range = if has_rgb {
                // TODO: check no other range specified
                Some(quote! {
                    if unsafe {
                        vlc_set(
                            opaque,
                            config.cast(),
                            ::vlcrs_core::module::capi::ModuleProperties::VLC_CONFIG_RANGE as _,
                            0,
                            0xFFFFFF
                        )
                    } != 0
                    {
                        return -1;
                    }
                })
            } else if let Some(range) = &param.range {
                if let Some(range_type) = range_type {
                    let from = match &range.from {
                        Some(from) => quote_spanned! {from.span()=>
                            ::std::convert::Into::<#range_type>::into(#from)
                        },
                        None => unreachable!(),
                    };
                    let to = match &range.to {
                        Some(to) => quote_spanned! {to.span()=>
                            ::std::convert::Into::<#range_type>::into(#to)
                        },
                        None => unreachable!(),
                    };
                    Some(quote! {
                        if unsafe {
                            vlc_set(
                                opaque,
                                config.cast(),
                                ::vlcrs_core::module::capi::ModuleProperties::VLC_CONFIG_RANGE as _,
                                #from,
                                #to
                            )
                        } != 0
                        {
                            return -1;
                        }
                    })
                } else {
                    unreachable!("this type doesn't support ranges")
                }
            } else {
                None
            };

            quote! {
                #section
                #setup
                #deprecated
                #range
            }
        });

        quote! {
            #(#params)*
        }
    });

    let module_entry = quote! {
        #[no_mangle]
        #[doc(hidden)]
        extern "C" fn vlc_entry(
            vlc_set: ::vlcrs_core::module::capi::vlc_set_cb,
            opaque: *mut ::std::ffi::c_void,
        ) -> i32 {
            let mut module: *mut ::vlcrs_core::module::capi::module_t = ::std::ptr::null_mut();
            let mut config: *mut ::vlcrs_core::module::capi::vlc_param = ::std::ptr::null_mut();
            let vlc_set = vlc_set.unwrap();

            if unsafe {
                vlc_set(
                    opaque,
                    ::std::ptr::null_mut(),
                    ::vlcrs_core::module::capi::ModuleProperties::VLC_MODULE_CREATE as _,
                    &mut module as *mut *mut ::vlcrs_core::module::capi::module_t,
                )
            } != 0
            {
                return -1;
            }
            if unsafe {
                vlc_set(
                    opaque,
                    module as _,
                    ::vlcrs_core::module::capi::ModuleProperties::VLC_MODULE_NAME as _,
                    #name_with_nul,
                )
            } != 0
            {
                return -1;
            }
            if unsafe {
                vlc_set(
                    opaque,
                    module as _,
                    ::vlcrs_core::module::capi::ModuleProperties::VLC_MODULE_CAPABILITY as _,
                    #capability_with_nul,
                )
            } != 0 {
                return -1;
            }
            if unsafe {
                vlc_set(
                    opaque,
                    module as _,
                    ::vlcrs_core::module::capi::ModuleProperties::VLC_MODULE_SCORE as _,
                    ::std::convert::Into::<i32>::into(#score),
                )
            } != 0 {
                return -1;
            }
            if unsafe {
                vlc_set(
                    opaque,
                    module as _,
                    ::vlcrs_core::module::capi::ModuleProperties::VLC_MODULE_DESCRIPTION as _,
                    #description_with_nul,
                )
            } != 0
            {
                return -1;
            }
            #module_entry_help
            #module_entry_shortname
            #module_entry_shortcuts
            if unsafe {
                vlc_set(
                    opaque,
                    module as _,
                    ::vlcrs_core::module::capi::ModuleProperties::VLC_MODULE_CB_OPEN as _,
                    b"module_open\0".as_ptr(),
                    ::vlcrs_core::module::#module::module_open::<#type_> as
                        unsafe extern "C" fn(*mut ::vlcrs_core::module::capi::vlc_object_t) -> i32,
                )
            } != 0
            {
                return -1;
            }
            if unsafe {
                vlc_set(
                    opaque,
                    module as _,
                    ::vlcrs_core::module::capi::ModuleProperties::VLC_MODULE_CB_CLOSE as _,
                    b"module_close\0".as_ptr(),
                    ::vlcrs_core::module::#module::module_close as
                        unsafe extern "C" fn(*mut ::vlcrs_core::module::capi::vlc_object_t) -> i32,
                )
            } != 0
            {
                return -1;
            }
            #vlc_entry_config_subcategory
            #vlc_entry_config_params
            0
        }
    };

    let expanded = quote! {
        #type_params
        #entry_api_version
        #entry_license
        #entry_copyright
        #module_name
        #module_entry
    };
    TokenStream::from(expanded)
}
