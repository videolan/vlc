//! Messages facilities.

use std::{ffi::CStr, ptr::NonNull};

mod sys;
use sys::{vlc_Log, vlc_logger};

pub use sys::vlc_log_type as LogType;

/// Logger object to be used with the warn!, error!, log! macros
#[repr(transparent)]
pub struct Logger(pub(crate) NonNull<vlc_logger>);

impl Logger {
    /// Log message to the logger
    pub fn log(
        &self,
        priority: LogType,
        logger: &CStr,
        file: &CStr,
        func: &CStr,
        line: u32,
        msg: &str,
    ) {
        const GENERIC: &CStr = unsafe { CStr::from_bytes_with_nul_unchecked(b"generic\0") };
        const PRINTF_S: &CStr = unsafe { CStr::from_bytes_with_nul_unchecked(b"%.*s\0") };

        // SAFETY: All the pointers are non-null and points to initialized structs.
        unsafe {
            vlc_Log(
                &self.0.as_ptr(),
                priority as i32,
                GENERIC.as_ptr(),
                logger.as_ptr(),
                file.as_ptr(),
                line,
                func.as_ptr(),
                // We do this to avoid manipulating the original formatted string to be printf
                // escaped. Using %s is just way simpler.
                PRINTF_S.as_ptr(),
                msg.len(),
                msg.as_ptr(),
            )
        }
    }
}

/// Log for a VLC Object
#[macro_export]
macro_rules! log {
    ($logger:expr, $level:expr, $format:expr, $($args:expr),*) => {{
        // SAFETY: The file name cannot be nul and doens't contains nul byte.
        // With the concat of the nul byte the slice is now a C-style array.
        let file = unsafe { ::std::ffi::CStr::from_bytes_with_nul_unchecked(concat!(file!(), "\0").as_bytes()) };
        let func = ::std::ffi::CString::new(::vlcrs_utils::func!())
            .expect("should always be valid utf-8");
        let logger = ::std::ffi::CString::new(option_env!("VLC_logger_NAME").unwrap_or("<unknown rust logger>"))
            .expect("unable to create the rust module name");
        let formatted = ::std::fmt::format(format_args!("{}\0", format_args!($format, $($args),*)));
        ::vlcrs_messages::Logger::log(
            $logger,
            $level,
            logger.as_c_str(),
            file,
            func.as_c_str(),
            line!(),
            &formatted
        )
    }};
}

/// Debug-level log for a VLC Object
///
/// ```ignore
/// // let logger = ...;
/// vlcrs_messages::debug!(logger, "test");
/// ```
#[macro_export]
#[doc(alias = "msg_Dbg")]
macro_rules! debug {
    ($logger:expr, $format:expr $(,)?) => {{
        $crate::log!($logger, $crate::LogType::VLC_MSG_DBG, $format,)
    }};
    ($logger:expr, $format:expr, $($args:tt)*) => {{
        $crate::log!($logger, $crate::LogType::VLC_MSG_DBG, $format, $($args)*)
    }};
}

/// Info-level log for a VLC Object
///
/// ```ignore
/// // let logger = ...;
/// vlcrs_messages::info!(logger, "test");
/// ```
#[macro_export]
#[doc(alias = "msg_Err")]
macro_rules! info {
    ($logger:expr, $format:expr $(,)?) => {{
        $crate::log!($logger, $crate::LogType::VLC_MSG_INFO, $format,)
    }};
    ($logger:expr, $format:expr, $($args:tt)*) => {{
        $crate::log!($logger, $crate::LogType::VLC_MSG_INFO, $format, $($args)*)
    }};
}

/// Warning-level log for a VLC Object
///
/// ```ignore
/// // let logger = ...;
/// vlcrs_messages::warn!(logger, "test");
/// ```
#[macro_export]
#[doc(alias = "msg_Warn")]
macro_rules! warn {
    ($logger:expr, $format:expr $(,)?) => {{
        $crate::log!($logger, $crate::LogType::VLC_MSG_WARN, $format,)
    }};
    ($logger:expr, $format:expr, $($args:tt)*) => {{
        $crate::log!($logger, $crate::LogType::VLC_MSG_WARN, $format, $($args)*)
    }};
}

/// Error-level log for a VLC Object
///
/// ```ignore
/// // let logger = ...;
/// vlcrs_messages::error!(logger, "test");
/// ```
#[macro_export]
#[doc(alias = "msg_Err")]
macro_rules! error {
    ($logger:expr, $format:expr $(,)?) => {{
        $crate::log!($logger, $crate::LogType::VLC_MSG_ERR, $format,)
    }};
    ($logger:expr, $format:expr, $($args:tt)*) => {{
        $crate::log!($logger, $crate::LogType::VLC_MSG_ERR, $format, $($args)*)
    }};
}
