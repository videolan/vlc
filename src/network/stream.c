/*****************************************************************************
 * stream.c
 *****************************************************************************
 * Copyright © 2004-2016 Rémi Denis-Courmont
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

/**
 * @ingroup transport
 * @file
 * Transport-layer stream abstraction
 *
 * This file implements the transport-layer stream (vlc_tls) abstraction.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_POLL_H
# include <poll.h>
#endif
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif
#ifdef HAVE_NETINET_TCP_H
# include <netinet/tcp.h>
#endif
#ifndef SOL_TCP
# define SOL_TCP IPPROTO_TCP
#endif

#include <vlc_common.h>
#include <vlc_tls.h>
#include <vlc_interrupt.h>

ssize_t vlc_tls_Read(vlc_tls_t *session, void *buf, size_t len, bool waitall)
{
    struct pollfd ufd;
    struct iovec iov;

    ufd.events = POLLIN;
    ufd.fd = vlc_tls_GetPollFD(session, &ufd.events);
    iov.iov_base = buf;
    iov.iov_len = len;

    for (size_t rcvd = 0;;)
    {
        if (vlc_killed())
        {
            errno = EINTR;
            return -1;
        }

        ssize_t val = session->ops->readv(session, &iov, 1);
        if (val > 0)
        {
            if (!waitall)
                return val;
            iov.iov_base = (char *)iov.iov_base + val;
            iov.iov_len -= val;
            rcvd += val;
        }
        if (iov.iov_len == 0 || val == 0)
            return rcvd;
        if (val == -1)
        {
            if (vlc_killed())
                return -1;
            if (errno != EINTR && errno != EAGAIN)
                return rcvd ? (ssize_t)rcvd : -1;
        }

        vlc_poll_i11e(&ufd, 1, -1);
    }
}

ssize_t vlc_tls_Write(vlc_tls_t *session, const void *buf, size_t len)
{
    struct pollfd ufd;
    struct iovec iov;

    ufd.events = POLLOUT;
    ufd.fd = vlc_tls_GetPollFD(session, &ufd.events);
    iov.iov_base = (void *)buf;
    iov.iov_len = len;

    for (size_t sent = 0;;)
    {
        if (vlc_killed())
        {
            errno = EINTR;
            return -1;
        }

        ssize_t val = session->ops->writev(session, &iov, 1);
        if (val > 0)
        {
            iov.iov_base = ((char *)iov.iov_base) + val;
            iov.iov_len -= val;
            sent += val;
        }
        if (iov.iov_len == 0 || val == 0)
            return sent;
        if (val == -1)
        {
            if (vlc_killed())
                return -1;
            if (errno != EINTR && errno != EAGAIN)
                return sent ? (ssize_t)sent : -1;
        }

        vlc_poll_i11e(&ufd, 1, -1);
    }
}

char *vlc_tls_GetLine(vlc_tls_t *session)
{
    char *line = NULL;
    size_t linelen = 0, linesize = 0;

    do
    {
        if (linelen == linesize)
        {
            linesize += 1024;

            char *newline = realloc(line, linesize);
            if (unlikely(newline == NULL))
                goto error;
            line = newline;
        }

        if (vlc_tls_Read(session, line + linelen, 1, false) <= 0)
            goto error;
    }
    while (line[linelen++] != '\n');

    if (linelen >= 2 && line[linelen - 2] == '\r')
        line[linelen - 2] = '\0';
    return line;

error:
    free(line);
    return NULL;
}

typedef struct vlc_tls_socket
{
    struct vlc_tls tls;
    int fd;
    socklen_t peerlen;
    struct sockaddr peer[];
} vlc_tls_socket_t;

static int vlc_tls_SocketGetFD(vlc_tls_t *tls, short *restrict events)
{
    vlc_tls_socket_t *sock = (struct vlc_tls_socket *)tls;

    (void) events;
    return sock->fd;
}

static ssize_t vlc_tls_SocketRead(vlc_tls_t *tls, struct iovec *iov,
                                  unsigned count)
{
    vlc_tls_socket_t *sock = (struct vlc_tls_socket *)tls;
    struct msghdr msg =
    {
        .msg_iov = iov,
        .msg_iovlen = count,
    };

    return recvmsg(sock->fd, &msg, 0);
}

static ssize_t vlc_tls_SocketWrite(vlc_tls_t *tls, const struct iovec *iov,
                                   unsigned count)
{
    vlc_tls_socket_t *sock = (struct vlc_tls_socket *)tls;
    const struct msghdr msg =
    {
        .msg_iov = (struct iovec *)iov,
        .msg_iovlen = count,
    };

    return vlc_sendmsg(sock->fd, &msg, 0);
}

static int vlc_tls_SocketShutdown(vlc_tls_t *tls, bool duplex)
{
    vlc_tls_socket_t *sock = (struct vlc_tls_socket *)tls;

    return shutdown(sock->fd, duplex ? SHUT_RDWR : SHUT_WR);
}

static void vlc_tls_SocketClose(vlc_tls_t *tls)
{
    vlc_tls_socket_t *sock = (struct vlc_tls_socket *)tls;

    net_Close(sock->fd);
    free(tls);
}

static const struct vlc_tls_operations vlc_tls_socket_ops =
{
    vlc_tls_SocketGetFD,
    vlc_tls_SocketRead,
    vlc_tls_SocketWrite,
    vlc_tls_SocketShutdown,
    vlc_tls_SocketClose,
};

static vlc_tls_t *vlc_tls_SocketAlloc(int fd,
                                      const struct sockaddr *restrict peer,
                                      socklen_t peerlen)
{
    vlc_tls_socket_t *sock = malloc(sizeof (*sock) + peerlen);
    if (unlikely(sock == NULL))
        return NULL;

    vlc_tls_t *tls = &sock->tls;

    tls->ops = &vlc_tls_socket_ops;
    tls->p = NULL;

    sock->fd = fd;
    sock->peerlen = peerlen;
    if (peerlen > 0)
        memcpy(sock->peer, peer, peerlen);
    return tls;
}

vlc_tls_t *vlc_tls_SocketOpen(int fd)
{
    return vlc_tls_SocketAlloc(fd, NULL, 0);
}

int vlc_tls_SocketPair(int family, int protocol, vlc_tls_t *pair[2])
{
    int fds[2];

    if (vlc_socketpair(family, SOCK_STREAM, protocol, fds, true))
        return -1;

    for (size_t i = 0; i < 2; i++)
    {
        setsockopt(fds[i], SOL_SOCKET, SO_REUSEADDR,
                   &(int){ 1 }, sizeof (int));

        pair[i] = vlc_tls_SocketAlloc(fds[i], NULL, 0);
        if (unlikely(pair[i] == NULL))
        {
            net_Close(fds[i]);
            if (i)
                vlc_tls_SessionDelete(pair[0]);
            else
                net_Close(fds[1]);
            return -1;
        }
    }
    return 0;
}

/**
 * Allocates an unconnected transport layer socket.
 */
