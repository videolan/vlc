pub mod sys;

use crate::object::Object;

pub const VLC_COPYRIGHT_VIDEOLAN: &str = r#"
\x43\x6f\x70\x79\x72\x69\x67\x68\x74\x20\x28\x43\x29\x20\x74\x68
\x65\x20\x56\x69\x64\x65\x6f\x4c\x41\x4e\x20\x56\x4c\x43\x20\x6d
\x65\x64\x69\x61\x20\x70\x6c\x61\x79\x65\x72\x20\x64\x65\x76\x65
\x6c\x6f\x70\x65\x72\x73"#;

pub const VLC_LICENSE_LGPL_2_1_PLUS: &str = r#"
\x4c\x69\x63\x65\x6e\x73\x65\x64\x20\x75\x6e\x64\x65\x72\x20\x74
\x68\x65\x20\x74\x65\x72\x6d\x73\x20\x6f\x66\x20\x74\x68\x65\x20
\x47\x4e\x55\x20\x4c\x65\x73\x73\x65\x72\x20\x47\x65\x6e\x65\x72
\x61\x6c\x20\x50\x75\x62\x6c\x69\x63\x20\x4c\x69\x63\x65\x6e\x73
\x65\x2c\x20\x76\x65\x72\x73\x69\x6f\x6e\x20\x32\x2e\x31\x20\x6f"#;

#[allow(non_camel_case_types)]
#[allow(unused)]
extern "C" {
    pub type module_t;
    pub type vlc_param;
}

#[allow(non_camel_case_types)]
#[repr(u32)]
pub enum ConfigModule {
    HINT_CATEGORY = 0x02,
    SUBCATEGORY = 0x07,
    SECTION = 0x08,
    ITEM_FLOAT = 1 << 5,
    ITEM_INTEGER = 2 << 5,
    ITEM_RGB = ConfigModule::ITEM_INTEGER as u32 | 0x01,
    ITEM_BOOL = 3 << 5,
    ITEM_STRING = 4 << 5,
    ITEM_PASSWORD = ConfigModule::ITEM_STRING as u32 | 0x01,
    ITEM_KEY = ConfigModule::ITEM_STRING as u32 | 0x02,
    ITEM_MODULE = ConfigModule::ITEM_STRING as u32 | 0x04,
    ITEM_MODULE_CAT = ConfigModule::ITEM_STRING as u32 | 0x05,
    ITEM_MODULE_LIST = ConfigModule::ITEM_STRING as u32 | 0x06,
    ITEM_MODULE_LIST_CAT = ConfigModule::ITEM_STRING as u32 | 0x07,
    ITEM_LOADFILE = ConfigModule::ITEM_STRING as u32 | 0x0C,
    ITEM_SAVEFILE = ConfigModule::ITEM_STRING as u32 | 0x0D,
    ITEM_DIRECTORY = ConfigModule::ITEM_STRING as u32 | 0x0E,
    ITEM_FONT = ConfigModule::ITEM_STRING as u32 | 0x0F,
}

#[allow(non_camel_case_types)]
#[repr(i32)]
pub enum ConfigCategory {
    HIDDEN = -1,
    UNKNOWN = 0,
    INTERFACE = 1,
    AUDIO = 2,
    VIDEO = 3,
    INPUT = 4,
    SOUT = 5,
    ADVANCED = 6,
    PLAYLIST = 7,
}

#[allow(non_camel_case_types)]
#[repr(i32)]
pub enum ConfigSubcategory {
    HIDDEN = -1,
    UNKNOWN = 0,

    INTERFACE_GENERAL = 101,
    INTERFACE_MAIN = 102,
    INTERFACE_CONTROL = 103,
    INTERFACE_HOTKEYS = 104,

    AUDIO_GENERAL = 201,
    AUDIO_AOUT = 202,
    AUDIO_AFILTER = 203,
    AUDIO_VISUAL = 204,
    AUDIO_RESAMPLER = 206,

    VIDEO_GENERAL = 301,
    VIDEO_VOUT = 302,
    VIDEO_VFILTER = 303,
    VIDEO_SUBPIC = 305,
    VIDEO_SPLITTER = 306,

    INPUT_GENERAL = 401,
    INPUT_ACCESS = 402,
    INPUT_DEMUX = 403,
    INPUT_VCODEC = 404,
    INPUT_ACODEC = 405,
    INPUT_SCODEC = 406,
    INPUT_STREAM_FILTER = 407,

