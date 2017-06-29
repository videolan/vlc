/*****************************************************************************
 * recvmsg.c: POSIX recvmsg() replacement
 *****************************************************************************
 * Copyright © 2017 VLC authors and VideoLAN
 * Copyright © 2016 Rémi Denis-Courmont
 *
 * Authors: Rémi Denis-Courmont
 *          Dennis Hamester <dhamester@jusst.de>
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
# include <config.h>
#endif

#ifdef _WIN32
# include <errno.h>
# include <stdlib.h>
# include <winsock2.h>

ssize_t recvmsg(int fd, struct msghdr *msg, int flags)
{
    if (msg->msg_controllen != 0)
    {
        errno = ENOSYS;
        return -1;
    }

    if (msg->msg_iovlen > IOV_MAX)
    {
        errno = EINVAL;
        return -1;
    }

    WSABUF *buf = malloc(msg->msg_iovlen * sizeof (*buf));
    if (buf == NULL)
        return -1;

    for (unsigned i = 0; i < msg->msg_iovlen; i++)
    {
        buf[i].len = msg->msg_iov[i].iov_len;
        buf[i].buf = msg->msg_iov[i].iov_base;
    }

    DWORD dwFlags = flags;
    INT fromlen = msg->msg_namelen;
    DWORD rcvd;
    int ret;
    if (fromlen)
        ret = WSARecvFrom(fd, buf, msg->msg_iovlen, &rcvd, &dwFlags,
                          msg->msg_name, &fromlen, NULL, NULL);
    else
        ret = WSARecv(fd, buf, msg->msg_iovlen, &rcvd, &dwFlags,
                      NULL, NULL);
    free(buf);

    if (ret == 0)
    {
        msg->msg_namelen = fromlen;
        msg->msg_flags = dwFlags;
        return rcvd;
    }

    switch (WSAGetLastError())
    {
        case WSAEWOULDBLOCK:
            errno = EAGAIN;
            break;
    }
    return -1;
}

#elif defined __native_client__
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>

ssize_t recvmsg(int fd, struct msghdr *msg, int flags)
{
    if (msg->msg_controllen != 0)
    {
        errno = ENOSYS;
        return -1;
    }

    if ((msg->msg_iovlen <= 0) || (msg->msg_iovlen > IOV_MAX))
    {
        errno = EMSGSIZE;
        return -1;
    }

    size_t full_size = 0;
    for (int i = 0; i < msg->msg_iovlen; ++i)
        full_size += msg->msg_iov[i].iov_len;

    if (full_size > SSIZE_MAX) {
        errno = EINVAL;
        return -1;
    }

    /**
     * We always allocate here, because whether recv/recvfrom allow NULL message
     * or not is unspecified.
     */
    char *data = malloc(full_size ? full_size : 1);
    if (!data) {
        errno = ENOMEM;
        return -1;
    }

    ssize_t res;
    if (msg->msg_name)
        res = recvfrom(fd, data, full_size, flags, msg->msg_name, &msg->msg_namelen);
    else
        res = recv(fd, data, full_size, flags);

    if (res > 0) {
        size_t left;
        if ((size_t)res <= full_size) {
            left = res;
            msg->msg_flags = 0;
        }
        else {
            left = full_size;
            msg->msg_flags = MSG_TRUNC;
        }

        const char *src = data;
        for (int i = 0; (i < msg->msg_iovlen) && (left > 0); ++i)
        {
            size_t to_copy = msg->msg_iov[i].iov_len;
            if (to_copy > left)
                to_copy = left;

            memcpy(msg->msg_iov[i].iov_base, src, to_copy);
            src += to_copy;
            left -= to_copy;
        }
    }

    free(data);
    return res;
}

#else
#error recvmsg not implemented on your platform!
#endif
