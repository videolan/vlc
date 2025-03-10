#![deny(unsafe_op_in_unsafe_fn)]
#![feature(extern_types)]
#![feature(associated_type_defaults)]
#![feature(allocator_api)]
#![feature(c_size_t)]

//! The `vlcrs-core` crate.
//!
//! This crate contains the vlc core APIs that have been ported or
//! wrapped for usage by Rust code in the modules and is shared by all of them.
//!
//! If you need a vlc core C API that is not ported or wrapped yet here,
//! then do so first instead of bypassing this crate.

// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 Alexandre Janniaux <ajanni@videolabs.io>
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
// Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.#![deny(unsafe_op_in_unsafe_fn)]

pub mod plugin;

pub mod object;

pub mod tracer;

pub(crate) mod convert;
