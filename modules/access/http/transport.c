/*****************************************************************************
 * transport.c: HTTP/TLS TCP transport layer
 *****************************************************************************
 * Copyright © 2015 Rémi Denis-Courmont
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

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef HAVE_POLL
#include <poll.h>
#endif
#include <fcntl.h>
#include <vlc_common.h>
#include <vlc_network.h>
#include <vlc_tls.h>

#include "transport.h"

static void cleanup_addrinfo(void *data)
{
    freeaddrinfo(data);
}

static void cleanup_fd(void *data)
{
    net_Close((intptr_t)data);
}

static int vlc_tcp_connect(vlc_object_t *obj, const char *name, unsigned port)
{
    struct addrinfo hints =
    {
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
    }, *res;

    assert(name != NULL);
    msg_Dbg(obj, "resolving %s ...", name);

    int val = vlc_getaddrinfo(name, port, &hints, &res);
    if (val != 0)
    {   /* TODO: C locale for gai_strerror() */
        msg_Err(obj, "cannot resolve %s port %u: %s", name, port,
                gai_strerror(val));
        return -1;
    }

    int fd = -1;

    vlc_cleanup_push(cleanup_addrinfo, res);
    msg_Dbg(obj, "connecting to %s port %u ...", name, port);

    for (const struct addrinfo *p = res; p != NULL; p = p->ai_next)
    {
        fd = vlc_socket(p->ai_family, p->ai_socktype, p->ai_protocol, false);
        if (fd == -1)
        {
            msg_Warn(obj, "cannot create socket: %s", vlc_strerror_c(errno));
            continue;
        }

        vlc_cleanup_push(cleanup_fd, (void *)(intptr_t)fd);
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof (int));

        val = connect(fd, p->ai_addr, p->ai_addrlen);
        vlc_cleanup_pop();

        if (val == 0)
            break; /* success! */

        msg_Err(obj, "cannot connect to %s port %u: %s", name, port,
                vlc_strerror_c(errno));
        net_Close(fd);
        fd = -1;
    }

    vlc_cleanup_pop();
    freeaddrinfo(res);

    if (fd != -1)
#ifndef _WIN32
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
#else
        ioctlsocket(fd, FIONBIO, &(unsigned long){ 1 });
#endif
    return fd;
}

vlc_tls_t *vlc_http_connect(vlc_object_t *obj, const char *name, unsigned port)
{
    if (port == 0)
        port = 80;

    int fd = vlc_tcp_connect(obj, name, port);
    if (fd == -1)
        return NULL;

    vlc_tls_t *tls = vlc_tls_SocketOpen(obj, fd);
    if (tls == NULL)
        net_Close(fd);
    return tls;
}

vlc_tls_t *vlc_https_connect(vlc_tls_creds_t *creds, const char *name,
                             unsigned port, bool *restrict two)
{
    if (port == 0)
        port = 443;

    int fd = vlc_tcp_connect(creds->obj.parent, name, port);
    if (fd == -1)
        return NULL;

    /* TLS with ALPN */
    const char *alpn[] = { "h2", "http/1.1", NULL };
    char *alp;

    vlc_tls_t *tls = vlc_tls_ClientSessionCreateFD(creds, fd, name, "https",
                                                 alpn + !*two, &alp);
    if (tls == NULL)
    {
        net_Close(fd);
        return NULL;
    }

    *two = (alp != NULL) && !strcmp(alp, "h2");
    free(alp);
    return tls;
}
