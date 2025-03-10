// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Alaric Senat <alaric@videolabs.io>
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

/// Unchecked conversions on internal VLC values.
///
/// VLC core APIs often have implicit constraints or internal check on exposed data that allow
/// conversions without the usual safety checks.
///
/// Implementers of this trait must ensure that the conversion is safe within the context of
/// VLC, and new implementations must be thoroughly reviewed.
///
/// # Safety
///
/// Calling `assume_valid` is **always unsafe**, as it bypasses Rust's usual validity checks.
/// The caller must ensure that the input is indeed valid and checked in some way beforehand by VLC.
///
/// # Examples
///
/// [`AssumeValid`]`<&`[`str`](std::str)`>` is implemented for [`CStr`](std::ffi::CStr) as the internals of
/// VLC manipulate already checked UTF8 strings. This lift the bindings from the obligation to
/// perform UTF8 validity verifications:
///
/// ```
/// extern "C" fn tracer_trace(trace: *const std::ffi::c_char) {
///     // `trace` is from VLC and is assumed to be valid UTF8.
///     let trace: &str = unsafe { std::ffi::CStr::from_ptr(trace).assume_valid() };
/// }
/// ```
pub trait AssumeValid<T> {
    /// Converts `self` into `T`, without safety checks.
    unsafe fn assume_valid(self) -> T;
}

impl<'a> AssumeValid<&'a str> for &'a std::ffi::CStr {
    /// Convert a [`CStr`](std::ffi::CStr) into an `&`[`str`](std::str).
    ///
    /// # Safety
    ///
    /// Should only be used with VLC strings which are already UTF8 validated. Otherwise, the
    /// string will be left unchecked and will lead to undefined behaviors.
    ///
    /// # Panics
    ///
    /// In **debug builds only**, this function will perform the UTF8 checks and panic on failure to
    /// highlight and help resolve VLC core missing checks.
    unsafe fn assume_valid(self) -> &'a str {
        if cfg!(debug_assertions) {
            str::from_utf8(self.to_bytes()).expect("Unexpected invalid UTF8 coming from VLC")
        } else {
            unsafe { str::from_utf8_unchecked(self.to_bytes()) }
        }
    }
}
