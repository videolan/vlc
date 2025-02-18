//! Module macros implementation

use proc_macro::TokenStream;
use proc_macro2::{Span, TokenStream as TokenStream2};
use quote::{quote, quote_spanned, ToTokens};
use syn::{
    braced, bracketed, parenthesized, parse::Parse, parse_macro_input, punctuated::Punctuated,
    spanned::Spanned, Attribute, Error, ExprRange, Ident, Lit, LitByteStr, LitInt, LitStr,
    MetaNameValue, RangeLimits, Token,
};

const VLC_API_VERSION_STRING: &str = "4.0.6";

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

struct SubmoduleInfo {
    submodule: ModuleInfo,
}

struct ModuleInfo {
    type_: Ident,
    loader: Ident,
    category: Ident,
    capability: CapabilityInfo,
    description: LitStr,
    help: Option<LitStr>,
    shortname: Option<LitStr>,
    prefix: Option<PrefixInfo>,
    params: Option<ParametersInfo>,
    shortcuts: Option<Punctuated<LitStr, Token![,]>>,
    submodules: Option<Punctuated<SubmoduleInfo, Token![,]>>,
}

impl Parse for ModuleInfo {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        let mut type_ = None;
        let mut loader = None;
        let mut category = None;
        let mut capability = None;
        let mut description = None;
        let mut help = None;
        let mut shortcuts = None;
        let mut shortname = None;
        let mut params = None;
        let mut prefix = None;
        let mut submodules = None;

