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
                                                 unsigned port,
                                                 const char *username,
                                                 const char *password)
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
    if (username != NULL)
        vlc_http_msg_add_creds_basic(req, true, username,
                                     (password != NULL) ? password : "");

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

typedef struct vlc_tls_proxy
{
    vlc_tls_t tls;
    vlc_tls_t *sock;
} vlc_tls_proxy_t;

static int vlc_tls_ProxyGetFD(vlc_tls_t *tls)
{
    vlc_tls_proxy_t *proxy = (vlc_tls_proxy_t *)tls;

    return vlc_tls_GetFD(proxy->sock);
}

static ssize_t vlc_tls_ProxyRead(vlc_tls_t *tls, struct iovec *iov,
                                 unsigned count)
{
    vlc_tls_proxy_t *proxy = (vlc_tls_proxy_t *)tls;
    vlc_tls_t *sock = proxy->sock;

    return sock->readv(sock, iov, count);
}

static ssize_t vlc_tls_ProxyWrite(vlc_tls_t *tls, const struct iovec *iov,
                                  unsigned count)
{
    vlc_tls_proxy_t *proxy = (vlc_tls_proxy_t *)tls;
    vlc_tls_t *sock = proxy->sock;

    return sock->writev(sock, iov, count);
}

static int vlc_tls_ProxyShutdown(vlc_tls_t *tls, bool duplex)
{
    vlc_tls_proxy_t *proxy = (vlc_tls_proxy_t *)tls;

    return vlc_tls_Shutdown(proxy->sock, duplex);
}

static void vlc_tls_ProxyClose(vlc_tls_t *tls)
{
    vlc_tls_proxy_t *proxy = (vlc_tls_proxy_t *)tls;

    free(proxy);
}

vlc_tls_t *vlc_https_connect_proxy(void *ctx, vlc_tls_creds_t *creds,
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

    vlc_tls_t *sock = NULL;
    bool ptwo = false;
    if (!strcasecmp(url.psz_protocol, "https"))
        sock = vlc_https_connect(creds, url.psz_host, url.i_port, &ptwo);
    else
    if (!strcasecmp(url.psz_protocol, "http"))
        sock = vlc_tls_SocketOpenTCP(creds ? creds->obj.parent : NULL,
                                     url.psz_host, url.i_port);
    else
        sock = NULL;

    if (sock == NULL)
    {
        vlc_UrlClean(&url);
        return NULL;
    }

    assert(!ptwo); /* HTTP/2 proxy not supported yet */

    vlc_tls_proxy_t *psock = malloc(sizeof (*psock));
    if (unlikely(psock == NULL))
    {
        vlc_UrlClean(&url);
        goto error;
    }

    psock->tls.get_fd = vlc_tls_ProxyGetFD;
    psock->tls.readv = vlc_tls_ProxyRead;
    psock->tls.writev = vlc_tls_ProxyWrite;
    psock->tls.shutdown = vlc_tls_ProxyShutdown;
    psock->tls.close = vlc_tls_ProxyClose;
    psock->tls.p = NULL;
    psock->sock = sock;

    struct vlc_http_conn *conn = /*ptwo ? vlc_h2_conn_create(ctx, &psock->tls)
                               :*/ vlc_h1_conn_create(ctx, &psock->tls, false);
    if (unlikely(conn == NULL))
    {
        vlc_tls_Close(&psock->tls);
        vlc_UrlClean(&url);
        goto error;
    }

    struct vlc_http_msg *resp = vlc_http_tunnel_open(conn, hostname, port,
                                                     url.psz_username,
                                                     url.psz_password);
    vlc_UrlClean(&url);

    /* TODO: reuse connection to HTTP/2 proxy */
    vlc_http_conn_release(conn); /* psock is destroyed there too */

    if (resp == NULL)
        goto error;

    vlc_tls_t *tls;
    const char *alpn[] = { "h2", "http/1.1", NULL };
    char *alp;

    tls = vlc_tls_ClientSessionCreate(creds, sock, hostname, "https",
                                      alpn + !*two, &alp);
    if (tls == NULL)
        goto error;

    *two = (alp != NULL) && !strcmp(alp, "h2");
    free(alp);
    return tls;
error:
    vlc_tls_Close(sock);
    return NULL;
}
