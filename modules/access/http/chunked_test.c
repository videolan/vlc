/*****************************************************************************
 * chunked_test.c: HTTP 1.1 chunked encoding decoder test
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
#include <stddef.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_tls.h>
#include <vlc_block.h>
#include "conn.h"
#include "message.h"

/* I/O callbacks */
static const char *stream_content;
static size_t stream_length;
static bool stream_bad;

static int fd_callback(struct vlc_tls *tls)
{
    (void) tls;
    return -1;
}

static ssize_t recv_callback(struct vlc_tls *tls, struct iovec *iov,
                             unsigned count)
{
    size_t rcvd = 0;

    while (count > 0)
    {
        size_t copy = iov->iov_len;
        if (copy > stream_length)
            copy = stream_length;

        if (copy > 0)
        {
            memcpy(iov->iov_base, stream_content, copy);
            stream_content += copy;
            stream_length -= copy;
            rcvd += copy;
        }

        iov++;
        count--;
    }
    (void) tls;
    return rcvd;
}

static void close_callback(struct vlc_tls *tls)
{
    (void) tls;
}

static struct vlc_tls chunked_tls =
{
    .get_fd = fd_callback,
    .readv = recv_callback,
    .close = close_callback,
};

static void stream_close_callback(struct vlc_http_stream *stream, bool bad)
{
    (void) stream;
    assert(bad == stream_bad);
}

static const struct vlc_http_stream_cbs chunked_stream_cbs =
{
    .close = stream_close_callback,
};

static struct vlc_http_stream chunked_stream =
{
    &chunked_stream_cbs,
};

/* Test cases */

static void test_good(void)
{
    struct vlc_http_stream *s;
    block_t *b;

    /* Simple good payload */
    stream_content =
        "A\r\n" "1234567890\r\n"
        "1A; ext-foo=1\r\n" "abcdefghijklmnopqrstuvwxyz\r\n"
        "0\r\n" "\r\n";
    stream_length = strlen(stream_content);
    stream_bad = false;

    s = vlc_chunked_open(&chunked_stream, &chunked_tls);
    assert(s != NULL);
    assert(vlc_http_stream_read_headers(s) == NULL);

    b = vlc_http_stream_read(s);
    assert(b != NULL);
    assert(b->i_buffer == 10);
    assert(!memcmp(b->p_buffer, "1234567890", 10));
    block_Release(b);

    b = vlc_http_stream_read(s);
    assert(b != NULL);
    assert(b->i_buffer == 26);
    assert(!memcmp(b->p_buffer, "abcdefghijklmnopqrstuvwxyz", 26));
    block_Release(b);

    b = vlc_http_stream_read(s);
    assert(b == NULL);
    b = vlc_http_stream_read(s);
    assert(b == NULL);

    vlc_http_stream_close(s, false);
}

static void test_empty(void)
{
    struct vlc_http_stream *s;
    block_t *b;

    stream_content = "0\r\n";
    stream_length = 3;
    stream_bad = true;

    s = vlc_chunked_open(&chunked_stream, &chunked_tls);
    assert(s != NULL);

    b = vlc_http_stream_read(s);
    assert(b == NULL);
    b = vlc_http_stream_read(s);
    assert(b == NULL);
    vlc_http_stream_close(s, false);
}

static void test_bad(const char *payload)
{
    struct vlc_http_stream *s;
    block_t *b;

    stream_content = payload;
    stream_length = strlen(payload);
    stream_bad = true;

    s = vlc_chunked_open(&chunked_stream, &chunked_tls);
    assert(s != NULL);

    while ((b = vlc_http_stream_read(s)) != vlc_http_error)
    {
        assert(b != NULL);
        block_Release(b);
    }

    vlc_http_stream_close(s, false);
}

int main(void)
{
    test_good();
    test_empty();
    test_bad("");
    test_bad("A\r\n" "123456789");
    test_bad("Z\r\n" "123456789");

    return 0;
}
