/*****************************************************************************
 * winsock.c: POSIX replacements for Winsock
 *****************************************************************************
 * Copyright © 2006-2008 Rémi Denis-Courmont
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

#include <vlc_common.h>
#include <errno.h>
#include <vlc_network.h>

#ifndef WSA_QOS_EUNKNOWNPSOBJ
# define WSA_QOS_EUNKNOWNPSOBJ 11024L
#endif

typedef struct
{
    int code;
    const char *msg;
} wsaerrmsg_t;

static const wsaerrmsg_t wsaerrmsg[] =
{
    { WSA_INVALID_HANDLE, "Specified event object handle is invalid" },
    { WSA_NOT_ENOUGH_MEMORY, "Insufficient memory available" },
    { WSA_INVALID_PARAMETER, "One or more parameters are invalid" },
    { WSA_OPERATION_ABORTED, "Overlapped operation aborted" },
    { WSA_IO_INCOMPLETE, "Overlapped I/O event object not in signaled state" },
    { WSA_IO_PENDING, "Overlapped operations will complete later" },
    { WSAEINTR, "Interrupted function call" },
    { WSAEBADF, "File handle is not valid" },
    { WSAEACCES, "Access denied" },
    { WSAEFAULT, "Invalid memory address" },
    { WSAEINVAL, "Invalid argument" },
    { WSAEMFILE, "Too many open sockets" },
    { WSAEWOULDBLOCK, "Resource temporarily unavailable" },
    { WSAEINPROGRESS, "Operation now in progress" },
    { WSAEALREADY, "Operation already in progress" },
    { WSAENOTSOCK, "Non-socket handle specified" },
    { WSAEDESTADDRREQ, "Missing destination address" },
    { WSAEMSGSIZE, "Message too long" },
    { WSAEPROTOTYPE, "Protocol wrong type for socket", },
    { WSAENOPROTOOPT, "Option not supported by protocol" },
    { WSAEPROTONOSUPPORT, "Protocol not supported" },
    { WSAESOCKTNOSUPPORT, "Socket type not supported" },
    { WSAEOPNOTSUPP, "Operation not supported" },
    { WSAEPFNOSUPPORT, "Protocol family not supported" },
    { WSAEAFNOSUPPORT, "Address family not supported by protocol family" },
    { WSAEADDRINUSE, "Address already in use" },
    { WSAEADDRNOTAVAIL, "Cannot assign requested address" },
    { WSAENETDOWN, "Network is down" },
    { WSAENETUNREACH, "Network unreachable" },
    { WSAENETRESET, "Network dropped connection on reset" },
    { WSAECONNABORTED, "Software caused connection abort" },
    { WSAECONNRESET, "Connection reset by peer" },
    { WSAENOBUFS, "No buffer space available (not enough memory)" },
    { WSAEISCONN, "Socket is already connected" },
    { WSAENOTCONN, "Socket is not connected" },
    { WSAESHUTDOWN, "Cannot send after socket shutdown" },
    { WSAETOOMANYREFS, "Too many references" },
    { WSAETIMEDOUT, "Connection timed out" },
    { WSAECONNREFUSED, "Connection refused by peer" },
    { WSAELOOP, "Cannot translate name" },
    { WSAENAMETOOLONG, "Name too long" },
    { WSAEHOSTDOWN, "Remote host is down" },
    { WSAEHOSTUNREACH, "No route to host (unreachable)" },
    { WSAENOTEMPTY, "Directory not empty" },
    { WSAEPROCLIM, "Too many processes" },
    { WSAEUSERS, "User quota exceeded" },
    { WSAEDQUOT, "Disk quota exceeded" },
    { WSAESTALE, "Stale file handle reference" },
    { WSAEREMOTE, "Item is remote", },
    { WSASYSNOTREADY, "Network subsystem is unavailable (network stack not ready)" },
    { WSAVERNOTSUPPORTED, "Winsock.dll version out of range (network stack version not supported" },
    { WSANOTINITIALISED, "Network not initialized" },
    { WSAEDISCON, "Graceful shutdown in progress" },
    { WSAENOMORE, "No more results" },
    { WSAECANCELLED, "Call has been cancelled" },
    { WSAEINVALIDPROCTABLE, "Procedure call table is invalid" },
    { WSAEINVALIDPROVIDER, "Service provider is invalid" },
    { WSAEPROVIDERFAILEDINIT, "Service provider failed to initialize" },
    { WSASYSCALLFAILURE, "System call failure" },
    { WSASERVICE_NOT_FOUND, "Service not found" },
    { WSATYPE_NOT_FOUND, "Class type not found" },
    { WSA_E_NO_MORE, "No more results" },
    { WSA_E_CANCELLED, "Call was cancelled" },
    { WSAEREFUSED, "Database query was refused" },
    { WSAHOST_NOT_FOUND, "Host not found" },
    { WSATRY_AGAIN, "Nonauthoritative host not found (temporary hostname error)" },
    { WSANO_RECOVERY, "Non-recoverable hostname error" },
    { WSANO_DATA, "Valid name, no data record of requested type" },
    { WSA_QOS_RECEIVERS, "QOS receivers" },
    { WSA_QOS_SENDERS, "QOS senders" },
    { WSA_QOS_NO_SENDERS, "No QOS senders" },
    { WSA_QOS_NO_RECEIVERS, "QOS no receivers" },
    { WSA_QOS_REQUEST_CONFIRMED, "QOS request confirmed" },
    { WSA_QOS_ADMISSION_FAILURE, "QOS admission error" },
    { WSA_QOS_POLICY_FAILURE, "QOS policy failure" },
    { WSA_QOS_BAD_STYLE, "QOS bad style" },
    { WSA_QOS_BAD_OBJECT, "QOS bad object" },
    { WSA_QOS_TRAFFIC_CTRL_ERROR, "QOS traffic control error" },
    { WSA_QOS_GENERIC_ERROR, "QOS generic error" },
    { WSA_QOS_ESERVICETYPE, "QOS service type error" },
    { WSA_QOS_EFLOWSPEC, "QOS flowspec error" },
    { WSA_QOS_EPROVSPECBUF, "Invalid QOS provider buffer" },
    { WSA_QOS_EFILTERSTYLE, "Invalid QOS filter style" },
    { WSA_QOS_EFILTERTYPE, "Invalid QOS filter type" },
    { WSA_QOS_EFILTERCOUNT, "Incorrect QOS filter count" },
    { WSA_QOS_EOBJLENGTH, "Invalid QOS object length" },
    { WSA_QOS_EFLOWCOUNT, "Incorrect QOS flow count" },
    { WSA_QOS_EUNKNOWNPSOBJ, "Unrecognized QOS object" },
    { WSA_QOS_EPOLICYOBJ, "Invalid QOS policy object" },
    { WSA_QOS_EFLOWDESC, "Invalid QOS flow descriptor" },
    { WSA_QOS_EPSFLOWSPEC, "Invalid QOS provider-specific flowspec" },
    { WSA_QOS_EPSFILTERSPEC, "Invalid QOS provider-specific filterspec" },
    { WSA_QOS_ESDMODEOBJ, "Invalid QOS shape discard mode object" },
    { WSA_QOS_ESHAPERATEOBJ, "Invalid QOS shaping rate object" },
    { WSA_QOS_RESERVED_PETYPE, "Reserved policy QOS element type" },
    { 0, NULL }
    /* Winsock2 error codes are missing, they "never" occur */
};


