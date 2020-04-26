/*****************************************************************************
 * datagram.c:
 *****************************************************************************
 * Copyright (C) 2020 Rémi Denis-Courmont
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

#ifndef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_network.h>
#include "vlc_dtls.h"

struct vlc_dgram_sock
{
    int fd;
    struct vlc_dtls s;
};

static void vlc_datagram_Close(struct vlc_dtls *dgs)
{
    struct vlc_dgram_sock *s = container_of(dgs, struct vlc_dgram_sock, s);

#ifndef _WIN32
    vlc_close(s->fd);
#else
    closesocket(s->fd);
#endif
    free(s);
}

/* Note: must not be a cancellation point */
static int vlc_datagram_GetPollFD(struct vlc_dtls *dgs, short *restrict ev)
{
    (void) ev; /* no changes there */
    return container_of(dgs, struct vlc_dgram_sock, s)->fd;
}

static ssize_t vlc_datagram_Recv(struct vlc_dtls *dgs, struct iovec *iov,
                                 unsigned iovlen, bool *truncated)
{
    struct msghdr msg = {
        .msg_iov = iov,
        .msg_iovlen = iovlen,
    };
    int fd = container_of(dgs, struct vlc_dgram_sock, s)->fd;
    ssize_t ret = recvmsg(fd, &msg, 0);

    if (ret >= 0)
        *truncated = (msg.msg_flags & MSG_TRUNC) != 0;

    return ret;
}

static ssize_t vlc_datagram_Send(struct vlc_dtls *dgs,
                                 const struct iovec *iov, unsigned iovlen)
{
    const struct msghdr msg = {
        .msg_iov = (struct iovec *)iov,
        .msg_iovlen = iovlen,
    };
    int fd = container_of(dgs, struct vlc_dgram_sock, s)->fd;

    return vlc_sendmsg(fd, &msg, 0);
}

static const struct vlc_dtls_operations vlc_datagram_ops = {
    vlc_datagram_Close,
    vlc_datagram_GetPollFD,
    vlc_datagram_Recv,
    vlc_datagram_Send,
};

struct vlc_dtls *vlc_datagram_CreateFD(int fd)
{
    struct vlc_dgram_sock *s = malloc(sizeof (*s));

    if (likely(s != NULL)) {
        s->fd = fd;
        s->s.ops = &vlc_datagram_ops;
    }

    return &s->s;
}
