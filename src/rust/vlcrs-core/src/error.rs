// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Alaric Senat <alaric@videolabs.io>
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

use std::{ffi, num::NonZero};

/// Error type for VLC core C API error codes.
///
/// ## VLC's C error convention
///
/// VLC core and module exported functions that can fail return a C `int` following this
/// convention:
///
/// - `0` indicates success.
/// - A **negative** value indicates failure; the value itself is the error code.
/// - A **positive** value indicates success with additional meaning defined by the documentation
///   of the function.
///
/// ### Error codes
///
/// Error codes are negated POSIX `errno` values. For example, [EINVAL](libc::EINVAL) is
/// represented as `-EINVAL`. A function may return non-`errno` negative values only when
/// explicitly documented.
///
/// The value `INT_MIN` is reserved as `VLC_EGENERIC`, a catch-all for errors that cannot be mapped
/// to any specific `errno`. It is considered legacy and should not be returned by new code.
///
/// ## Rust mapping
///
/// [`Error`] stores the raw `c_int` error code without conversion, it can safely be passed around
/// the ffi. Since VLC error codes are negated `errno` values, the type can translate them to and
/// from [`std::io::ErrorKind`], giving the Rust API a standard error classification without losing
/// the original code.
///
/// ### Example
///
/// ```
/// # use vlcrs_core::Error;
/// use std::num::NonZero;
/// use std::io::ErrorKind;
/// fn vlc_binding() -> Result<(), Error> {
///     Err(Error::from_raw(NonZero::new(-libc::ENOSYS).unwrap()))
/// }
///
/// // Matching against a VLC error:
/// match vlc_binding() {
///     Err(e) if e.kind() == ErrorKind::Unsupported => {
///         eprintln!("unsupported call")
///     }
///     _ => (),
/// }
/// ```
#[derive(Clone, Copy, Eq, Hash, PartialEq)]
pub struct Error(NonZero<ffi::c_int>);

