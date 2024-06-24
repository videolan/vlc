//
// Copyright (C) 2024      Alexandre Janniaux <ajanni@videolabs.io>
//

#![feature(c_variadic)]
#![feature(associated_type_defaults)]
#![feature(extern_types)]
#![feature(fn_ptr_trait)]

mod test_common;
use crate::test_common::TestContext;

use vlcrs_macros::module;

use std::ffi::c_int;
use std::marker::PhantomData;

use vlcrs_plugin::{vlc_activate, vlc_deactivate};

unsafe extern "C"
fn activate_test<T: SpecificCapabilityModule>(_obj: *mut vlcrs_plugin::vlc_object_t) -> c_int
{
    0
}

unsafe extern "C"
fn deactivate_test<T: SpecificCapabilityModule>(_obj: *mut vlcrs_plugin::vlc_object_t)
{}

use vlcrs_plugin::ModuleProtocol;

pub struct ModuleLoader<T> { _phantom: PhantomData<T> }
impl<T> ModuleProtocol<T, vlc_activate, vlc_deactivate> for ModuleLoader<T>
    where T: SpecificCapabilityModule
{
    fn activate_function() -> vlc_activate
    {
        activate_test::<T>
    }

    fn deactivate_function() -> Option<vlc_deactivate>
    {
        Some(deactivate_test::<T>)
    }
}

/* Implement dummy module */
pub trait SpecificCapabilityModule : Sized {
    type Activate = vlc_activate;
    type Deactivate = vlc_deactivate;

    type Loader = ModuleLoader<Self>;

    fn open();
}
pub struct TestModule;
impl SpecificCapabilityModule for TestModule {
    fn open() {
        todo!()
    }
}

module! {
    type: TestModule (SpecificCapabilityModule),
    capability: "video_filter" @ 0,
    category: VIDEO_VFILTER,
    description: "A new module",
    shortname: "mynewmodule",
    shortcuts: ["mynewmodule_filter"],
}

#[test]
fn test_module_load_common_activate()
{
    use vlcrs_plugin::ModuleProperties;

    let mut context = TestContext::<vlc_activate> {
        command_cursor: 0,
        commands: vec![
            ModuleProperties::MODULE_CREATE,
            ModuleProperties::MODULE_NAME,
            ModuleProperties::MODULE_CAPABILITY,
            ModuleProperties::MODULE_SCORE,
            ModuleProperties::MODULE_DESCRIPTION,
            ModuleProperties::MODULE_SHORTNAME,
            ModuleProperties::MODULE_SHORTCUT,
            ModuleProperties::MODULE_CB_OPEN,
            ModuleProperties::MODULE_CB_CLOSE,
            ModuleProperties::CONFIG_CREATE,
            ModuleProperties::CONFIG_VALUE,
        ],
        open_cb: None,
        close_cb: None,
    };
    let ret = test_common::load_manifest(&mut context, vlc_entry);
    assert_eq!(ret, 0);
    assert_ne!(context.open_cb, None);
    assert_ne!(context.close_cb, None);

    unsafe {
        context.open_cb.unwrap()(std::ptr::null_mut());
        context.close_cb.unwrap()(std::ptr::null_mut());
    }
}