const char *net_strerror( int value )
{
    /* There doesn't seem to be any portable error message generation for
     * Winsock errors. Some old versions had s_error, but it appears to be
     * gone, and is not documented.
     */
    for( const wsaerrmsg_t *e = wsaerrmsg; e->msg != NULL; e++ )
        if( e->code == value )
            return e->msg;

    /* Remember to update src/misc/messages.c if you change this one */
    return "Unknown network stack error";
}

#if 0
ssize_t vlc_sendmsg (int s, struct msghdr *hdr, int flags)
{
    /* WSASendMsg would be more straightforward, and would support ancilliary
     * data, but it's not yet in mingw32. */
    if ((hdr->msg_iovlen > 100) || (hdr->msg_controllen > 0))
    {
        errno = EINVAL;
        return -1;
    }

    WSABUF buf[hdr->msg_iovlen];
    for (size_t i = 0; i < sizeof (buf) / sizeof (buf[0]); i++)
        buf[i].buf = hdr->msg_iov[i].iov_base,
        buf[i].len = hdr->msg_iov[i].iov_len;

    DWORD sent;
    if (WSASendTo (s, buf, sizeof (buf) / sizeof (buf[0]), &sent, flags,
                   hdr->msg_name, hdr->msg_namelen, NULL, NULL) == 0)
        return sent;
    return -1;
}

ssize_t vlc_recvmsg (int s, struct msghdr *hdr, int flags)
{
    /* WSARecvMsg would be more straightforward, and would support ancilliary
     * data, but it's not yet in mingw32. */
    if (hdr->msg_iovlen > 100)
    {
        errno = EINVAL;
        return -1;
    }

    WSABUF buf[hdr->msg_iovlen];
    for (size_t i = 0; i < sizeof (buf) / sizeof (buf[0]); i++)
        buf[i].buf = hdr->msg_iov[i].iov_base,
        buf[i].len = hdr->msg_iov[i].iov_len;

    DWORD recvd, dwFlags = flags;
    INT fromlen = hdr->msg_namelen;
    hdr->msg_controllen = 0;
    hdr->msg_flags = 0;

    int ret = WSARecvFrom (s, buf, sizeof (buf) / sizeof (buf[0]), &recvd,
                           &dwFlags, hdr->msg_name, &fromlen, NULL, NULL);
    hdr->msg_namelen = fromlen;
    hdr->msg_flags = dwFlags;
    if (ret == 0)
        return recvd;

#ifdef MSG_TRUNC
    if (WSAGetLastError() == WSAEMSGSIZE)
    {
        hdr->msg_flags |= MSG_TRUNC;
        return recvd;
    }
#else
# warning Out-of-date Winsock header files!
#endif
    return -1;
}
#endif