macro_rules! error_kind_map {
    ($($(#[$attr:meta])* $kind:ident => $errno:ident $(| $alias:ident)*),+ $(,)?) => {
        fn new_impl(kind: std::io::ErrorKind) -> Self {
            match kind {
                $($(#[$attr])*
                std::io::ErrorKind::$kind => Self::from_errno(
                    // SAFETY: POSIX errno constants are non-zero.
                    unsafe { NonZero::new_unchecked(libc::$errno) }
                ),)+
                _ => Self::from_raw(Self::EGENERIC),
            }
        }

        fn kind_impl(&self) -> std::io::ErrorKind {
            let Some(errno) = self.0.get().checked_abs() else {
                return std::io::ErrorKind::Other;
            };

            #[allow(unreachable_patterns)]
            match errno {
                $($(#[$attr])*
                libc::$errno $(| libc::$alias)* => std::io::ErrorKind::$kind,)+
                _ => std::io::ErrorKind::Other,
            }
        }
    };
}

impl Error {
    // SAFETY: c_int::MIN is not zero.
    const EGENERIC: NonZero<ffi::c_int> = unsafe { NonZero::new_unchecked(ffi::c_int::MIN) };

    /// Creates an error from a standard [`std::io::ErrorKind`].
    ///
    /// The kind is mapped to the corresponding POSIX `errno` value. Kinds that have no mapping
    /// should be avoided as they produce a generic error; see [`is_generic`](Self::is_generic).
    ///
    /// # Examples
    ///
    /// ```
    /// # use vlcrs_core::Error;
    /// let err = Error::new(std::io::ErrorKind::NotFound);
    /// assert_eq!(err.kind(), std::io::ErrorKind::NotFound);
    ///
    /// // Unmapped kinds fall back to the generic error.
    /// let err = Error::new(std::io::ErrorKind::Other);
    /// assert!(err.is_generic());
    /// ```
    pub fn new(kind: std::io::ErrorKind) -> Self {
        Self::new_impl(kind)
    }

    /// Creates an error from a raw VLC error code.
    ///
    /// `raw` must be a strictly negative `c_int`. Use this when receiving error codes directly
    /// from a VLC C API call.
    ///
    /// # Examples
    ///
    /// ```
    /// # use vlcrs_core::Error;
    /// # use std::num::NonZero;
    /// let err = Error::from_raw(NonZero::new(-libc::EINVAL).unwrap());
    /// assert_eq!(err.kind(), std::io::ErrorKind::InvalidInput);
    /// ```
    pub fn from_raw(raw: NonZero<ffi::c_int>) -> Self {
        debug_assert!(raw.get().is_negative());
        Self(raw)
    }

    /// Returns the [`std::io::ErrorKind`] that best describes this error.
    ///
    /// Error codes that do not correspond to a known `errno` return [`std::io::ErrorKind::Other`],
    /// in that case consider matching the raw error code; see [`raw`](Self::raw).
    pub fn kind(&self) -> std::io::ErrorKind {
        self.kind_impl()
    }

    /// Returns the raw VLC error code.
    ///
    /// Useful to match error value not mappable to [`std::io::ErrorKind`].
    ///
    /// # Examples
    ///
    /// ```
    /// # use vlcrs_core::Error;
    /// # use std::num::NonZero;
    /// let err = Error::from_raw(NonZero::new(-libc::EBADF).unwrap());
    /// assert_eq!(err.kind(), std::io::ErrorKind::Other);
    ///
    /// match -err.raw().get() {
    ///     libc::EBADF => eprintln!("Bad file descriptor!"),
    ///     _ => (),
    /// }
    /// ```
    pub fn raw(&self) -> NonZero<ffi::c_int> {
        self.0
    }

    /// Returns `true` if this error is the legacy `VLC_EGENERIC` error.
    ///
    /// `VLC_EGENERIC` (`INT_MIN`) is used when no specific error code applies. It should not
    /// be produced by new code.
    pub fn is_generic(&self) -> bool {
        self.0 == Self::EGENERIC
    }

    fn from_errno(errno: NonZero<ffi::c_int>) -> Self {
        Self(-errno)
    }

    error_kind_map! {
        AddrInUse              => EADDRINUSE,
        AddrNotAvailable       => EADDRNOTAVAIL,
        AlreadyExists          => EEXIST,
        ArgumentListTooLong    => E2BIG,
        BrokenPipe             => EPIPE,
        ConnectionAborted      => ECONNABORTED,
        ConnectionRefused      => ECONNREFUSED,
        ConnectionReset        => ECONNRESET,
        CrossesDevices         => EXDEV,
        Deadlock               => EDEADLK,
        DirectoryNotEmpty      => ENOTEMPTY,
        ExecutableFileBusy     => ETXTBSY,
        FileTooLarge           => EFBIG,
        HostUnreachable        => EHOSTUNREACH,
        Interrupted            => EINTR,
        InvalidFilename        => ENAMETOOLONG,
        InvalidInput           => EINVAL,
        IsADirectory           => EISDIR,
        NetworkDown            => ENETDOWN,
        NetworkUnreachable     => ENETUNREACH,
        NotADirectory          => ENOTDIR,
        NotConnected           => ENOTCONN,
        NotFound               => ENOENT,
        NotSeekable            => ESPIPE,
        OutOfMemory            => ENOMEM,
        PermissionDenied       => EACCES | EPERM,
        // MinGW does not define EDQUOT [^1].
        #[cfg(not(windows))]
        QuotaExceeded          => EDQUOT,
        ReadOnlyFilesystem     => EROFS,
        ResourceBusy           => EBUSY,
        // MinGW does not define ESTALE [^1].
        #[cfg(not(windows))]
        StaleNetworkFileHandle => ESTALE,
        StorageFull            => ENOSPC,
        TimedOut               => ETIMEDOUT,
        TooManyLinks           => EMLINK,
        Unsupported            => ENOSYS | ENOTSUP | EOPNOTSUPP,
        WouldBlock             => EAGAIN | EWOULDBLOCK,
        // [^1]: See https://www.gnu.org/software/gnulib/manual/html_node/errno_002eh.html
    }
}

impl std::fmt::Debug for Error {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        use std::io::ErrorKind;
        let mut fmt = fmt.debug_struct("VLCError");
        match self.kind() {
            ErrorKind::Other if self.is_generic() => fmt.field("kind", &"Generic"),
            kind => fmt.field("kind", &kind),
        }
        .field("raw", &self.raw().get())
        .finish()
    }
}

impl std::fmt::Display for Error {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        use std::io::ErrorKind;
        match self.kind() {
            ErrorKind::Other if self.is_generic() => write!(fmt, "generic error (EGENERIC)"),
            ErrorKind::Other => write!(fmt, "unknown error ({})", self.raw()),
            kind => kind.fmt(fmt),
        }
    }
}

impl std::error::Error for Error {}

impl From<std::io::Error> for Error {
    fn from(value: std::io::Error) -> Self {
        Error::new(value.kind())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::ErrorKind;
    use std::num::NonZero;

    #[test]
    fn raw_round_trip() {
        let err = Error::from_raw(NonZero::new(-libc::EINVAL).unwrap());
        assert_eq!(err.raw().get(), -libc::EINVAL);
    }

    #[test]
    fn is_generic() {
        let err = Error::from_raw(NonZero::new(libc::c_int::MIN).unwrap());
        assert!(err.is_generic());
        let err = Error::from_raw(NonZero::new(-libc::EINVAL).unwrap());
        assert!(!err.is_generic());
    }

    #[test]
    fn kind_round_trip_timed_out() {
        let err = Error::new(ErrorKind::TimedOut);
        assert_eq!(err.kind(), ErrorKind::TimedOut);
    }

    #[test]
    fn unmapped_kind_becomes_generic() {
        let err = Error::new(ErrorKind::Other);
        assert!(err.is_generic());
    }

    #[test]
    fn unknown_errno_maps_to_other() {
        let err = Error::from_raw(NonZero::new(libc::c_int::MIN + 1).unwrap());
        assert_eq!(err.kind(), ErrorKind::Other);
        assert!(!err.is_generic());
    }

    #[test]
    fn from_io_error() {
        let io_err = std::io::Error::new(ErrorKind::NotFound, "not found");
        let err = Error::from(io_err);
        assert_eq!(err.kind(), ErrorKind::NotFound);
    }

    #[test]
    fn errno_would_block() {
        let again = Error::from_raw(NonZero::new(-libc::EAGAIN).unwrap());
        let would_block = Error::from_raw(NonZero::new(-libc::EWOULDBLOCK).unwrap());
        assert_eq!(again.kind(), ErrorKind::WouldBlock);
        assert_eq!(would_block.kind(), ErrorKind::WouldBlock);
    }

    #[test]
    fn from_io_would_block() {
        let io_err = std::io::Error::new(ErrorKind::WouldBlock, "would block");
        let err = Error::from(io_err);
        assert_eq!(err.kind(), ErrorKind::WouldBlock);
        assert_eq!(err.raw().get(), -libc::EAGAIN);
    }
}
