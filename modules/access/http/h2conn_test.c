/*****************************************************************************
 * h2conn_test.c: HTTP/2 connection tests
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

#undef NDEBUG

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#ifndef SOCK_CLOEXEC
# define SOCK_CLOEXEC 0
#endif

#include <vlc_common.h>
#include <vlc_block.h>
#include "h2frame.h"
#include "h2conn.h"
#include "message.h"
#include "transport.h"

/* I/O callbacks */
static int internal_fd = -1;

ssize_t vlc_https_send(struct vlc_tls *tls, const void *buf, size_t len)
{
    assert(tls == NULL);
    (void) buf;
    return len;
}

ssize_t vlc_https_recv(struct vlc_tls *tls, void *buf, size_t size)
{
    assert(tls == NULL);
    return read(internal_fd, buf, size);
}

void vlc_https_disconnect(struct vlc_tls *tls)
{
    assert(tls == NULL);
    if (close(internal_fd))
        assert(!"close");
}

static struct vlc_h2_conn *conn;
static int external_fd;

static void conn_send(struct vlc_h2_frame *f)
{
    assert(f != NULL);

    size_t len = vlc_h2_frame_size(f);
    ssize_t val = write(external_fd, f->data, len);
    assert((size_t)val == len);
    free(f);
}

static void conn_create(void)
{
    int fds[2];

    if (socketpair(PF_LOCAL, SOCK_STREAM|SOCK_CLOEXEC, 0, fds))
        assert(!"socketpair");

    external_fd = fds[0];
    internal_fd = fds[1];

    conn = vlc_h2_conn_create(NULL);
    assert(conn != NULL);
    conn_send(vlc_h2_frame_settings());
}

static void conn_destroy(void)
{
    shutdown(external_fd, SHUT_WR);
    vlc_h2_conn_release(conn);
    close(external_fd);
}

static struct vlc_http_stream *stream_open(void)
{
    struct vlc_http_msg *m = vlc_http_req_create("GET", "https",
                                                 "www.example.com", "/");
    assert(m != NULL);

    struct vlc_http_stream *s = vlc_h2_stream_open(conn, m);
    vlc_http_msg_destroy(m);
    return s;
}

static void stream_reply(uint_fast32_t id, bool nodata)
{
    struct vlc_http_msg *m = vlc_http_resp_create(200);
    assert(m != NULL);
    vlc_http_msg_add_agent(m, "VLC-h2-tester");

    conn_send(vlc_http_msg_h2_frame(m, id, nodata));
    vlc_http_msg_destroy(m);
}

static void stream_continuation(uint_fast32_t id)
{
    const char *h[][2] = {
        { ":status", "100" },
    };

    conn_send(vlc_h2_frame_headers(id, VLC_H2_DEFAULT_MAX_FRAME, false, 1, h));
}

static void stream_data(uint_fast32_t id, const char *str, bool eos)
{
    conn_send(vlc_h2_frame_data(id, str, strlen(str), eos));
}

int main(void)
{
    struct vlc_http_stream *s, *s2;
    struct vlc_http_msg *m;
    struct block_t *b;
    uint_fast32_t sid = -1; /* Second guessed stream IDs :-/ */

    conn_create();
    conn_destroy();

    conn_create();
    conn_send(vlc_h2_frame_ping(42));

    /* Test rejected stream */
    sid += 2;
    s = stream_open();
    assert(s != NULL);
    conn_send(vlc_h2_frame_rst_stream(sid, VLC_H2_REFUSED_STREAM));
    m = vlc_http_stream_read_headers(s);
    assert(m == NULL);
    b = vlc_http_stream_read(s);
    assert(b == NULL);
    vlc_http_stream_close(s, false);

    /* Test accepted stream */
    sid += 2;
    s = stream_open();
    assert(s != NULL);
    stream_reply(sid, false);
    m = vlc_http_stream_read_headers(s);
    assert(m != NULL);
    vlc_http_msg_destroy(m);

    /* Test late data */
    stream_data(3, "Hello ", false);
    stream_data(3, "world!", true);

    /* Test continuation then accepted stream */
    sid += 2;
    s = stream_open();
    assert(s != NULL);
    stream_continuation(sid);
    m = vlc_http_stream_read_headers(s);
    assert(m != NULL);
    assert(vlc_http_msg_get_status(m) == 100);
    stream_reply(sid, false);
    m = vlc_http_msg_iterate(m);
    assert(m != NULL);
    stream_data(sid, "Hello ", false);
    stream_data(sid, "world!", true);
    stream_data(sid, "Stray message", false); /* data after EOS */
    b = vlc_http_msg_read(m);
    assert(b != NULL);
    block_Release(b);
    b = vlc_http_msg_read(m);
    assert(b != NULL);
    block_Release(b);
    b = vlc_http_msg_read(m);
    assert(b == NULL);
    vlc_http_msg_destroy(m);

    /* Test accepted stream after continuation */
    sid += 2;
    s = stream_open();
    assert(s != NULL);
    stream_continuation(sid);
    stream_reply(sid, true);
    sid += 2;
    s2 = stream_open(); /* second stream to enforce test timing/ordering */
    assert(s2 != NULL);
    stream_reply(sid, true);
    m = vlc_http_stream_read_headers(s2);
    assert(m != NULL);
    vlc_http_msg_destroy(m);
    m = vlc_http_stream_read_headers(s);
    assert(m != NULL);
    assert(vlc_http_msg_get_status(m) == 200);
    b = vlc_http_msg_read(m);
    assert(b == NULL);
    vlc_http_msg_destroy(m);

    /* Test multiple streams in non-LIFO order */
    sid += 2;
    s = stream_open();
    assert(s != NULL);
    sid += 2;
    s2 = stream_open();
    assert(s2 != NULL);
    stream_reply(sid, false);
    stream_reply(sid - 2, true);
    stream_data(sid, "Discarded", false); /* not read data */
    m = vlc_http_stream_read_headers(s);
    assert(m != NULL);
    vlc_http_msg_destroy(m);
    m = vlc_http_stream_read_headers(s2);
    assert(m != NULL);
    vlc_http_msg_destroy(m);

    /* Test graceful connection termination */
    sid += 2;
    s = stream_open();
    assert(s != NULL);
    conn_send(vlc_h2_frame_goaway(sid - 2, VLC_H2_NO_ERROR));
    m = vlc_http_stream_read_headers(s);
    assert(m == NULL);

    /* Test stream after connection shut down */
    assert(stream_open() == NULL);

    /* Test releasing connection before stream */
    conn_destroy();
    vlc_http_stream_close(s, false);

    return 0;
}