    SOUT_GENERAL = 501,
    SOUT_STREAM = 502,
    SOUT_MUX = 503,
    SOUT_ACO = 504,
    SOUT_PACKETIZER = 505,
    SOUT_VOD = 507,
    SOUT_RENDERER = 508,

    ADVANCED_MISC = 602,
    ADVANCED_NETWORK = 603,

    PLAYLIST_GENERAL = 701,
    PLAYLIST_SD = 702,
    PLAYLIST_EXPORT = 703,
}

#[derive(Debug, PartialEq, PartialOrd)]
#[allow(non_camel_case_types)]
#[repr(i32)]
pub enum ModuleProperties {
    MODULE_CREATE,
    CONFIG_CREATE,

    MODULE_CPU_REQUIREMENT = 0x100,
    MODULE_SHORTCUT,
    MODULE_CAPABILITY,
    MODULE_SCORE,
    MODULE_CB_OPEN,
    MODULE_CB_CLOSE,
    MODULE_NO_UNLOAD,
    MODULE_NAME,
    MODULE_SHORTNAME,
    MODULE_DESCRIPTION,
    MODULE_HELP,
    MODULE_TEXTDOMAIN,
    MODULE_HELP_HTML,

    CONFIG_NAME = 0x1000,
    CONFIG_VALUE,
    CONFIG_RANGE,
    CONFIG_ADVANCED_RESERVED,
    CONFIG_VOLATILE,
    CONFIG_PERSISTENT_OBSOLETE,
    CONFIG_PRIVATE,
    CONFIG_REMOVED,
    CONFIG_CAPABILITY,
    CONFIG_SHORTCUT,
    CONFIG_OLDNAME_OBSOLETE,
    CONFIG_SAFE,
    CONFIG_DESC,
    CONFIG_LIST_OBSOLETE,
    CONFIG_ADD_ACTION_OBSOLETE,
    CONFIG_LIST,
    CONFIG_LIST_CB_OBSOLETE,
}

impl TryFrom<i32> for ModuleProperties {
    type Error = ();

    fn try_from(opcode: i32) -> Result<Self, Self::Error> {
        let prop = match opcode {
            x if x == ModuleProperties::MODULE_CREATE as i32 => ModuleProperties::MODULE_CREATE,
            x if x == ModuleProperties::CONFIG_CREATE as i32 => ModuleProperties::CONFIG_CREATE,

            x if x == ModuleProperties::MODULE_CPU_REQUIREMENT as i32 => {
                ModuleProperties::MODULE_CPU_REQUIREMENT
            }
            x if x == ModuleProperties::MODULE_SHORTCUT as i32 => ModuleProperties::MODULE_SHORTCUT,
            x if x == ModuleProperties::MODULE_CAPABILITY as i32 => {
                ModuleProperties::MODULE_CAPABILITY
            }
            x if x == ModuleProperties::MODULE_SCORE as i32 => ModuleProperties::MODULE_SCORE,
            x if x == ModuleProperties::MODULE_CB_OPEN as i32 => ModuleProperties::MODULE_CB_OPEN,
            x if x == ModuleProperties::MODULE_CB_CLOSE as i32 => ModuleProperties::MODULE_CB_CLOSE,
            x if x == ModuleProperties::MODULE_NO_UNLOAD as i32 => {
                ModuleProperties::MODULE_NO_UNLOAD
            }
            x if x == ModuleProperties::MODULE_NAME as i32 => ModuleProperties::MODULE_NAME,
            x if x == ModuleProperties::MODULE_SHORTNAME as i32 => {
                ModuleProperties::MODULE_SHORTNAME
            }
            x if x == ModuleProperties::MODULE_DESCRIPTION as i32 => {
                ModuleProperties::MODULE_DESCRIPTION
            }
            x if x == ModuleProperties::MODULE_HELP as i32 => ModuleProperties::MODULE_HELP,
            x if x == ModuleProperties::MODULE_TEXTDOMAIN as i32 => {
                ModuleProperties::MODULE_TEXTDOMAIN
            }
            x if x == ModuleProperties::MODULE_HELP_HTML as i32 => {
                ModuleProperties::MODULE_HELP_HTML
            }

            x if x == ModuleProperties::CONFIG_NAME as i32 => ModuleProperties::CONFIG_NAME,
            x if x == ModuleProperties::CONFIG_VALUE as i32 => ModuleProperties::CONFIG_VALUE,
            x if x == ModuleProperties::CONFIG_RANGE as i32 => ModuleProperties::CONFIG_RANGE,
            x if x == ModuleProperties::CONFIG_ADVANCED_RESERVED as i32 => {
                ModuleProperties::CONFIG_ADVANCED_RESERVED
            }
            x if x == ModuleProperties::CONFIG_VOLATILE as i32 => ModuleProperties::CONFIG_VOLATILE,
            x if x == ModuleProperties::CONFIG_PERSISTENT_OBSOLETE as i32 => {
                ModuleProperties::CONFIG_PERSISTENT_OBSOLETE
            }
            x if x == ModuleProperties::CONFIG_PRIVATE as i32 => ModuleProperties::CONFIG_PRIVATE,
            x if x == ModuleProperties::CONFIG_REMOVED as i32 => ModuleProperties::CONFIG_REMOVED,
            x if x == ModuleProperties::CONFIG_CAPABILITY as i32 => {
                ModuleProperties::CONFIG_CAPABILITY
            }
            x if x == ModuleProperties::CONFIG_SHORTCUT as i32 => ModuleProperties::CONFIG_SHORTCUT,
            x if x == ModuleProperties::CONFIG_OLDNAME_OBSOLETE as i32 => {
                ModuleProperties::CONFIG_OLDNAME_OBSOLETE
            }
            x if x == ModuleProperties::CONFIG_SAFE as i32 => ModuleProperties::CONFIG_SAFE,
            x if x == ModuleProperties::CONFIG_DESC as i32 => ModuleProperties::CONFIG_DESC,
            x if x == ModuleProperties::CONFIG_LIST_OBSOLETE as i32 => {
                ModuleProperties::CONFIG_LIST_OBSOLETE
            }
            x if x == ModuleProperties::CONFIG_ADD_ACTION_OBSOLETE as i32 => {
                ModuleProperties::CONFIG_ADD_ACTION_OBSOLETE
            }
            x if x == ModuleProperties::CONFIG_LIST as i32 => ModuleProperties::CONFIG_LIST,
            x if x == ModuleProperties::CONFIG_LIST_CB_OBSOLETE as i32 => {
                ModuleProperties::CONFIG_LIST_CB_OBSOLETE
            }
            _ => return Err(()),
        };
        Ok(prop)
    }
}

