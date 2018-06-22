/*****************************************************************************
 * tls.c
 *****************************************************************************
 * Copyright © 2004-2016 Rémi Denis-Courmont
 * $Id$
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
 * @file
 * Transport Layer Socket abstraction.
 *
 * This file implements the Transport Layer Socket (vlc_tls) abstraction.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_POLL
# include <poll.h>
#endif
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "libvlc.h"

#include <vlc_tls.h>
#include <vlc_modules.h>
#include <vlc_interrupt.h>

/*** TLS credentials ***/

static int tls_server_load(void *func, va_list ap)
{
    int (*activate) (vlc_tls_creds_t *, const char *, const char *) = func;
    vlc_tls_creds_t *crd = va_arg (ap, vlc_tls_creds_t *);
    const char *cert = va_arg (ap, const char *);
    const char *key = va_arg (ap, const char *);

    return activate (crd, cert, key);
}

static int tls_client_load(void *func, va_list ap)
{
    int (*activate) (vlc_tls_creds_t *) = func;
    vlc_tls_creds_t *crd = va_arg (ap, vlc_tls_creds_t *);

    return activate (crd);
}

static void tls_unload(void *func, va_list ap)
{
    void (*deactivate) (vlc_tls_creds_t *) = func;
    vlc_tls_creds_t *crd = va_arg (ap, vlc_tls_creds_t *);

    deactivate (crd);
}

vlc_tls_creds_t *
vlc_tls_ServerCreate (vlc_object_t *obj, const char *cert_path,
                      const char *key_path)
{
    vlc_tls_creds_t *srv = vlc_custom_create (obj, sizeof (*srv),
                                              "tls server");
    if (unlikely(srv == NULL))
        return NULL;

    if (key_path == NULL)
        key_path = cert_path;

    srv->module = vlc_module_load (srv, "tls server", NULL, false,
                                   tls_server_load, srv, cert_path, key_path);
    if (srv->module == NULL)
    {
        msg_Err (srv, "TLS server plugin not available");
        vlc_object_release (srv);
        return NULL;
    }

    return srv;
}

vlc_tls_creds_t *vlc_tls_ClientCreate (vlc_object_t *obj)
{
    vlc_tls_creds_t *crd = vlc_custom_create (obj, sizeof (*crd),
                                              "tls client");
    if (unlikely(crd == NULL))
        return NULL;

    crd->module = vlc_module_load (crd, "tls client", NULL, false,
                                   tls_client_load, crd);
    if (crd->module == NULL)
    {
        msg_Err (crd, "TLS client plugin not available");
        vlc_object_release (crd);
        return NULL;
    }

    return crd;
}

void vlc_tls_Delete (vlc_tls_creds_t *crd)
{
    if (crd == NULL)
        return;

    vlc_module_unload(crd, crd->module, tls_unload, crd);
    vlc_object_release (crd);
}


/*** TLS  session ***/

static vlc_tls_t *vlc_tls_SessionCreate(vlc_tls_creds_t *crd,
                                        vlc_tls_t *sock,
                                        const char *host,
                                        const char *const *alpn)
{
    vlc_tls_t *session;
    int canc = vlc_savecancel();
    session = crd->open(crd, sock, host, alpn);
    vlc_restorecancel(canc);
    if (session != NULL)
        session->p = sock;
    return session;
}

void vlc_tls_SessionDelete (vlc_tls_t *session)
{
    int canc = vlc_savecancel();
    session->close(session);
    vlc_restorecancel(canc);
}

static void cleanup_tls(void *data)
{
    vlc_tls_t *session = data;

    vlc_tls_SessionDelete (session);
}

#undef vlc_tls_ClientSessionCreate
vlc_tls_t *vlc_tls_ClientSessionCreate(vlc_tls_creds_t *crd, vlc_tls_t *sock,
                                       const char *host, const char *service,
                                       const char *const *alpn, char **alp)
{
    int val;

    vlc_tls_t *session = vlc_tls_SessionCreate(crd, sock, host, alpn);
    if (session == NULL)
        return NULL;

    int canc = vlc_savecancel();
    vlc_tick_t deadline = mdate ();
    deadline += var_InheritInteger (crd, "ipv4-timeout") * 1000;

    struct pollfd ufd[1];
    ufd[0].fd = vlc_tls_GetFD(sock);

    vlc_cleanup_push (cleanup_tls, session);
    while ((val = crd->handshake(crd, session, host, service, alp)) != 0)
    {
        if (val < 0 || vlc_killed() )
        {
            if (val < 0)
                msg_Err(crd, "TLS session handshake error");
error:
            vlc_tls_SessionDelete (session);
            session = NULL;
            break;
        }

        vlc_tick_t now = mdate ();
        if (now > deadline)
           now = deadline;

        assert (val <= 2);
        ufd[0] .events = (val == 1) ? POLLIN : POLLOUT;

        vlc_restorecancel(canc);
        val = vlc_poll_i11e(ufd, 1, (deadline - now) / 1000);
        canc = vlc_savecancel();
        if (val == 0)
        {
            msg_Err(crd, "TLS session handshake timeout");
            goto error;
        }
    }
    vlc_cleanup_pop();
    vlc_restorecancel(canc);
    return session;
}

