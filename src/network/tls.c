/*****************************************************************************
 * tls.c
 *****************************************************************************
 * Copyright © 2004-2007 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
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

vlc_tls_t *vlc_tls_SessionCreate (vlc_tls_creds_t *crd, int fd,
                                  const char *host, const char *const *alpn)
{
    vlc_tls_t *session = vlc_custom_create (crd, sizeof (*session),
                                            "tls session");
    int val = crd->open (crd, session, fd, host, alpn);
    if (val != VLC_SUCCESS)
    {
        vlc_object_release (session);
        return NULL;
    }
    session->fd = fd;
    return session;
}

int vlc_tls_SessionHandshake (vlc_tls_t *session, const char *host,
                              const char *service, char **restrict alp)
{
    vlc_tls_creds_t *crd = (vlc_tls_creds_t *)(session->p_parent);

    return crd->handshake (session, host, service, alp);
}

void vlc_tls_SessionDelete (vlc_tls_t *session)
{
    vlc_tls_creds_t *crd = (vlc_tls_creds_t *)(session->p_parent);

    crd->close (session);
    vlc_object_release (session);
}

static void cleanup_tls(void *data)
{
    vlc_tls_t *session = data;

    vlc_tls_SessionDelete (session);
}

vlc_tls_t *vlc_tls_ClientSessionCreate (vlc_tls_creds_t *crd, int fd,
                                        const char *host, const char *service,
                                        const char *const *alpn, char **alp)
{
    vlc_tls_t *session;
    int canc, val;

    canc = vlc_savecancel();
    session = vlc_tls_SessionCreate (crd, fd, host, alpn);
    if (session == NULL)
    {
        vlc_restorecancel(canc);
        return NULL;
    }

    mtime_t deadline = mdate ();
    deadline += var_InheritInteger (crd, "ipv4-timeout") * 1000;

    struct pollfd ufd[1];
    ufd[0].fd = fd;

    vlc_cleanup_push (cleanup_tls, session);
    while ((val = vlc_tls_SessionHandshake (session, host, service, alp)) != 0)
    {
        if (val < 0)
        {
            msg_Err (session, "TLS client session handshake error");
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
            msg_Err (session, "TLS client session handshake timeout");
            goto error;
        }
    }
    vlc_cleanup_pop();
    vlc_restorecancel(canc);
    return session;
}

int vlc_tls_Read(vlc_tls_t *session, void *buf, size_t len, bool waitall)
{
    struct pollfd ufd;

    ufd.fd = session->fd;
    ufd.events = POLLIN;

    for (size_t rcvd = 0;;)
    {
        if (vlc_killed())
        {
            errno = EINTR;
            return -1;
        }

        ssize_t val = session->sock.pf_recv(session, buf, len);
        if (val > 0)
        {
            if (!waitall)
                return val;
            buf = ((char *)buf) + val;
            len -= val;
            rcvd += val;
        }
        if (len == 0 || val == 0)
            return rcvd;
        if (val == -1 && errno != EINTR && errno != EAGAIN)
            return rcvd ? (ssize_t)rcvd : -1;

        vlc_poll_i11e(&ufd, 1, -1);
    }
}

int vlc_tls_Write(vlc_tls_t *session, const void *buf, size_t len)
{
    struct pollfd ufd;

    ufd.fd = session->fd;
    ufd.events = POLLOUT;

    for (size_t sent = 0;;)
    {
        if (vlc_killed())
        {
            errno = EINTR;
            return -1;
        }

        ssize_t val = session->sock.pf_send(session, buf, len);
        if (val > 0)
        {
            buf = ((const char *)buf) + val;
            len -= val;
            sent += val;
        }
        if (len == 0 || val == 0)
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
