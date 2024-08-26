use std::ptr::NonNull;

#[repr(C)]
pub(super) struct ObjectInternals {
    _unused: [u8; 0],
}

#[repr(C)]
pub(super) struct ObjectMarker {
    _unused: [u8; 0],
}

#[repr(C)]
#[derive(Copy, Clone)]
pub(super) union ObjectInternalData {
    pub internals: Option<NonNull<ObjectInternals>>,
    pub marker: Option<NonNull<ObjectMarker>>,
}
