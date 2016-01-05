/*****************************************************************************
 * tunnel.c: HTTP CONNECT
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_tls.h>
#include <vlc_url.h>
#include "message.h"
#include "conn.h"
#include "transport.h"

static char *vlc_http_authority(const char *host, unsigned port)
{
    static const char *const formats[2] = { "%s:%u", "[%s]:%u" };
    const bool brackets = strchr(host, ':') != NULL;
    char *authority;

    if (unlikely(asprintf(&authority, formats[brackets], host, port) == -1))
        return NULL;
    return authority;
}

static struct vlc_http_msg *vlc_http_tunnel_open(struct vlc_http_conn *conn,
                                                 const char *hostname,
                                                 unsigned port)
{
    char *authority = vlc_http_authority(hostname, port);
    if (authority == NULL)
        return NULL;

    struct vlc_http_msg *req = vlc_http_req_create("CONNECT", NULL, authority,
                                                   NULL);
    free(authority);
    if (unlikely(req == NULL))
        return NULL;

    vlc_http_msg_add_header(req, "ALPN", "h2, http%%2F1.1");
    vlc_http_msg_add_agent(req, PACKAGE_NAME "/" PACKAGE_VERSION);

    struct vlc_http_stream *stream = vlc_http_stream_open(conn, req);

    vlc_http_msg_destroy(req);
    if (stream == NULL)
        return NULL;

    struct vlc_http_msg *resp = vlc_http_msg_get_initial(stream);
    resp = vlc_http_msg_get_final(resp);
    if (resp == NULL)
        return NULL;

    int status = vlc_http_msg_get_status(resp);
    if ((status / 100) != 2)
    {
        vlc_http_msg_destroy(resp);
        resp = NULL;
    }
    return resp;
}
/* So far, this will only work with HTTP 1.1 over plain TCP as the TLS protocol
 * module can only use a socket file descriptor as I/O back-end. Consequently
 * neither TLS over TLS nor TLS over HTTP/2 framing are possible. */
#define TLS_OVER_TLS 0

#if !TLS_OVER_TLS
static int vlc_http_tls_shutdown_ignore(vlc_tls_t *session, bool duplex)
{
    (void) session; (void) duplex;
    return 0;
}

static void vlc_http_tls_close_ignore(vlc_tls_t *session)
{
    (void) session;
}
#endif

vlc_tls_t *vlc_https_connect_proxy(vlc_tls_creds_t *creds,
                                   const char *hostname, unsigned port,
                                   bool *restrict two, const char *proxy)
{
    vlc_url_t url;
    int canc;

    assert(proxy != NULL);

    if (port == 0)
        port = 443;

    canc = vlc_savecancel();
    vlc_UrlParse(&url, proxy);
    vlc_restorecancel(canc);

    if (url.psz_protocol == NULL || url.psz_host == NULL)
    {
        vlc_UrlClean(&url);
        return NULL;
    }

    vlc_tls_t *session = NULL;
    bool ptwo = false;
#if TLS_OVER_TLS
    if (strcasecmp(url.psz_protocol, "https"))
        session = vlc_https_connect(creds, url.psz_host, url.i_port, &ptwo);
    else
#endif
    if (strcasecmp(url.psz_protocol, "http"))
        session = vlc_http_connect(creds ? creds->p_parent : NULL,
                                   url.psz_host, url.i_port);
    else
        session = NULL;

    vlc_UrlClean(&url);

    if (session == NULL)
        return NULL;

    struct vlc_http_conn *conn = ptwo ? vlc_h2_conn_create(session)
                                      : vlc_h1_conn_create(session, false);
    if (unlikely(conn == NULL))
    {
        vlc_tls_Close(session);
        return NULL;
    }

    struct vlc_http_msg *resp = vlc_http_tunnel_open(conn, hostname, port);

    /* TODO: reuse connection to HTTP/2 proxy */
    vlc_http_conn_release(conn);

    if (resp == NULL)
        return NULL;

    const char *alpn[] = { "h2", "http/1.1", NULL };
    char *alp;
#if TLS_OVER_TLS
# error ENOSYS
    /* TODO: create a vlc_tls_t * from a struct vlc_http_msg *. */
#else
    int fd = session->fd;

    session->shutdown = vlc_http_tls_shutdown_ignore;
    session->close = vlc_http_tls_close_ignore;
    vlc_http_msg_destroy(resp); /* <- session is destroyed here */

    session = vlc_tls_ClientSessionCreate(creds, fd, hostname, "https", alpn,
                                          &alp);
#endif
    if (session == NULL)
    {
        net_Close(fd);
        return NULL;
    }

    *two = (alp != NULL) && !strcmp(alp, "h2");
    free(alp);
    return session;
}
