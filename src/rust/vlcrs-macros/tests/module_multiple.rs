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
use vlcrs_core::plugin::ModuleProtocol;

extern "C" {
    // Create a dummy different type to change the activation function.
    #[allow(non_camel_case_types)]
    pub type vlc_filter_t;
}

#[allow(non_camel_case_types)]
type vlc_filter_activate = unsafe extern "C" fn(_obj: *mut vlc_filter_t, valid: &mut bool) -> c_int;

unsafe extern "C" fn activate_filter<T: TestFilterCapability>(
    _obj: *mut vlc_filter_t,
    valid: &mut bool,
) -> c_int {
    T::open(_obj, valid);
    0
}

unsafe extern "C" fn activate_other_filter<T: TestOtherCapability>(
    _obj: *mut vlc_filter_t,
    valid: &mut bool,
) -> c_int {
    T::open(_obj, valid);
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
    T: TestFilterCapability,
{
    type Activate = vlc_filter_activate;
    fn activate_function() -> Self::Activate {
        activate_filter::<T>
    }
}

/* Implement dummy module capability */
pub trait TestFilterCapability {
    fn open(obj: *mut vlc_filter_t, bool: &mut bool);
}

///
/// Create a dummy module using this capability
///
pub struct TestModuleFilter;
impl TestFilterCapability for TestModuleFilter {
    fn open(_obj: *mut vlc_filter_t, valid: &mut bool) {
        *valid = true;
    }
}

/* Implement dummy module capability */
pub trait TestOtherCapability {
    fn open(obj: *mut vlc_filter_t, bool: &mut bool);
}

struct TestOtherCapabilityLoader;
impl<T> ModuleProtocol<T> for TestOtherCapabilityLoader
where
    T: TestOtherCapability,
{
    type Activate = vlc_filter_activate;
    fn activate_function() -> Self::Activate {
        activate_other_filter::<T>
    }
}

///
/// Create a dummy module using this capability
///
impl TestOtherCapability for TestModuleFilter {
    fn open(_obj: *mut vlc_filter_t, valid: &mut bool) {
        *valid = true;
    }
}

//
// Define a module manifest using this module capability
// and this module.
//
module! {
    type: TestModuleFilter (FilterModuleLoader),
    capability: "video_filter" @ 0,
    category: VIDEO_VFILTER,
    description: "A new module",
    shortname: "mynewmodule",
    shortcuts: ["mynewmodule_filter"],
    submodules: [
        {
            type: TestModuleFilter (TestOtherCapabilityLoader),
            capability: "other_capability" @ 0,
            category: VIDEO_VFILTER,
            description: "Another module",
            shortname: "othermodule"
        }
    ]
}

//
// This test uses the defined capability and module from above
// and tries to load the manifest and open an instance of the
// module.
//
#[test]
fn test_module_manifest_multiple_capabilities() {
    use vlcrs_core::plugin::ModuleProperties;
    let mut context = TestContext::<vlc_filter_activate> {
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
            ModuleProperties::MODULE_CREATE,
            ModuleProperties::MODULE_CAPABILITY,
            ModuleProperties::MODULE_SCORE,
            ModuleProperties::MODULE_DESCRIPTION,
            ModuleProperties::MODULE_SHORTNAME,
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

    let mut valid = false;
    unsafe {
        context.open_cb.unwrap()(std::ptr::null_mut(), &mut valid);
    }

    assert!(valid, "The open from the module must have been called");
}
