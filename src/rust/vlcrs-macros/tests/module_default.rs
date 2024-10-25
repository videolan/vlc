//
// Copyright (C) 2024      Alexandre Janniaux <ajanni@videolabs.io>
//

#![feature(c_variadic)]
#![feature(associated_type_defaults)]
#![feature(extern_types)]
#![feature(fn_ptr_trait)]

mod common;
use common::TestContext;

use vlcrs_macros::module;

use std::ffi::{c_int, CStr};
use vlcrs_core::plugin::{vlc_activate, ModuleProtocol};

use vlcrs_core::object::Object;

unsafe extern "C" fn activate_filter(_obj: *mut Object) -> c_int {
    0
}

//
// Create an implementation loader for the TestFilterCapability
//
pub struct FilterModuleLoader;

///
/// Signal the core that we can load modules with this loader
///
impl<T> ModuleProtocol<T> for FilterModuleLoader
where
    T: TestNoDeactivateCapability,
{
    type Activate = vlc_activate;
    type Deactivate = *mut ();
    fn activate_function() -> vlc_activate {
        activate_filter
    }
}

/* Implement dummy module capability */
pub trait TestNoDeactivateCapability {}

///
/// Create a dummy module using this capability
///
pub struct TestModule;
impl TestNoDeactivateCapability for TestModule {}

//
// Define a module manifest using this module capability
// and this module.
//
module! {
    type: TestModule (FilterModuleLoader),
    capability: "video_filter" @ 0,
    category: VIDEO_VFILTER,
    description: "A new module",
    shortname: "mynewmodule",
    shortcuts: ["mynewmodule_filter"],
}

//
// This test uses the defined capability and module from above
// and tries to load the manifest and open an instance of the
// module.
//
#[test]
fn test_module_load_default_deactivate() {
    let version = unsafe { CStr::from_ptr(vlc_entry_api_version() as *const i8) };
    assert_eq!(version, c"4.0.6");

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
            ModuleProperties::CONFIG_CREATE,
            ModuleProperties::CONFIG_VALUE,
        ],
        open_cb: None,
        close_cb: None,
    };
    let ret = common::load_manifest(&mut context, vlc_entry);
    assert_eq!(ret, 0);
    assert_ne!(context.open_cb, None);
    assert_eq!(context.close_cb, None);
}
