/*****************************************************************************
 * error.c: Network error handling
 *****************************************************************************
 * Copyright (C) 2006 Rémi Denis-Courmont
 * $Id$
 *
 * Author : Rémi Denis-Courmont <rem # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <errno.h>
#include "network.h"

#if defined (WIN32) || defined (UNDER_CE)
typedef struct
{
    int code;
    const char *msg;
} wsaerrmsg_t;

static const wsaerrmsg_t wsaerrmsg =
{
    { WSAEINTR, "Interrupted by signal" },
    { WSAEACCES, "Access denied" },
    { WSAEFAULT, "Invalid memory address" },
    { WSAEINVAL, "Invalid argument" },
    { WSAEMFILE, "Too many open sockets" },
    { WSAEWOULDBLOCK, "Would block" },
    //{ WSAEALREADY
    { WSAENOTSOCK, "Non-socket handle specified" },
    { WSAEDESTADDRREQ, "Missing destination address" },
    { WSAEMSGSIZE, "Message too big" },
    //{ WSAEPROTOTYPE
    { WSAENOPROTOOPT, "Option not supported by protocol" },
    { WSAEPROTONOSUPPORT, "Protocol not support" },
    //WSAESOCKTNOSUPPORT
    { WSAEOPNOTSUPP, "Operation not supported" },
    { WSAEPFNOSUPPORT, "Protocol family not supported" },
    { WSAEAFNOSUPPORT, "Address family not supported" },
    { WSAEADDRINUSE, "Address already in use" },
    { WSAEADDRNOTAVAIL, "Address not available" },
    { WSAENETDOWN, "Network down" },
    { WSAENETUNREACH, "Network unreachable" },
    //WSAENETRESET
    { WSAECONNABORTED, "Connection aborted" },
    { WSAECONNRESET, "Connection reset by peer" },
    { WSAENOBUFS, "Not enough memory" },
    { WSAEISCONN, "Socket already connected" },
    { WSAENOTCONN, "Connection required first" },
    { WSAESHUTDOWN, "Connection shutdown" },
    { WSAETOOMANYREFS, "Too many references" },
    { WSAETIMEDOUT, "Connection timed out" },
    { WSAECONNREFUSED, "Connection refused by peer" },
    //WSAELOOP
    //WSAENAMETOOLONG
    { WSAEHOSTDOWN, "Remote host down" },
    { WSAEHOSTUNREACH, "Remote host unreachable" },
    //WSAENOTEMPTY
    //WSAEPROCLIM
    //WSAEUSERS
    //WSAEDQUOT
    //WSAESTALE
    //WSAEREMOTE
    //WSAEDISCON
    { WSASYSNOTREADY, "Network stack not ready" },
    { WSAVERNOTSUPPORTED, "Network stack version not supported" },
    { WSANOTINITIALISED, "Network not initialized" },
    { WSAHOST_NOT_FOUND, "Hostname not found" },
    { WSATRY_AGAIN, "Temporary hostname error" },
    { WSANO_RECOVERY, "Non-recoverable hostname error" },
    /* Winsock2 and QoS error are codes missing,
       I'm too bored, and they "never" occur. */
    { 0, NULL }
};


const char *net_strerror( int value )
{
    /* There doesn't seem to be any portable error message generation for
     * Winsock errors. Some old versions had s_error, but it appears to be
     * gone, and is not documented.
     */
    for( const wsaerrmsg_t *e = wsaerrmsg; e.msg != NULL; e++ )
        if( e.code == value )
            return e.msg;

    return "Unknown network stack error";
}
#endif
