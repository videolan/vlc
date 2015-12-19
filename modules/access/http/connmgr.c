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
#include "conn.h"
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
    struct vlc_http_conn *conn;
};

static struct vlc_http_conn *vlc_http_mgr_find(struct vlc_http_mgr *mgr,
                                               const char *host, unsigned port)
{
    (void) host; (void) port;
    return mgr->conn;
}

static void vlc_http_mgr_release(struct vlc_http_mgr *mgr,
                                 struct vlc_http_conn *conn)
{
    assert(mgr->conn == conn);
    mgr->conn = NULL;

    vlc_http_conn_release(conn);
}

static
struct vlc_http_msg *vlc_http_mgr_reuse(struct vlc_http_mgr *mgr,
                                        const char *host, unsigned port,
                                        const struct vlc_http_msg *req)
{
    struct vlc_http_conn *conn = vlc_http_mgr_find(mgr, host, port);
    if (conn == NULL)
        return NULL;

    struct vlc_http_stream *stream = vlc_http_stream_open(conn, req);
    if (stream != NULL)
    {
        struct vlc_http_msg *m = vlc_http_stream_read_headers(stream);
        if (m != NULL)
            return m;

        vlc_http_stream_close(stream, false);
        /* NOTE: If the request were not idempotent, we do not know if it was
         * process by the other end. So POST is not used/supported so far, and
         * CONNECT is treated as if it were idempotent (which is OK here). */
    }
    /* Get rid of closing or reset connection */
    vlc_http_mgr_release(mgr, conn);
    return NULL;
}

struct vlc_http_msg *vlc_https_request(struct vlc_http_mgr *mgr,
                                       const char *host, unsigned port,
                                       const struct vlc_http_msg *req)
{
    /* TODO? non-idempotent request support */
    struct vlc_http_msg *resp = vlc_http_mgr_reuse(mgr, host, port, req);
    if (resp != NULL)
        return resp;

    bool http2;
    vlc_tls_t *tls = vlc_https_connect_i11e(mgr->creds, host, port, &http2);
    if (tls == NULL)
        return NULL;

    struct vlc_http_conn *conn;

    if (http2)
        conn = vlc_h2_conn_create(tls);
    else
        conn = vlc_h1_conn_create(tls);

    if (unlikely(conn == NULL))
    {
        vlc_tls_Close(tls);
        return NULL;
    }

    mgr->conn = conn;

    return vlc_http_mgr_reuse(mgr, host, port, req);
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

    mgr->conn = NULL;
    return mgr;
}

void vlc_http_mgr_destroy(struct vlc_http_mgr *mgr)
{
    if (mgr->conn != NULL)
        vlc_http_mgr_release(mgr, mgr->conn);
    vlc_tls_Delete(mgr->creds);
    free(mgr);
}
