#![deny(unsafe_op_in_unsafe_fn)]
#![feature(extern_types)]
#![feature(associated_type_defaults)]
#![feature(c_size_t)]

//! The `vlcrs-core` crate.
//!
//! This crate contains the vlc core APIs that have been ported or
//! wrapped for usage by Rust code in the modules and is shared by all of them.
//!
//! If you need a vlc core C API that is not ported or wrapped yet here,
//! then do so first instead of bypassing this crate.

pub mod plugin;

pub mod object;
