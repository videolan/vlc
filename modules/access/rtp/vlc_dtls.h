/*****************************************************************************
 * datagram.h:
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

#ifndef VLC_DATAGRAM_SOCKET_H
# define VLC_DATAGRAM_SOCKET_H

struct iovec;

/**
 * Datagram socket
 */
struct vlc_dtls {
    const struct vlc_dtls_operations *ops;
};

struct vlc_dtls_operations {
    void (*close)(struct vlc_dtls *);

    int (*get_fd)(struct vlc_dtls *, short *events);
    ssize_t (*readv)(struct vlc_dtls *, struct iovec *iov, unsigned len,
                     bool *restrict truncated);
    ssize_t (*writev)(struct vlc_dtls *, const struct iovec *iov, unsigned len);
};

static inline void vlc_dtls_Close(struct vlc_dtls *dgs)
{
    dgs->ops->close(dgs);
}

static inline int vlc_dtls_GetPollFD(struct vlc_dtls *dgs, short *restrict ev)
{
    return dgs->ops->get_fd(dgs, ev);
}

static inline ssize_t vlc_dtls_Recv(struct vlc_dtls *dgs, void *buf, size_t len,
                                   bool *restrict truncated)
{
    struct iovec iov = { .iov_base = buf, .iov_len = len };

    return dgs->ops->readv(dgs, &iov, 1, truncated);
}

static inline ssize_t vlc_dtls_Send(struct vlc_dtls *dgs, const void *buf,
                                   size_t len)
{
    struct iovec iov = { .iov_base = (void *)buf, .iov_len = len };

    return dgs->ops->writev(dgs, &iov, 1);
}

struct vlc_dtls *vlc_datagram_CreateFD(int fd);
struct vlc_dtls *vlc_dccp_CreateFD(int fd);

#endif
