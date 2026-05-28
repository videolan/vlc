/*****************************************************************************
 * gai_strerror.c: gai_strerror() replacement function
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * Copyright (C) 2002-2007 Rémi Denis-Courmont
 * Copyright (C) 2011-2015 KO Myung-Hun
 *
 * Authors: KO Myung-Hun <komh@chollian.net>
 *          Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

const char *gai_strerror (int);

#ifdef _WIN32
# include <winsock2.h>
# undef gai_strerror /* mapped to gai_strerrorA or gai_strerrorW */

#ifdef _MSC_VER
/* EAI_NODATA was removed in RFC 3493
 * The Windows SDK defines EAI_NODATA as EAI_NONAME, aka WSAHOST_NOT_FOUND.
 * It used to be WSANO_DATA. Remap it locally so we have strings for all the
 * WSA errors that getaddrinfo() can return.
 *
 * In mingw-w64 EAI_NODATA is still mapped to WSANO_DATA.
 */
# undef EAI_NODATA
# define EAI_NODATA  WSANO_DATA
#endif

#endif /* _WIN32 */

/* GAI error codes. See include/vlc_network.h. */
#ifndef EAI_BADFLAGS
# define EAI_BADFLAGS -1
#endif
#ifndef EAI_NONAME
# define EAI_NONAME -2
#endif
#ifndef EAI_AGAIN
# define EAI_AGAIN -3
#endif
#ifndef EAI_FAIL
# define EAI_FAIL -4
#endif
#ifndef EAI_NODATA
# define EAI_NODATA -5
#endif
#ifndef EAI_FAMILY
# define EAI_FAMILY -6
#endif
#ifndef EAI_SOCKTYPE
# define EAI_SOCKTYPE -7
#endif
#ifndef EAI_SERVICE
# define EAI_SERVICE -8
#endif
#ifndef EAI_ADDRFAMILY
# define EAI_ADDRFAMILY -9
#endif
#ifndef EAI_MEMORY
# define EAI_MEMORY -10
#endif
#ifndef EAI_OVERFLOW
# define EAI_OVERFLOW -11
#endif
#ifndef EAI_SYSTEM
# define EAI_SYSTEM -12
#endif

static const struct
{
    int        code;
    const char msg[41];
} gai_errlist[] =
{
    { 0,              "Error 0" },
    { EAI_BADFLAGS,   "Invalid flag used" },
    { EAI_NONAME,     "Host or service not found" },
    { EAI_AGAIN,      "Temporary name service failure" },
    { EAI_FAIL,       "Non-recoverable name service failure" },
    { EAI_NODATA,     "No data for host name" },
    { EAI_FAMILY,     "Unsupported address family" },
    { EAI_SOCKTYPE,   "Unsupported socket type" },
    { EAI_SERVICE,    "Incompatible service for socket type" },
    { EAI_ADDRFAMILY, "Unavailable address family for host name" },
    { EAI_MEMORY,     "Memory allocation failure" },
    { EAI_OVERFLOW,   "Buffer overflow" },
    { EAI_SYSTEM,     "System error" },

#ifdef _WIN32
    { WSAEFAULT,      "Invalid memory address" },
    { WSANOTINITIALISED, "WSAStartup not called or failed" },
#endif

    { 0,              "" },
};

static const char gai_unknownerr[] = "Unrecognized error number";

/****************************************************************************
 * Converts an EAI_* error code into human readable english text.
 ****************************************************************************/
const char *gai_strerror (int errnum)
{
    for (unsigned i = 0; *gai_errlist[i].msg; i++)
        if (errnum == gai_errlist[i].code)
            return gai_errlist[i].msg;

    return gai_unknownerr;
}
