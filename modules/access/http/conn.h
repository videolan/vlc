/*****************************************************************************
 * conn.h: HTTP connections
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

/**
 * \defgroup http_conn Connections
 * HTTP connections
 * \ingroup http_connmgr
 * @{
 */

struct vlc_tls;
struct vlc_http_conn;
struct vlc_http_msg;
struct vlc_http_stream;

struct vlc_http_conn_cbs
{
    struct vlc_http_stream *(*stream_open)(struct vlc_http_conn *,
                                           const struct vlc_http_msg *);
    void (*release)(struct vlc_http_conn *);
};

struct vlc_http_conn
{
    const struct vlc_http_conn_cbs *cbs;
    struct vlc_tls *tls;
};

static inline struct vlc_http_stream *
vlc_http_stream_open(struct vlc_http_conn *conn, const struct vlc_http_msg *m)
{
    return conn->cbs->stream_open(conn, m);
}

static inline void vlc_http_conn_release(struct vlc_http_conn *conn)
{
    conn->cbs->release(conn);
}

void vlc_http_err(void *, const char *msg, ...) VLC_FORMAT(2, 3);
void vlc_http_dbg(void *, const char *msg, ...) VLC_FORMAT(2, 3);

/**
 * \defgroup http1 HTTP/1.x
 * @{
 */
struct vlc_http_conn *vlc_h1_conn_create(void *ctx, struct vlc_tls *,
                                         bool proxy);
struct vlc_http_stream *vlc_chunked_open(struct vlc_http_stream *,
                                         struct vlc_tls *);

/** @} */

/**
 * \defgroup h2 HTTP/2.0
 * @{
 */
struct vlc_http_conn *vlc_h2_conn_create(void *ctx, struct vlc_tls *);

/** @} */

/** @} */
