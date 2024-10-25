//
// Copyright (C) 2024      Alexandre Janniaux <ajanni@videolabs.io>
//

#![feature(c_variadic)]
#![feature(extern_types)]
#![feature(fn_ptr_trait)]

mod common;
use common::TestContext;

use vlcrs_macros::module;

use std::ffi::c_int;

use vlcrs_core::plugin::{vlc_activate, vlc_deactivate};

use vlcrs_core::object::Object;

unsafe extern "C" fn activate_test<T: SpecificCapabilityModule>(_obj: *mut Object) -> c_int {
    0
}

unsafe extern "C" fn deactivate_test<T: SpecificCapabilityModule>(_obj: *mut Object) {}

use vlcrs_core::plugin::ModuleProtocol;

pub struct ModuleLoader;
impl<T> ModuleProtocol<T> for ModuleLoader
where
    T: SpecificCapabilityModule,
{
    type Activate = vlc_activate;
    type Deactivate = vlc_deactivate;

    fn activate_function() -> Self::Activate {
        activate_test::<T>
    }

    fn deactivate_function() -> Option<Self::Deactivate> {
        Some(deactivate_test::<T>)
    }
}

/* Implement dummy module */
pub trait SpecificCapabilityModule {
    fn open();
}
pub struct TestModule;
impl SpecificCapabilityModule for TestModule {
    fn open() {
        todo!()
    }
}

module! {
    type: TestModule (ModuleLoader),
    capability: "video_filter" @ 0,
    category: VIDEO_VFILTER,
    description: "A new module",
    shortname: "mynewmodule",
    shortcuts: ["mynewmodule_filter"],
}

#[test]
fn test_module_load_common_activate() {
    use vlcrs_core::plugin::ModuleProperties;

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
    let ret = common::load_manifest(&mut context, vlc_entry);
    assert_eq!(ret, 0);
    assert_ne!(context.open_cb, None);
    assert_ne!(context.close_cb, None);

    unsafe {
        context.open_cb.unwrap()(std::ptr::null_mut());
        context.close_cb.unwrap()(std::ptr::null_mut());
    }
}