use std::ffi::c_int;
#[allow(non_camel_case_types)]
pub type vlc_activate = unsafe extern "C" fn(*mut Object) -> c_int;
#[allow(non_camel_case_types)]
pub type vlc_deactivate = unsafe extern "C" fn(*mut Object);

///
/// Exposes the activation and deactivation functions for modules.
///
/// Capabilities for modules are traits implementing a Loader type
/// implementing the ModuleProtocol trait to provide the activation
/// and deactivation functions for the module.
///
/// * `T`: Type implementing the final capability trait
/// * `Activate`: Type for the activation function
/// * `Deactivate`: Type for the deactivation function
///
/// ```no_run
/// use vlcrs_core::plugin::ModuleProtocol;
///
/// /* New trait bringing support for new capabilities in modules. */
///
/// type ActivateFunction = unsafe extern "C" fn();
/// type DeactivateFunction = unsafe extern "C" fn();
/// trait NewCapabilityTrait {
///     /* Mandatory type definition for capability traits */
///
///     /* Example capability interface, can be anything and can add
///      * as many function as required. */
///     fn open(&self);
/// }
///
/// /* Implement a function to load modules implementing NewCapabilityTrait. */
/// unsafe extern "C" fn activate_new_capability<ModuleType: NewCapabilityTrait>() {
///     /* Do anything needed to initialize the module, for instance initializing
///      * a ModuleType object and call the open() function on it. */
/// }
///
/// /* Implement a module loader for this new capability trait. */
/// struct NewCapabilityLoader;
/// impl<ModuleType> ModuleProtocol<ModuleType>
/// for NewCapabilityLoader
/// where ModuleType : NewCapabilityTrait {
///     type Activate = ActivateFunction;
///     type Deactivate = DeactivateFunction;
///
///     fn activate_function() -> ActivateFunction { activate_new_capability::<ModuleType> }
///     fn deactivate_function() -> Option<DeactivateFunction> { None }
/// }
/// ```
pub trait ModuleProtocol<ModuleType: ?Sized> {
    type Activate;
    type Deactivate = *mut ();
    fn activate_function() -> Self::Activate;
    fn deactivate_function() -> Option<Self::Deactivate> {
        None
    }
}
