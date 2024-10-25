use crate::object::Object;
use std::marker::PhantomData;
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
pub(super) union ObjectInternalData<'parent> {
    pub internals: Option<NonNull<ObjectInternals>>,
    pub marker: Option<NonNull<ObjectMarker>>,
    pub _parent: PhantomData<&'parent Object<'parent>>,
}

extern "C" {

    ///
    /// Create a VLC object, with an optional parent.
    ///
    /// For now, the lifetime is more constrained than necessary so that
    /// the function can be used in test without creating unsound situations.
    ///
    pub(crate) fn vlc_object_create<'parent, 'child>(
        parent: Option<&'_ Object<'parent>>,
        length: core::ffi::c_size_t,
    ) -> Option<NonNull<Object<'child>>>
    where
        'parent: 'child;

    pub(crate) fn vlc_object_delete(obj: &mut Object);
}
