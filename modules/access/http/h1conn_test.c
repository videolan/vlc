/*****************************************************************************
 * h1conn_test.c: HTTP/1 connection tests
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
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_tls.h>
#include "conn.h"
#include "message.h"

#if defined(PF_UNIX) && !defined(PF_LOCAL)
#    define PF_LOCAL PF_UNIX
#endif

const char vlc_module_name[] = "test_h1conn";

static struct vlc_http_conn *conn;
static struct vlc_tls *external_tls;

static void conn_create(void)
{
    vlc_tls_t *tlsv[2];

    if (vlc_tls_SocketPair(PF_LOCAL, 0, tlsv))
        assert(!"vlc_tls_SocketPair");

    external_tls = tlsv[0];

    conn = vlc_h1_conn_create(NULL, tlsv[1], false);
    assert(conn != NULL);
}

static void conn_send_raw(const void *buf, size_t len)
{
    ssize_t val = vlc_tls_Write(external_tls, buf, len);
    assert((size_t)val == len);
}

static void conn_send(const char *str)
{
    return conn_send_raw(str, strlen(str));
}

static void conn_shutdown(bool duplex)
{
    vlc_tls_Shutdown(external_tls, duplex);
}

static void conn_destroy(void)
{
    conn_shutdown(false);
    vlc_http_conn_release(conn);
    vlc_tls_SessionDelete(external_tls);
}

static struct vlc_http_stream *stream_open(void)
{
    struct vlc_http_msg *m = vlc_http_req_create("GET", "https",
                                                 "www.example.com", "/");
    assert(m != NULL);

    struct vlc_http_stream *s = vlc_http_stream_open(conn, m);
    vlc_http_msg_destroy(m);
    return s;
}

int main(void)
{
    struct vlc_http_stream *s;
    struct vlc_http_msg *m;
    struct block_t *b;

    /* Dummy */
    conn_create();
    conn_destroy();

    /* Test rejected connection */
    conn_create();
    conn_shutdown(SHUT_RD);
    s = stream_open();
    if (s != NULL)
        /* Remote read shutdown does not result in an error on some systems. */
        vlc_http_stream_close(s, true);
    conn_destroy();

    /* Test rejected stream */
    conn_create();
    s = stream_open();
    assert(s != NULL);
    conn_shutdown(SHUT_WR);

    m = vlc_http_stream_read_headers(s);
    assert(m == NULL);
    b = vlc_http_stream_read(s);
    assert(b == vlc_http_error);
    m = vlc_http_stream_read_headers(s);
    assert(m == NULL);
    b = vlc_http_stream_read(s);
    assert(b == vlc_http_error);
    m = vlc_http_msg_get_initial(s);
    assert(m == NULL);

    s = stream_open();
    assert(s == NULL);
    conn_destroy();

    /* Test garbage */
    conn_create();
    s = stream_open();
    assert(s != NULL);
    conn_send("Go away!\r\n\r\n");
    conn_shutdown(SHUT_WR);

    m = vlc_http_stream_read_headers(s);
    assert(m == NULL);
    b = vlc_http_stream_read(s);
    assert(b == vlc_http_error);
    conn_destroy();
    vlc_http_stream_close(s, false);

    /* Test HTTP/1.0 stream */
    conn_create();
    s = stream_open();
    assert(s != NULL);
    conn_send("HTTP/1.0 200 OK\r\n\r\n");
    m = vlc_http_msg_get_initial(s);
    assert(m != NULL);

    conn_send("Hello world!");
    conn_shutdown(SHUT_WR);
    b = vlc_http_msg_read(m);
    assert(b != NULL);
    assert(b->i_buffer == 12);
    assert(!memcmp(b->p_buffer, "Hello world!", 12));
    block_Release(b);
    b = vlc_http_msg_read(m);
    assert(b == NULL);
    vlc_http_msg_destroy(m);
    conn_destroy();

    /* Test HTTP/1.1 with closed connection */
    conn_create();
    s = stream_open();
    assert(s != NULL);
    conn_send("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n");
    m = vlc_http_msg_get_initial(s);
    assert(m != NULL);

    conn_send("Hello again!");
    conn_shutdown(SHUT_WR);
    b = vlc_http_msg_read(m);
    assert(b != NULL);
    assert(b->i_buffer == 12);
    assert(!memcmp(b->p_buffer, "Hello again!", 12));
    block_Release(b);
    b = vlc_http_msg_read(m);
    assert(b == NULL);
    vlc_http_msg_destroy(m);
    conn_destroy();

    /* Test HTTP/1.1 with chunked transfer encoding */
    conn_create();
    s = stream_open();
    assert(s != NULL);
    conn_send("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
              "Content-Length: 1000000\r\n\r\n"); /* length must be ignored */
    m = vlc_http_msg_get_initial(s);
    assert(m != NULL);

    conn_send("C\r\nHello there!\r\n0\r\n\r\n");
    b = vlc_http_msg_read(m);
    assert(b != NULL);
    assert(b->i_buffer == 12);
    assert(!memcmp(b->p_buffer, "Hello there!", 12));
    block_Release(b);
    conn_destroy(); /* test connection release before closing stream */
    b = vlc_http_msg_read(m);
    assert(b == NULL);
    vlc_http_msg_destroy(m);

    /* Test HTTP/1.1 with content length */
    conn_create();
    s = stream_open();
    assert(s != NULL);
    conn_send("HTTP/1.1 200 OK\r\nContent-Length: 8\r\n\r\n");
    m = vlc_http_msg_get_initial(s);
    assert(m != NULL);

    conn_send("Bye bye!");
    b = vlc_http_msg_read(m);
    assert(b != NULL);
    assert(b->i_buffer == 8);
    assert(!memcmp(b->p_buffer, "Bye bye!", 8));
    block_Release(b);
    b = vlc_http_msg_read(m);
    assert(b == NULL);
    vlc_http_msg_destroy(m);
    conn_destroy();

    /* Test HTTP/1.1 with content length, shortened by error */
    conn_create();
    s = stream_open();
    assert(s != NULL);
    conn_send("HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\n");
    m = vlc_http_msg_get_initial(s);
    assert(m != NULL);

    conn_send("Hello ");
    b = vlc_http_msg_read(m);
    assert(b != NULL);
    assert(b->i_buffer == 6);
    assert(!memcmp(b->p_buffer, "Hello ", 6));
    block_Release(b);
    conn_shutdown(SHUT_RDWR);
    b = vlc_http_msg_read(m);
    assert(b == vlc_http_error);
    vlc_http_msg_destroy(m);
    conn_destroy();

    return 0;
}