static vlc_tls_t *vlc_tls_SocketAddrInfo(const struct addrinfo *restrict info)
{
    int fd = vlc_socket(info->ai_family, info->ai_socktype, info->ai_protocol,
                        true /* nonblocking */);
    if (fd == -1)
        return NULL;

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof (int));

    if (info->ai_socktype == SOCK_STREAM && info->ai_protocol == IPPROTO_TCP)
        setsockopt(fd, SOL_TCP, TCP_NODELAY, &(int){ 1 }, sizeof (int));

    vlc_tls_t *sk = vlc_tls_SocketAlloc(fd, info->ai_addr, info->ai_addrlen);
    if (unlikely(sk == NULL))
        net_Close(fd);
    return sk;
}

/**
 * Waits for pending transport layer socket connection.
 */
static int vlc_tls_WaitConnect(vlc_tls_t *tls)
{
    const int fd = vlc_tls_GetFD(tls);
    struct pollfd ufd;

    ufd.fd = fd;
    ufd.events = POLLOUT;

    do
    {
        if (vlc_killed())
        {
            errno = EINTR;
            return -1;
        }
    }
    while (vlc_poll_i11e(&ufd, 1, -1) <= 0);

    int val;
    socklen_t len = sizeof (val);

    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &len))
        return -1;

    if (val != 0)
    {
        errno = val;
        return -1;
    }
    return 0;
}