vlc_tls_t *vlc_tls_ServerSessionCreate(vlc_tls_creds_t *crd,
                                       vlc_tls_t *sock,
                                       const char *const *alpn)
{
    return vlc_tls_SessionCreate(crd, sock, NULL, alpn);
}

ssize_t vlc_tls_Read(vlc_tls_t *session, void *buf, size_t len, bool waitall)
{
    struct pollfd ufd;
    struct iovec iov;

    ufd.fd = vlc_tls_GetFD(session);
    ufd.events = POLLIN;
    iov.iov_base = buf;
    iov.iov_len = len;

    for (size_t rcvd = 0;;)
    {
        if (vlc_killed())
        {
            errno = EINTR;
            return -1;
        }

        ssize_t val = session->readv(session, &iov, 1);
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

    ufd.fd = vlc_tls_GetFD(session);
    ufd.events = POLLOUT;
    iov.iov_base = (void *)buf;
    iov.iov_len = len;

    for (size_t sent = 0;;)
    {
        if (vlc_killed())
        {
            errno = EINTR;
            return -1;
        }

        ssize_t val = session->writev(session, &iov, 1);
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
    else
        line[linelen - 1] = '\0';
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

static int vlc_tls_SocketGetFD(vlc_tls_t *tls)
{
    vlc_tls_socket_t *sock = (struct vlc_tls_socket *)tls;

    return sock->fd;
}

static ssize_t vlc_tls_SocketRead(vlc_tls_t *tls, struct iovec *iov,
                                  unsigned count)
{
    struct msghdr msg =
    {
        .msg_iov = iov,
        .msg_iovlen = count,
    };

    return recvmsg(vlc_tls_SocketGetFD(tls), &msg, 0);
}

static ssize_t vlc_tls_SocketWrite(vlc_tls_t *tls, const struct iovec *iov,
                                   unsigned count)
{
    const struct msghdr msg =
    {
        .msg_iov = (struct iovec *)iov,
        .msg_iovlen = count,
    };

    return sendmsg(vlc_tls_SocketGetFD(tls), &msg, MSG_NOSIGNAL);
}

static int vlc_tls_SocketShutdown(vlc_tls_t *tls, bool duplex)
{
    return shutdown(vlc_tls_SocketGetFD(tls), duplex ? SHUT_RDWR : SHUT_WR);
}

static void vlc_tls_SocketClose(vlc_tls_t *tls)
{
    net_Close(vlc_tls_SocketGetFD(tls));
    free(tls);
}

static vlc_tls_t *vlc_tls_SocketAlloc(int fd,
                                      const struct sockaddr *restrict peer,
                                      socklen_t peerlen)
{
    vlc_tls_socket_t *sock = malloc(sizeof (*sock) + peerlen);
    if (unlikely(sock == NULL))
        return NULL;

    vlc_tls_t *tls = &sock->tls;

    tls->get_fd = vlc_tls_SocketGetFD;
    tls->readv = vlc_tls_SocketRead;
    tls->writev = vlc_tls_SocketWrite;
    tls->shutdown = vlc_tls_SocketShutdown;
    tls->close = vlc_tls_SocketClose;
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

    /* Next time, write directly. Do not retry to connect. */
    tls->writev = vlc_tls_SocketWrite;

    ret = sendmsg(vlc_tls_SocketGetFD(tls), &msg, MSG_NOSIGNAL|MSG_FASTOPEN);
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
#else
    tls->writev = vlc_tls_SocketWrite;
#endif

    if (vlc_tls_Connect(tls))
        return -1;

    return vlc_tls_SocketWrite(tls, iov, count);
}

vlc_tls_t *vlc_tls_SocketOpenAddrInfo(const struct addrinfo *restrict info,
                                      bool defer_connect)
{
    vlc_tls_t *sock = vlc_tls_SocketAddrInfo(info);
    if (sock == NULL)
        return NULL;

    if (defer_connect)
    {   /* The socket is not connected yet.
         * The connection will be triggered on the first send. */
        sock->writev = vlc_tls_ConnectWrite;
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

    /* TODO: implement RFC6555 */
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

vlc_tls_t *vlc_tls_SocketOpenTLS(vlc_tls_creds_t *creds, const char *name,
                                 unsigned port, const char *service,
                                 const char *const *alpn, char **alp)
{
    struct addrinfo hints =
    {
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
    }, *res;

    msg_Dbg(creds, "resolving %s ...", name);

    int val = vlc_getaddrinfo_i11e(name, port, &hints, &res);
    if (val != 0)
    {   /* TODO: C locale for gai_strerror() */
        msg_Err(creds, "cannot resolve %s port %u: %s", name, port,
                gai_strerror(val));
        return NULL;
    }

    for (const struct addrinfo *p = res; p != NULL; p = p->ai_next)
    {
        vlc_tls_t *tcp = vlc_tls_SocketOpenAddrInfo(p, true);
        if (tcp == NULL)
        {
            msg_Err(creds, "socket error: %s", vlc_strerror_c(errno));
            continue;
        }

        vlc_tls_t *tls = vlc_tls_ClientSessionCreate(creds, tcp, name, service,
                                                     alpn, alp);
        if (tls != NULL)
        {   /* Success! */
            freeaddrinfo(res);
            return tls;
        }

        msg_Err(creds, "connection error: %s", vlc_strerror_c(errno));
        vlc_tls_SessionDelete(tcp);
    }

    /* Failure! */
    freeaddrinfo(res);
    return NULL;
}
