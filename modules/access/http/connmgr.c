/*****************************************************************************
 * connmgr.c: HTTP/TLS VLC connection manager
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
# include "config.h"
#endif

#include <assert.h>
#include <vlc_common.h>
#include <vlc_tls.h>
#include <vlc_interrupt.h>
#include "transport.h"
#include "h1conn.h"
#include "h2conn.h"
#include "connmgr.h"
#include "message.h"

#pragma GCC visibility push(default)

struct vlc_https_connecting
{
    vlc_tls_creds_t *creds;
    const char *host;
    unsigned port;
    bool http2;
    vlc_sem_t done;
};

static void *vlc_https_connect_thread(void *data)
{
    struct vlc_https_connecting *c = data;
    vlc_tls_t *tls;

    tls = vlc_https_connect(c->creds, c->host, c->port, &c->http2);
    vlc_sem_post(&c->done);
    return tls;
}

/** Interruptible vlc_https_connect() */
static vlc_tls_t *vlc_https_connect_i11e(vlc_tls_creds_t *creds,
                                         const char *host, unsigned port,
                                         bool *restrict http_two)
{
    struct vlc_https_connecting c;
    vlc_thread_t th;

    c.creds = creds;
    c.host = host;
    c.port = port;
    vlc_sem_init(&c.done, 0);

    if (vlc_clone(&th, vlc_https_connect_thread, &c,
                  VLC_THREAD_PRIORITY_INPUT))
        return NULL;

    /* This would be much simpler if vlc_join_i11e() existed. */
    void *res;

    if (vlc_sem_wait_i11e(&c.done))
        vlc_cancel(th);
    vlc_join(th, &res);
    vlc_sem_destroy(&c.done);

    if (res == VLC_THREAD_CANCELED)
        res = NULL;
    if (res != NULL)
        *http_two = c.http2;
    return res;
}

struct vlc_http_mgr
{
    vlc_tls_creds_t *creds;
    struct vlc_h1_conn *conn1;
    struct vlc_h2_conn *conn2;
};

static struct vlc_h1_conn *vlc_h1_conn_find(struct vlc_http_mgr *mgr,
                                            const char *host, unsigned port)
{
    (void) host; (void) port;
    return mgr->conn1;
}

static struct vlc_h2_conn *vlc_h2_conn_find(struct vlc_http_mgr *mgr,
                                            const char *host, unsigned port)
{
    (void) host; (void) port;
    return mgr->conn2;
}

static
struct vlc_http_msg *vlc_https_request_reuse(struct vlc_http_mgr *mgr,
                                             const char *host, unsigned port,
                                             const struct vlc_http_msg *req)
{
    struct vlc_h2_conn *conn2 = vlc_h2_conn_find(mgr, host, port);
    if (conn2 != NULL)
    {
        struct vlc_http_stream *s = vlc_h2_stream_open(conn2, req);
        if (s != NULL)
        {
            struct vlc_http_msg *m = vlc_http_stream_read_headers(s);
            if (m != NULL)
                return m;

            vlc_http_stream_close(s, false);
            /* NOTE: If the request were not idempotent, NULL should be
             * returned here. POST is not used/supported so far, and CONNECT is
             * treated as if it were idempotent (which turns out OK here). */
        }
        /* Get rid of closing or reset connection */
        vlc_h2_conn_release(conn2);
        mgr->conn2 = NULL;
    }

    struct vlc_h1_conn *conn1 = vlc_h1_conn_find(mgr, host, port);
    if (conn1 != NULL)
    {
        struct vlc_http_stream *s = vlc_h1_stream_open(conn1, req);
        if (s != NULL)
        {
            struct vlc_http_msg *m = vlc_http_stream_read_headers(s);
            if (m != NULL)
                return m;

            vlc_http_stream_close(s, false);
        }
        vlc_h1_conn_release(conn1);
        mgr->conn1 = NULL;
    }
    return NULL;
}

struct vlc_http_msg *vlc_https_request(struct vlc_http_mgr *mgr,
                                       const char *host, unsigned port,
                                       const struct vlc_http_msg *req)
{
    /* TODO? non-idempotent request support */
    struct vlc_http_msg *resp = vlc_https_request_reuse(mgr, host, port, req);
    if (resp != NULL)
        return resp;

    bool http2;
    vlc_tls_t *tls = vlc_https_connect_i11e(mgr->creds, host, port, &http2);

    if (tls == NULL)
        return NULL;

    if (http2)
    {
        struct vlc_h2_conn *conn2 = vlc_h2_conn_create(tls);
        if (likely(conn2 != NULL))
            mgr->conn2 = conn2;
        else
            vlc_https_disconnect(tls);
    }
    else /* TODO: HTTP/1.x support */
    {
        struct vlc_h1_conn *conn1 = vlc_h1_conn_create(tls);
        if (likely(conn1 != NULL))
            mgr->conn1 = conn1;
        else
            vlc_https_disconnect(tls);
    }

    return vlc_https_request_reuse(mgr, host, port, req);
}

struct vlc_http_mgr *vlc_http_mgr_create(vlc_object_t *obj)
{
    struct vlc_http_mgr *mgr = malloc(sizeof (*mgr));
    if (unlikely(mgr == NULL))
        return NULL;

    mgr->creds = vlc_tls_ClientCreate(obj);
    if (mgr->creds == NULL)
    {
        free(mgr);
        return NULL;
    }

    mgr->conn1 = NULL;
    mgr->conn2 = NULL;
    return mgr;
}

void vlc_http_mgr_destroy(struct vlc_http_mgr *mgr)
{
    if (mgr->conn2 != NULL)
        vlc_h2_conn_release(mgr->conn2);
    if (mgr->conn1 != NULL)
        vlc_h1_conn_release(mgr->conn1);
    vlc_tls_Delete(mgr->creds);
    free(mgr);
}
