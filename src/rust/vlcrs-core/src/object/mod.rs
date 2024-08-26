mod sys;

use sys::ObjectInternalData;
use vlcrs_messages::Logger;

#[repr(C)]
pub struct Object {
    logger: Option<Logger>,
    internal_data: ObjectInternalData,
    no_interact: bool,
    force: bool,
}

impl Object {

    pub fn logger(&self) -> Option<&Logger> {
        self.logger.as_ref()
    } 
}
