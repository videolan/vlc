use std::ffi::{c_int, c_void};

#[allow(non_camel_case_types)]
pub type vlc_set_cb = unsafe extern "C" fn(*mut c_void, *mut c_void, c_int, ...) -> c_int;
