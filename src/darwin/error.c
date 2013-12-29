/*****************************************************************************
 * error.c: Darwin error messages handling
 *****************************************************************************
 * Copyright © 2006-2013 Rémi Denis-Courmont
 *           © 2013 Felix Paul Kühne
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <errno.h>

#include <vlc_common.h>

typedef struct
{
    int code;
    const char *msg;
} darwinerrmsg_t;

static const darwinerrmsg_t darwinerrmsg[] =
{
    { EPERM, "Operation not permitted" },
    { ENOENT, "No such file or directory" },
    { ESRCH, "No such process" },
    { EINTR, "Interrupted system call" },
    { EIO, "Input/output error" },
    { ENXIO, "Device not configured" },
    { E2BIG, "Argument list too long" },
    { ENOEXEC, "Exec format error" },
    { EBADF, "Bad file descriptor" },
    { ECHILD, "No child processes" },
    { EDEADLK, "Resource deadlock avoided" },
    { ENOMEM, "Cannot allocate memory" },
    { EACCES, "Permission denied" },
    { EFAULT, "Bad address" },
    { ENOTBLK, "Block device required" },
    { EBUSY, "Device / Resource busy" },
    { EEXIST, "File exists" },
    { EXDEV, "Cross-device link" },
    { ENODEV, "Operation not supported by device" },
    { ENOTDIR, "Not a directory" },
    { EISDIR, "Is a directory" },
    { EINVAL, "Invalid argument" },
    { ENFILE, "Too many open files in system" },
    { EMFILE, "Too many open files" },
    { ENOTTY, "Inappropriate ioctl for device" },
    { ETXTBSY, "Text file busy" },
    { EFBIG, "File too large" },
    { ENOSPC, "No space left on device" },
    { ESPIPE, "Illegal seek" },
    { EROFS, "Read-only file system" },
    { EMLINK, "Too many links" },
    { EPIPE, "Broken pipe" },
    { EDOM, "Numerical argument out of domain" },
    { ERANGE, "Result too large" },
    { EAGAIN, "Resource temporarily unavailable" },
    { EWOULDBLOCK, "Operation would block" },
    { EINPROGRESS, "Operation now in progress" },
    { EALREADY, "Operation already in progress" },
    { ENOTSOCK, "Socket operation on non-socket" },
    { EDESTADDRREQ, "Destination address required" },
    { EMSGSIZE, "Message too long" },
    { EPROTOTYPE, "Protocol wrong type for socket" },
    { ENOPROTOOPT, "Protocol not available" },
    { EPROTONOSUPPORT, "Protocol not supported" },
    { ESOCKTNOSUPPORT, "Socket type not supported" },
    { ENOTSUP, "Operation not supported" },
    { EOPNOTSUPP, "Operation not supported on socket" },
    { EPFNOSUPPORT, "Protocol family not supported" },
    { EAFNOSUPPORT, "Address family not supported by protocol family" },
    { EADDRINUSE, "Address already in use" },
    { EADDRNOTAVAIL, "Can't assign requested address" },
    { ENETDOWN, "Network is down" },
    { ENETUNREACH, "Network is unreachable" },
    { ENETRESET, "Network dropped connection on reset" },
    { ECONNABORTED, "Software caused connection abort" },
    { ECONNRESET, "Connection reset by peer" },
    { ENOBUFS, "No buffer space available" },
    { EISCONN, "Socket is already connected" },
    { ENOTCONN, "Socket is not connected" },
    { ESHUTDOWN, "Can't send after socket shutdown" },
    { ETOOMANYREFS, "Too many references: can't splice" },
    { ETIMEDOUT, "Operation timed out" },
    { ECONNREFUSED, "Connection refused" },
    { ELOOP, "Too many levels of symbolic links" },
    { ENAMETOOLONG, "File name too long" },
    { EHOSTDOWN, "Host is down" },
    { EHOSTUNREACH, "No route to host" },
    { ENOTEMPTY, "Directory not empty" },
    { EPROCLIM, "Too many processes" },
    { EUSERS, "Too many users" },
    { EDQUOT, "Disc quota exceeded" },
    { ESTALE, "Stale NFS file handle" },
    { EREMOTE, "Too many levels of remote in path" },
    { EBADRPC, "RPC struct is bad" },
    { ERPCMISMATCH, "RPC version wrong" },
    { EPROGUNAVAIL, "RPC prog. not avail" },
    { EPROGMISMATCH, "Program version wrong" },
    { EPROCUNAVAIL, "Bad procedure for program" },
    { ENOLCK, "No locks available" },
    { ENOSYS, "Function not implemented" },
    { EFTYPE, "Inappropriate file type or format" },
    { EAUTH, "Authentication error" },
    { ENEEDAUTH, "Need authenticator" },
    { EPWROFF, "Device power is off" },
    { EDEVERR, "Device error, e.g. paper out" },
    { EOVERFLOW, "Value too large to be stored in data type" },
    { EBADEXEC, "Bad executable" },
    { EBADARCH, "Bad CPU type in executable" },
    { ESHLIBVERS, "Shared library version mismatch" },
    { EBADMACHO, "Malformed Macho file" },
    { ECANCELED, "Operation canceled" },
    { EIDRM, "Identifier removed" },
    { ENOMSG, "No message of desired type" },
    { EILSEQ, "Illegal byte sequence" },
    { ENOATTR, "Attribute not found" },
    { EBADMSG, "Bad message" },
    { EMULTIHOP, "Reserved" },
    { ENODATA, "No message available on STREAM" },
    { ENOLINK, "Reserved" },
    { ENOSR, "No STREAM resources" },
    { ENOSTR, "Not a STREAM" },
    { EPROTO, "Protocol error" },
    { ETIME, "STREAM ioctl timeout" },
    { ENOPOLICY, "No such policy registered" },
    { ENOTRECOVERABLE, "State not recoverable" },
    { EOWNERDEAD, "Previous owner died" },
    { EQFULL, "Interface output queue is full" },
    { 0, NULL }
};

const char *vlc_strerror_c(int errnum)
{
    /* C run-time errors */
    if ((unsigned)errnum < (unsigned)sys_nerr)
        return sys_errlist[errnum];

    /* Darwin socket errors */
    for (const darwinerrmsg_t *e = darwinerrmsg; e->msg != NULL; e++)
        if (e->code == errnum)
            return e->msg;

    return "Unknown error";
}

const char *vlc_strerror(int errnum)
{
    return /*vlc_gettext*/(vlc_strerror_c(errnum));
}
