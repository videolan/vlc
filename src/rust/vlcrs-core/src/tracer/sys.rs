// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024-2025 Alexandre Janniaux <ajanni@videolabs.io>
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation; either version 2.1 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.

#![allow(rustdoc::bare_urls)]
#![allow(rustdoc::broken_intra_doc_links)]
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use std::{
    ffi::{c_char, c_double, c_void},
    ptr::NonNull,
};

use crate::object::Object;

pub type vlc_tick = i64;

#[repr(C)]
#[non_exhaustive]
#[doc = "Tracer message values"]
#[derive(Debug, Copy, Clone, Hash, PartialEq, Eq)]
pub enum vlc_tracer_value_type {
    Integer = 0,
    Double = 1,
    String = 2,
    Unsigned = 3,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub union vlc_tracer_value {
    pub integer: i64,
    pub double: c_double,
    pub string: *const c_char,
    pub unsigned: u64,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct vlc_tracer_entry {
    pub key: *const c_char,
    pub value: vlc_tracer_value,
    pub kind: vlc_tracer_value_type,
}

#[repr(C)]
pub struct vlc_tracer_trace {
    pub entries: NonNull<vlc_tracer_entry>,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct vlc_tracer {
    _unused: [u8; 0],
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct vlc_tracer_operations {
    pub trace: extern "C" fn(opaque: *const c_void, ts: vlc_tick, trace: NonNull<vlc_tracer_trace>),
    pub destroy: extern "C" fn(*mut c_void),
}

extern "C" {
    pub fn vlc_tracer_Create(parent: &Object, name: *const c_char) -> Option<NonNull<vlc_tracer>>;
    pub fn vlc_tracer_TraceWithTs(
        tracer: NonNull<vlc_tracer>,
        tick: vlc_tick,
        trace: NonNull<vlc_tracer_trace>,
    );
}