/**
 * Connects a transport layer socket.
 */
static ssize_t vlc_tls_Connect(vlc_tls_t *tls)
{
    const vlc_tls_socket_t *sock = (vlc_tls_socket_t *)tls;

    if (connect(sock->fd, sock->peer, sock->peerlen) == 0)
        return 0;
#ifndef _WIN32
    if (errno != EINPROGRESS)
        return -1;
#else
    if (WSAGetLastError() != WSAEWOULDBLOCK)
        return -1;
#endif
    return vlc_tls_WaitConnect(tls);
}

/* Callback for combined connection establishment and initial send */
static ssize_t vlc_tls_ConnectWrite(vlc_tls_t *tls,
                                    const struct iovec *iov,unsigned count)
{
    /* Next time, write directly. Do not retry to connect. */
    tls->ops = &vlc_tls_socket_ops;

#ifdef MSG_FASTOPEN
    vlc_tls_socket_t *sock = (vlc_tls_socket_t *)tls;
    const struct msghdr msg =
    {
        .msg_name = sock->peer,
        .msg_namelen = sock->peerlen,
        .msg_iov = (struct iovec *)iov,
        .msg_iovlen = count,
    };
    ssize_t ret;

    ret = vlc_sendmsg(sock->fd, &msg, MSG_FASTOPEN);
    if (ret >= 0)
    {   /* Fast open in progress */
        return ret;
    }

    if (errno == EINPROGRESS)
    {
        if (vlc_tls_WaitConnect(tls))
            return -1;
    }
    else
    if (errno != EOPNOTSUPP)
        return -1;
    /* Fast open not supported or disabled... fallback to normal mode */
#endif

    if (vlc_tls_Connect(tls))
        return -1;

    return vlc_tls_SocketWrite(tls, iov, count);
}

static const struct vlc_tls_operations vlc_tls_socket_fastopen_ops =
{
    vlc_tls_SocketGetFD,
    vlc_tls_SocketRead,
    vlc_tls_ConnectWrite,
    vlc_tls_SocketShutdown,
    vlc_tls_SocketClose,
};

vlc_tls_t *vlc_tls_SocketOpenAddrInfo(const struct addrinfo *restrict info,
                                      bool defer_connect)
{
    vlc_tls_t *sock = vlc_tls_SocketAddrInfo(info);
    if (sock == NULL)
        return NULL;

    if (defer_connect)
    {   /* The socket is not connected yet.
         * The connection will be triggered on the first send. */
        sock->ops = &vlc_tls_socket_fastopen_ops;
    }
    else
    {
        if (vlc_tls_Connect(sock))
        {
            vlc_tls_SessionDelete(sock);
            sock = NULL;
        }
    }
    return sock;
}

vlc_tls_t *vlc_tls_SocketOpenTCP(vlc_object_t *obj, const char *name,
                                 unsigned port)
{
    struct addrinfo hints =
    {
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
    }, *res;

    assert(name != NULL);
    msg_Dbg(obj, "resolving %s ...", name);

    int val = vlc_getaddrinfo_i11e(name, port, &hints, &res);
    if (val != 0)
    {   /* TODO: C locale for gai_strerror() */
        msg_Err(obj, "cannot resolve %s port %u: %s", name, port,
                gai_strerror(val));
        return NULL;
    }

    msg_Dbg(obj, "connecting to %s port %u ...", name, port);

    /* TODO: implement RFC8305 */
    for (const struct addrinfo *p = res; p != NULL; p = p->ai_next)
    {
        vlc_tls_t *tls = vlc_tls_SocketOpenAddrInfo(p, false);
        if (tls == NULL)
        {
            msg_Err(obj, "connection error: %s", vlc_strerror_c(errno));
            continue;
        }

        freeaddrinfo(res);
        return tls;
    }

    freeaddrinfo(res);
    return NULL;
}