        while !input.is_empty() {
            let global_attrs: Vec<Attribute> = input.call(Attribute::parse_inner)?;
            let local_attrs: Vec<Attribute> = input.call(Attribute::parse_outer)?;

            if let Some(global_attr) = global_attrs.first() {
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
                let parenthesis_content;
                parenthesized!(parenthesis_content in input);
                loader = Some(parenthesis_content.parse()?);
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
                                    "unexcepted, try `#[prefix = \"...\"]`",
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
                "submodules" => {
                    input.parse::<Token![:]>()?;

                    let inner;
                    bracketed!(inner in input);
                    let parsed_submodules = inner.parse_terminated(SubmoduleInfo::parse)?;

                    for submodule in &parsed_submodules {
                        if submodule.submodule.submodules.is_some() {
                            return Err(Error::new_spanned(
                                key,
                                "nested submodules are not allowed",
                            ));
                        }
                    }

                    submodules = Some(parsed_submodules);
                }
                _ => {
                    return Err(Error::new_spanned(key, format!("unknow {key_name} key")));
                }
            }

            if !use_local_args {
                if let Some(local_attr) = local_attrs.first() {
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

        let Some(loader) = loader else {
            return Err(input.error("invalid `type` key, missing module loader"));
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
            loader,
            category,
            capability,
            description,
            help,
            shortcuts,
            shortname,
            params,
            prefix,
            submodules,
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

impl Parse for SubmoduleInfo {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        let content;
        braced!(content in input);
        Ok(SubmoduleInfo {
            submodule: content.parse()?,
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

macro_rules! tt_c_str {
    ($span:expr => $value:expr) => {{
        LitByteStr::new(&format!("{}\0", $value).into_bytes(), $span).into_token_stream()
    }};
}

fn vlc_param_name(module_info: &ModuleInfo, param: &ParameterInfo) -> String {
    let mut name = if let Some(prefix) = &param.prefix {
        prefix.prefix.value()
    } else if let Some(prefix) = &module_info.prefix {
        prefix.prefix.value()
    } else {
        unreachable!();
    };

    if !name.ends_with("_") && !name.ends_with("-") {
        name.push('-');
    }

    name.push_str(&param.name.to_string().replace("_", "-"));

    name
}

#[allow(unused_variables)]
fn generate_module_code(module_info: &ModuleInfo) -> TokenStream2 {
    let ModuleInfo {
        type_,
        loader,
        category,
        description,
        help,
        shortcuts,
        shortname,
        prefix,
        params,
        capability: CapabilityInfo { capability, score },
        submodules,
    } = module_info;

    // TODO: Improve this with some kind environment variable passed by the build system
    // like what is done for the C side.
    let name = format!("{}-rs", type_.to_string().to_lowercase());

    let description_with_nul = tt_c_str!(description.span() => description.value());
    let name_with_nul = tt_c_str!(type_.span() => name);

    let module = &capability.value();
    let capability_with_nul = tt_c_str!(capability.span() =>
                                        capability.value());

    let module_entry_help = help.as_ref().map(|help| {
        let help_with_nul = tt_c_str!(help.span() => help.value());
        quote! {
            if unsafe {
                vlc_set(
                    opaque,
                    module as _,
                    ::vlcrs_core::plugin::ModuleProperties::MODULE_HELP as _,
                    #help_with_nul,
                )
            } != 0
            {
                return -1;
            }
        }
    });

    let module_entry_shortname = shortname.as_ref().map(|shortname| {
        let shortname_with_nul = tt_c_str!(shortname.span() => shortname.value());
        quote! {
            if unsafe {
                vlc_set(
                    opaque,
                    module as _,
                    ::vlcrs_core::plugin::ModuleProperties::MODULE_SHORTNAME as _,
                    #shortname_with_nul,
                )
            } != 0
            {
                return -1;
            }
        }
    });

    let module_entry_shortcuts = shortcuts.as_ref().map(|shortcuts| {
        let shortcuts_with_nul: Vec<_> = shortcuts
            .into_iter()
            .map(|shortcut| tt_c_str!(shortcut.span() => shortcut.value()))
            .collect();

        let shortcuts_with_nul_len = shortcuts_with_nul.len();
        quote! {
            {
                const SHORCUTS: [*const [u8]; #shortcuts_with_nul_len] = [#(#shortcuts_with_nul),*];
                if unsafe {
                    vlc_set(
                        opaque,
                        module as _,
                        ::vlcrs_core::plugin::ModuleProperties::MODULE_SHORTCUT as _,
                        #shortcuts_with_nul_len,
                        SHORCUTS.as_ptr(),
                    )
                } != 0
                {
                    return -1;
                }
            }
        }
    });

    let vlc_entry_config_subcategory = {
        quote! {
            if unsafe {
                vlc_set(
                    opaque,
                    ::std::ptr::null_mut(),
                    ::vlcrs_core::plugin::ModuleProperties::CONFIG_CREATE as _,
                    ::vlcrs_core::plugin::ConfigModule::SUBCATEGORY as std::ffi::c_int,
                    &mut config as *mut *mut ::vlcrs_core::plugin::vlc_param,
                )
            } != 0
            {
                return -1;
            }
            if unsafe {
                vlc_set(
                    opaque,
                    config as _,
                    ::vlcrs_core::plugin::ModuleProperties::CONFIG_VALUE as _,
                    ::vlcrs_core::plugin::ConfigSubcategory::#category as std::ffi::c_int,
                )
            } != 0
            {
                return -1;
            }
        }
    };

    let vlc_entry_config_params = params.as_ref().map(|params| {
        let params = params.params.iter().map(|param| {
            let name = vlc_param_name(module_info, param);
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
                            ::vlcrs_core::plugin::ModuleProperties::CONFIG_CREATE as _,
                            ::vlcrs_core::plugin::ConfigModule::SECTION as std::ffi::c_int,
                            &mut config as *mut *mut ::vlcrs_core::plugin::vlc_param,
                        )
                    } != 0
                    {
                        return -1;
                    }
                    if unsafe {
                        vlc_set(
                            opaque,
                            config.cast(),
                            ::vlcrs_core::plugin::ModuleProperties::CONFIG_DESC as _,
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
                    quote! { ::vlcrs_core::plugin::ConfigModule::ITEM_RGB }
                } else {
                    quote! { ::vlcrs_core::plugin::ConfigModule::ITEM_INTEGER }
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
                let item_type = quote! { ::vlcrs_core::plugin::ConfigModule::ITEM_FLOAT };
                let range_type = Some(quote! { ::std::ffi::c_double });

                (value, item_type, range_type)
            } else if param.type_ == "bool" {
                let value = {
                    let default_ = &param.default_;
                    quote_spanned! {default_.span()=>
                        ::std::convert::Into::<i64>::into(#default_)
                    }
                };
                let item_type = quote! { ::vlcrs_core::plugin::ConfigModule::ITEM_BOOL };

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
                    quote! { ::vlcrs_core::plugin::ConfigModule::ITEM_FONT }
                } else if has_savefile {
                    quote! { ::vlcrs_core::plugin::ConfigModule::ITEM_SAVEFILE }
                } else if has_loadfile {
                    quote! { ::vlcrs_core::plugin::ConfigModule::ITEM_LOADFILE }
                } else if has_password {
                    quote! { ::vlcrs_core::plugin::ConfigModule::ITEM_PASSWORD }
                } else if has_directory {
                    quote! { ::vlcrs_core::plugin::ConfigModule::ITEM_DIRECTORY }
                } else {
                    quote! { ::vlcrs_core::plugin::ConfigModule::ITEM_STRING }
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
                        ::vlcrs_core::plugin::ModuleProperties::CONFIG_CREATE as _,
                        #item_type as std::ffi::c_int,
                        &mut config as *mut *mut ::vlcrs_core::plugin::vlc_param,
                    )
                } != 0
                {
                    return -1;
                }
                if unsafe {
                    vlc_set(
                        opaque,
                        config.cast(),
                        ::vlcrs_core::plugin::ModuleProperties::CONFIG_DESC as _,
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
                        ::vlcrs_core::plugin::ModuleProperties::CONFIG_NAME as _,
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
                        ::vlcrs_core::plugin::ModuleProperties::CONFIG_VALUE as _,
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
                            ::vlcrs_core::plugin::ModuleProperties::CONFIG_REMOVED as _,
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
                            ::vlcrs_core::plugin::ModuleProperties::CONFIG_RANGE as _,
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
                                ::vlcrs_core::plugin::ModuleProperties::CONFIG_RANGE as _,
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

    // Ensure each submodule has uniquely named open/close functions
    // to prevent any naming conflicts.

    let module_open = format!("{}-open", type_);
    let module_close = format!("{}-close", type_);

    let module_open_with_nul = tt_c_str!(type_.span() => module_open);
    let module_close_with_nul = tt_c_str!(type_.span() => module_close);

    quote! {
        if unsafe {
            vlc_set(
                opaque,
                module as _,
                ::vlcrs_core::plugin::ModuleProperties::MODULE_CAPABILITY as _,
                #capability_with_nul,
            )
        } != 0 {
            return -1;
        }
        if unsafe {
            vlc_set(
                opaque,
                module as _,
                ::vlcrs_core::plugin::ModuleProperties::MODULE_SCORE as _,
                ::std::convert::Into::<i32>::into(#score),
            )
        } != 0 {
            return -1;
        }

        if unsafe {
            vlc_set(
                opaque,
                module as _,
                ::vlcrs_core::plugin::ModuleProperties::MODULE_DESCRIPTION as _,
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
                ::vlcrs_core::plugin::ModuleProperties::MODULE_CB_OPEN as _,
                #module_open_with_nul,
                unsafe {
                    <#loader as ModuleProtocol<#type_>>::activate_function()
                    as *mut std::ffi::c_void
                }
            )
        } != 0
        {
            return -1;
        }

        if <#loader as ModuleProtocol<#type_>>::deactivate_function() != None {
            if unsafe {
                vlc_set(
                    opaque,
                    module as _,
                    ::vlcrs_core::plugin::ModuleProperties::MODULE_CB_CLOSE as _,
                    #module_close_with_nul,
                    unsafe {
                        <#loader as ModuleProtocol<#type_>>::deactivate_function().unwrap()
                        as *mut std::ffi::c_void
                    }
                )
            } != 0
            {
                return -1;
            }
        }

        #vlc_entry_config_subcategory
        #vlc_entry_config_params
    }
}

fn vlc_symbol(symbol_name: &str, module_name: Option<&str>) -> Ident {
    if let Some(module_name) = module_name {
        Ident::new(
            &format!("{}__{}", symbol_name, module_name),
            Span::call_site(),
        )
    } else {
        Ident::new(symbol_name, Span::call_site())
    }
}

fn vlc_entry_api_version(module_suffix: Option<&str>) -> TokenStream2 {
    let symbol = vlc_symbol("vlc_entry_api_version", module_suffix);
    quote! {
        #[no_mangle]
        #[doc(hidden)]
        extern "C" fn #symbol() -> *const u8 {
            concat!(#VLC_API_VERSION_STRING, "\0").as_ptr()
        }
    }
}

fn vlc_entry_copyright(module_suffix: Option<&str>) -> TokenStream2 {
    let symbol = vlc_symbol("vlc_entry_copyright", module_suffix);
    quote! {
        #[no_mangle]
        #[doc(hidden)]
        extern "C" fn #symbol() -> *const u8 {
            ::vlcrs_core::plugin::VLC_COPYRIGHT_VIDEOLAN.as_ptr()
        }
    }
}

fn vlc_entry_license(module_suffix: Option<&str>) -> TokenStream2 {
    let symbol = vlc_symbol("vlc_entry_license", module_suffix);
    quote! {
        #[no_mangle]
        #[doc(hidden)]
        extern "C" fn #symbol() -> *const u8 {
            ::vlcrs_core::plugin::VLC_LICENSE_LGPL_2_1_PLUS.as_ptr()
        }
    }
}

pub fn module(input: TokenStream) -> TokenStream {
    let module_info = parse_macro_input!(input as ModuleInfo);

    // TODO: Improve this with some kind environment variable passed by the build system
    // like what is done for the C side.
    let module_suffix = module_info.type_.to_string().to_lowercase();
    let name = format!("{}-rs", module_suffix);
    let name_len = name.len() + 1;
    let module_suffix = module_suffix + "_rs";

    let name_with_nul = tt_c_str!(module_info.type_.span() => name);

    let module_name = quote! {
        #[used]
        #[no_mangle]
        #[doc(hidden)]
        pub static vlc_module_name: &[u8; #name_len] = #name_with_nul;
    };

    let entry_api_version = vlc_entry_api_version(None);
    let entry_copyright = vlc_entry_copyright(None);
    let entry_license = vlc_entry_license(None);

    let entry_api_version_module_name = vlc_entry_api_version(Some(&module_suffix));
    let entry_copyright_module_name = vlc_entry_copyright(Some(&module_suffix));
    let entry_license_module_name = vlc_entry_license(Some(&module_suffix));

    let type_params = module_info.params.as_ref().map(|params| {
        let struct_name = Ident::new(
            &format!("{}Args", module_info.type_),
            module_info.type_.span(),
        );

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

        quote! {
            #[derive(Debug, PartialEq)]
            struct #struct_name {
                #(#params_def)*
            }
        }
    });

    let module_entry_configs = generate_module_code(&module_info);

    let submodules_entry = module_info.submodules.as_ref().map(|submodule_info| {
        let submodules = submodule_info.iter().map(|submodule_info| {
            let submodule_entry_configs = generate_module_code(&submodule_info.submodule);

            quote! {
                if unsafe {
                    vlc_set(
                        opaque,
                        module as _,
                        ::vlcrs_core::plugin::ModuleProperties::MODULE_CREATE as _,
                        &mut module as *mut *mut ::vlcrs_core::plugin::module_t,
                    )
                } != 0
                {
                    return -1;
                }
                #submodule_entry_configs
            }
        });

        quote! {
            #(#submodules)*
        }
    });

    let vlc_entry = |module_suffix: Option<&str>| {
        let symbol = vlc_symbol("vlc_entry", module_suffix);
        quote! {
            #[no_mangle]
            #[doc(hidden)]
            extern "C" fn #symbol(
                vlc_set: ::vlcrs_core::plugin::sys::vlc_set_cb,
                opaque: *mut ::std::ffi::c_void,
            ) -> i32 {
                use vlcrs_core::plugin::ModuleProtocol;
                let mut module: *mut ::vlcrs_core::plugin::module_t = ::std::ptr::null_mut();
                let mut config: *mut ::vlcrs_core::plugin::vlc_param = ::std::ptr::null_mut();

                if unsafe {
                    vlc_set(
                        opaque,
                        ::std::ptr::null_mut(),
                        ::vlcrs_core::plugin::ModuleProperties::MODULE_CREATE as _,
                        &mut module as *mut *mut ::vlcrs_core::plugin::module_t,
                    )
                } != 0
                {
                    return -1;
                }
                if unsafe {
                    vlc_set(
                        opaque,
                        module as _,
                        ::vlcrs_core::plugin::ModuleProperties::MODULE_NAME as _,
                        #name_with_nul,
                    )
                } != 0
                {
                    return -1;
                }
                #module_entry_configs
                #submodules_entry
                0
            }
        }
    };
    let module_entry_module_name = vlc_entry(Some(&module_suffix));
    let module_entry = vlc_entry(None);

    let cfg_static = quote! { #[cfg(vlc_static_plugins)] };
    let cfg_not_static = quote! { #[cfg(not(vlc_static_plugins))] };

    let expanded = quote! {
        #type_params

        #cfg_static
        #entry_api_version_module_name

        #cfg_static
        #entry_license_module_name

        #cfg_static
        #entry_copyright_module_name

        #cfg_static
        #module_entry_module_name

        #cfg_not_static
        #entry_api_version

        #cfg_not_static
        #entry_license

        #cfg_not_static
        #entry_copyright

        #cfg_not_static
        #module_name

        #cfg_not_static
        #module_entry
    };
    TokenStream::from(expanded)
}
