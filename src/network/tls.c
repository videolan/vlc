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
 * libvlc interface to the Transport Layer Security (TLS) plugins.
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
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
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

    vlc_module_unload (crd->module, tls_unload, crd);
    vlc_object_release (crd);
}


/*** TLS  session ***/

static vlc_tls_t *vlc_tls_SessionCreate(vlc_tls_creds_t *crd,
                                        vlc_tls_t *sock,
                                        const char *host,
                                        const char *const *alpn)
{
    vlc_tls_t *session = malloc(sizeof (*session));
    if (unlikely(session == NULL))
        return NULL;

    session->obj = crd->obj.parent;
    session->p = NULL;

    int canc = vlc_savecancel();

    if (crd->open(crd, session, sock, host, alpn) != VLC_SUCCESS)
    {
        free(session);
        session = NULL;
    }

    vlc_restorecancel(canc);
    return session;
}

void vlc_tls_SessionDelete (vlc_tls_t *session)
{
    do
    {
        int canc = vlc_savecancel();
        session->close(session);
        vlc_restorecancel(canc);

        vlc_tls_t *sock = session->p;
        free(session);
        session = sock;
    }
    while (session != NULL);
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
    mtime_t deadline = mdate ();
    deadline += var_InheritInteger (crd, "ipv4-timeout") * 1000;

    struct pollfd ufd[1];
    ufd[0].fd = vlc_tls_GetFD(sock);

    vlc_cleanup_push (cleanup_tls, session);
    while ((val = crd->handshake(crd, session, host, service, alp)) != 0)
    {
        if (val < 0)
        {
            msg_Err(crd, "TLS session handshake error");
error:
            vlc_tls_SessionDelete (session);
            session = NULL;
            break;
        }

        mtime_t now = mdate ();
        if (now > deadline)
           now = deadline;

        assert (val <= 2);
        ufd[0] .events = (val == 1) ? POLLIN : POLLOUT;

        vlc_restorecancel(canc);
        val = poll (ufd, 1, (deadline - now) / 1000);
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

vlc_tls_t *vlc_tls_ServerSessionCreate(vlc_tls_creds_t *crd, int fd,
                                       const char *const *alpn)
{
    vlc_tls_t *sock = vlc_tls_SocketOpen(VLC_OBJECT(crd), fd);
    if (unlikely(sock == NULL))
        return NULL;

    vlc_tls_t *tls = vlc_tls_SessionCreate(crd, sock, NULL, alpn);
    if (unlikely(tls == NULL))
        vlc_tls_SessionDelete(sock);
    else
        tls->p = sock;
    return tls;
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
        if (val == -1 && errno != EINTR && errno != EAGAIN)
            return rcvd ? (ssize_t)rcvd : -1;

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
        if (val == -1 && errno != EINTR && errno != EAGAIN)
            return sent ? (ssize_t)sent : -1;

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

static int vlc_tls_SocketGetFD(vlc_tls_t *tls)
{
    return (intptr_t)tls->sys;
}

static ssize_t vlc_tls_SocketRead(vlc_tls_t *tls, struct iovec *iov,
                                  unsigned count)
{
    int fd = (intptr_t)tls->sys;
    struct msghdr msg =
    {
        .msg_iov = iov,
        .msg_iovlen = count,
    };
    return recvmsg(fd, &msg, 0);
}

static ssize_t vlc_tls_SocketWrite(vlc_tls_t *tls, const struct iovec *iov,
                                   unsigned count)
{
    int fd = (intptr_t)tls->sys;
    const struct msghdr msg =
    {
        .msg_iov = (struct iovec *)iov,
        .msg_iovlen = count,
    };
    return sendmsg(fd, &msg, MSG_NOSIGNAL);
}

static int vlc_tls_SocketShutdown(vlc_tls_t *tls, bool duplex)
{
    int fd = (intptr_t)tls->sys;
    return shutdown(fd, duplex ? SHUT_RDWR : SHUT_WR);
}

static void vlc_tls_SocketClose(vlc_tls_t *tls)
{
#if 0
    int fd = (intptr_t)tls->sys;

    net_Close(fd);
#else
    (void) tls;
#endif
}

vlc_tls_t *vlc_tls_SocketOpen(vlc_object_t *obj, int fd)
{
    vlc_tls_t *session = malloc(sizeof (*session));
    if (unlikely(session == NULL))
        return NULL;

    session->obj = obj;
    session->sys = (void *)(intptr_t)fd;
    session->get_fd = vlc_tls_SocketGetFD;
    session->readv = vlc_tls_SocketRead;
    session->writev = vlc_tls_SocketWrite;
    session->shutdown = vlc_tls_SocketShutdown;
    session->close = vlc_tls_SocketClose;
    session->p = NULL;
    return session;
}
