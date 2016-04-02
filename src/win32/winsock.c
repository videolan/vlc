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
#include <vlc_network.h>

#if 0
ssize_t vlc_sendmsg (int s, struct msghdr *hdr, int flags)
{
    /* WSASendMsg would be more straightforward, and would support ancillary
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
    /* WSARecvMsg would be more straightforward, and would support ancillary
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
