/*****************************************************************************
 * tls.c
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
 * @ingroup tls
 * @file
 * Transport Layer Session protocol API.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_POLL_H
# include <poll.h>
#endif
#include <assert.h>
#include <errno.h>
#ifndef SOL_TCP
# define SOL_TCP IPPROTO_TCP
#endif

#include <vlc_common.h>
#include "libvlc.h"

#include <vlc_tls.h>
#include <vlc_modules.h>
#include <vlc_interrupt.h>

/*** TLS credentials ***/

static int tls_server_load(void *func, bool forced, va_list ap)
{
    int (*activate)(vlc_tls_server_t *, const char *, const char *) = func;
    vlc_tls_server_t *crd = va_arg(ap, vlc_tls_server_t *);
    const char *cert = va_arg (ap, const char *);
    const char *key = va_arg (ap, const char *);

    int ret = activate (crd, cert, key);
    if (ret)
        vlc_objres_clear(VLC_OBJECT(crd));
    (void) forced;
    return ret;
}

static int tls_client_load(void *func, bool forced, va_list ap)
{
    int (*activate)(vlc_tls_client_t *) = func;
    vlc_tls_client_t *crd = va_arg(ap, vlc_tls_client_t *);

    int ret = activate (crd);
    if (ret)
        vlc_objres_clear(VLC_OBJECT(crd));
    (void) forced;
    return ret;
}

vlc_tls_server_t *
vlc_tls_ServerCreate (vlc_object_t *obj, const char *cert_path,
                      const char *key_path)
{
    vlc_tls_server_t *srv = vlc_custom_create(obj, sizeof (*srv),
                                              "tls server");
    if (unlikely(srv == NULL))
        return NULL;

    if (key_path == NULL)
        key_path = cert_path;

    if (vlc_module_load(srv, "tls server", NULL, false,
                        tls_server_load, srv, cert_path, key_path) == NULL)
    {
        msg_Err (srv, "TLS server plugin not available");
        vlc_object_delete(srv);
        return NULL;
    }

    return srv;
}

void vlc_tls_ServerDelete(vlc_tls_server_t *crd)
{
    if (crd == NULL)
        return;

    crd->ops->destroy(crd);
    vlc_objres_clear(VLC_OBJECT(crd));
    vlc_object_delete(crd);
}

vlc_tls_client_t *vlc_tls_ClientCreate(vlc_object_t *obj)
{
    vlc_tls_client_t *crd = vlc_custom_create(obj, sizeof (*crd),
                                              "tls client");
    if (unlikely(crd == NULL))
        return NULL;

    if (vlc_module_load(crd, "tls client", NULL, false,
                        tls_client_load, crd) == NULL)
    {
        msg_Err (crd, "TLS client plugin not available");
        vlc_object_delete(crd);
        return NULL;
    }

    return crd;
}

void vlc_tls_ClientDelete(vlc_tls_client_t *crd)
{
    if (crd == NULL)
        return;

    crd->ops->destroy(crd);
    vlc_objres_clear(VLC_OBJECT(crd));
    vlc_object_delete(crd);
}


/*** TLS  session ***/

void vlc_tls_SessionDelete (vlc_tls_t *session)
{
    int canc = vlc_savecancel();
    session->ops->close(session);
    vlc_restorecancel(canc);
}

static void cleanup_tls(void *data)
{
    vlc_tls_t *session = data;

    vlc_tls_SessionDelete (session);
}

vlc_tls_t *vlc_tls_ClientSessionCreate(vlc_tls_client_t *crd, vlc_tls_t *sock,
                                       const char *host, const char *service,
                                       const char *const *alpn, char **alp)
{
    int val;
    int canc = vlc_savecancel();
    vlc_tls_t *session = crd->ops->open(crd, sock, host, alpn);
    vlc_restorecancel(canc);

    if (session == NULL)
        return NULL;

    session->p = sock;

    canc = vlc_savecancel();
    vlc_tick_t deadline = vlc_tick_now ();
    deadline += VLC_TICK_FROM_MS( var_InheritInteger (crd, "ipv4-timeout") );

    vlc_cleanup_push (cleanup_tls, session);
    while ((val = crd->ops->handshake(session, host, service, alp)) != 0)
    {
        struct pollfd ufd[1];

        if (val < 0 || vlc_killed() )
        {
            if (val < 0)
                msg_Err(crd, "TLS session handshake error");
error:
            vlc_tls_SessionDelete (session);
            session = NULL;
            break;
        }

        vlc_tick_t now = vlc_tick_now ();
        if (now > deadline)
           now = deadline;

        assert (val <= 2);

        ufd[0].events = (val == 1) ? POLLIN : POLLOUT;
        ufd[0].fd = vlc_tls_GetPollFD(sock, &ufd->events);

        vlc_restorecancel(canc);
        val = vlc_poll_i11e(ufd, 1, MS_FROM_VLC_TICK(deadline - now));
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

vlc_tls_t *vlc_tls_ServerSessionCreate(vlc_tls_server_t *crd,
                                       vlc_tls_t *sock,
                                       const char *const *alpn)
{
    int canc = vlc_savecancel();
    vlc_tls_t *session = crd->ops->open(crd, sock, alpn);
    vlc_restorecancel(canc);
    if (session != NULL)
        session->p = sock;
    return session;
}

vlc_tls_t *vlc_tls_SocketOpenTLS(vlc_tls_client_t *creds, const char *name,
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
