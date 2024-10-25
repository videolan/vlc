mod sys;

use sys::ObjectInternalData;
use vlcrs_messages::Logger;

use std::{
    ops::{Deref, DerefMut},
    ptr::NonNull,
};

#[repr(C)]
pub struct Object<'a> {
    logger: Option<Logger>,
    internal_data: ObjectInternalData<'a>,
    no_interact: bool,
    force: bool,
}

pub struct ObjectHandle<'a> {
    obj: NonNull<Object<'a>>,
}

impl Object<'_> {
    pub fn logger(&self) -> Option<&Logger> {
        self.logger.as_ref()
    }
}

impl ObjectHandle<'_> {
    ///
    /// Create a new VLC Object as child of an existing object or from scratch.
    ///
    /// The new object cannot be used after its parent has been destroyed, so
    /// the following code is forbidden:
    ///
    /// ```compile_fail
    /// use vlcrs_core::object::ObjectHandle;
    /// let mut obj = ObjectHandle::new(None).unwrap();
    /// let obj2 = ObjectHandle::new(Some(&obj)).unwrap();
    /// drop(obj);
    /// drop(obj2);
    /// ````
    pub fn new<'parent>(parent: Option<&'parent Object>) -> Option<ObjectHandle<'parent>> {
        let obj = unsafe { sys::vlc_object_create(parent, size_of::<Object>())? };
        Some(ObjectHandle { obj })
    }
}

impl<'a> Deref for ObjectHandle<'a> {
    type Target = Object<'a>;

    fn deref(&self) -> &Self::Target {
        return unsafe { self.obj.as_ref() };
    }
}

impl<'a> DerefMut for ObjectHandle<'a> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        return unsafe { self.obj.as_mut() };
    }
}

impl Drop for ObjectHandle<'_> {
    fn drop(&mut self) {
        unsafe { sys::vlc_object_delete(self.obj.as_mut()) };
    }
}

#[cfg(test)]
mod test {
    use crate::object::ObjectHandle;

    #[test]
    fn test_create_and_destroy_object() {
        let mut obj = ObjectHandle::new(None).unwrap();
        let obj2 = ObjectHandle::new(Some(&mut obj)).unwrap();
        drop(obj2);
        drop(obj);
    }
}
