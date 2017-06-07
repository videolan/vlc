/*****************************************************************************
 * h2conn.c: HTTP/2 connection handling
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
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#ifdef HAVE_POLL
# include <poll.h>
#endif
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif
#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_interrupt.h>
#include <vlc_tls.h>

#include "h2frame.h"
#include "h2output.h"
#include "conn.h"
#include "message.h"

#define CO(c) ((c)->opaque)
#define SO(s) CO((s)->conn)

/** HTTP/2 connection */
struct vlc_h2_conn
{
    struct vlc_http_conn conn;
    struct vlc_h2_output *out; /**< Send thread */
    void *opaque;

    struct vlc_h2_stream *streams; /**< List of open streams */
    uint32_t next_id; /**< Next free stream identifier */
    bool released; /**< Connection released by owner */

    vlc_mutex_t lock; /**< State machine lock */
    vlc_thread_t thread; /**< Receive thread */
};

static void vlc_h2_conn_destroy(struct vlc_h2_conn *conn);

/** HTTP/2 stream */
struct vlc_h2_stream
{
    struct vlc_http_stream stream; /**< Base class */
    struct vlc_h2_conn *conn; /**< Underlying HTTP/2 connection */
    struct vlc_h2_stream *older; /**< Previous open stream in connection */
    struct vlc_h2_stream *newer; /**< Next open stream in connection */
    uint32_t id; /**< Stream 31-bits identifier */

    bool interrupted;
    bool recv_end; /**< End-of-stream flag */
    int recv_err; /**< Standard C error code */
    struct vlc_http_msg *recv_hdr; /**< Latest received headers (or NULL) */

    size_t recv_cwnd; /**< Free space in receive congestion window */
    struct vlc_h2_frame *recv_head; /**< Earliest pending received buffer */
    struct vlc_h2_frame **recv_tailp; /**< Tail of receive queue */
    vlc_cond_t recv_wait;
};

static int vlc_h2_conn_queue(struct vlc_h2_conn *conn, struct vlc_h2_frame *f)
{
    vlc_h2_frame_dump(conn->opaque, f, "out");
    return vlc_h2_output_send(conn->out, f);
}

static int vlc_h2_conn_queue_prio(struct vlc_h2_conn *conn,
                                  struct vlc_h2_frame *f)
{
    vlc_h2_frame_dump(conn->opaque, f, "out (priority)");
    return vlc_h2_output_send_prio(conn->out, f);
}


/* Stream callbacks */

/** Looks a stream up by ID. */
static void *vlc_h2_stream_lookup(void *ctx, uint_fast32_t id)
{
    struct vlc_h2_conn *conn = ctx;

    for (struct vlc_h2_stream *s = conn->streams; s != NULL; s = s->older)
        if (s->id == id)
            return s;
    return NULL;
}

/** Reports a local stream error */
static int vlc_h2_stream_error(void *ctx, uint_fast32_t id, uint_fast32_t code)
{
    struct vlc_h2_conn *conn = ctx;

    /* NOTE: This function is used both w/ and w/o conn->lock. Care. */
    if (code != VLC_H2_NO_ERROR)
        vlc_http_err(CO(conn), "local stream %"PRIuFAST32" error: "
                     "%s (0x%"PRIXFAST32")", id, vlc_h2_strerror(code), code);
    else
        vlc_http_dbg(CO(conn), "local stream %"PRIuFAST32" shut down", id);
    return vlc_h2_conn_queue(conn, vlc_h2_frame_rst_stream(id, code));
}

static int vlc_h2_stream_fatal(struct vlc_h2_stream *s, uint_fast32_t code)
{
    s->recv_end = true;
    s->recv_err = EPROTO;
    return vlc_h2_stream_error(s->conn, s->id, code);
}

/** Reports received stream headers */
static void vlc_h2_stream_headers(void *ctx, unsigned count,
                                  const char *const hdrs[][2])
{
    struct vlc_h2_stream *s = ctx;

    /* NOTE: HTTP message trailers are not supported so far. Follow-up headers
     * can therefore only be a final response after a 1xx continue response.
     * Then it is safe to discard the existing header. */
    if (s->recv_hdr != NULL)
    {
        vlc_http_dbg(SO(s), "stream %"PRIu32" discarding old headers", s->id);
        vlc_http_msg_destroy(s->recv_hdr);
        s->recv_hdr = NULL;
    }

    vlc_http_dbg(SO(s), "stream %"PRIu32" %u headers:", s->id, count);

    for (unsigned i = 0; i < count; i++)
        vlc_http_dbg(SO(s), " %s: \"%s\"", hdrs[i][0], hdrs[i][1]);

    s->recv_hdr = vlc_http_msg_h2_headers(count, hdrs);
    if (unlikely(s->recv_hdr == NULL))
        vlc_h2_stream_fatal(s, VLC_H2_PROTOCOL_ERROR);
    vlc_cond_signal(&s->recv_wait);
}

/** Reports received stream data */
static int vlc_h2_stream_data(void *ctx, struct vlc_h2_frame *f)
{
    struct vlc_h2_stream *s = ctx;
    size_t len;

    if (s->recv_end)
    {
        free(f);
        return vlc_h2_stream_error(s->conn, s->id, VLC_H2_STREAM_CLOSED);
    }

    /* Enforce the congestion window as required by the protocol spec */
    vlc_h2_frame_data_get(f, &len);
    if (len > s->recv_cwnd)
    {
        free(f);
        return vlc_h2_stream_fatal(s, VLC_H2_FLOW_CONTROL_ERROR);
    }
    s->recv_cwnd -= len;

    *(s->recv_tailp) = f;
    s->recv_tailp = &f->next;
    vlc_cond_signal(&s->recv_wait);
    return 0;
}

/** Reports received end of stream */
static void vlc_h2_stream_end(void *ctx)
{
    struct vlc_h2_stream *s = ctx;

    vlc_http_dbg(SO(s), "stream %"PRIu32" closed by peer", s->id);

    s->recv_end = true;
    vlc_cond_broadcast(&s->recv_wait);
}

/** Reports remote stream error */
static int vlc_h2_stream_reset(void *ctx, uint_fast32_t code)
{
    struct vlc_h2_stream *s = ctx;

    vlc_http_err(SO(s), "peer stream %"PRIu32" error: %s (0x%"PRIXFAST32")",
                 s->id, vlc_h2_strerror(code), code);

    s->recv_end = true;
    s->recv_err = ECONNRESET;
    vlc_cond_broadcast(&s->recv_wait);
    return 0;
}

static void vlc_h2_stream_wake_up(void *data)
{
    struct vlc_h2_stream *s = data;
    struct vlc_h2_conn *conn = s->conn;

    vlc_mutex_lock(&conn->lock);
    s->interrupted = true;
    vlc_cond_signal(&s->recv_wait);
    vlc_mutex_unlock(&conn->lock);
}

static void vlc_h2_stream_lock(struct vlc_h2_stream *s)
{
    s->interrupted = false;
    /* When using interrupts, there shall be only one waiter per stream.
     * Otherwise, there would be no ways to map the interrupt to a thread. */
    vlc_interrupt_register(vlc_h2_stream_wake_up, s);
    vlc_mutex_lock(&s->conn->lock);
}

static int vlc_h2_stream_unlock(struct vlc_h2_stream *s)
{
    vlc_mutex_unlock(&s->conn->lock);
    return vlc_interrupt_unregister();
}

static struct vlc_http_msg *vlc_h2_stream_wait(struct vlc_http_stream *stream)
{
    struct vlc_h2_stream *s =
        container_of(stream, struct vlc_h2_stream, stream);
    struct vlc_h2_conn *conn = s->conn;
    struct vlc_http_msg *m;

    vlc_h2_stream_lock(s);
    while ((m = s->recv_hdr) == NULL && !s->recv_end && !s->interrupted)
    {
        mutex_cleanup_push(&conn->lock);
        vlc_cond_wait(&s->recv_wait, &conn->lock);
        vlc_cleanup_pop();
    }
    s->recv_hdr = NULL;
    vlc_h2_stream_unlock(s);

    /* TODO? distinguish failed/unknown/ignored */
    if (m != NULL)
        vlc_http_msg_attach(m, stream);
    return m;
}

/**
 * Receives stream data.
 *
 * Dequeues pending incoming data for an HTTP/2 stream. If there is currently
 * no data block, wait for one.
 *
 * \return a VLC data block, NULL on end of stream,
 *         or vlc_http_error on stream error
 */
static block_t *vlc_h2_stream_read(struct vlc_http_stream *stream)
{
    struct vlc_h2_stream *s =
        container_of(stream, struct vlc_h2_stream, stream);
    struct vlc_h2_conn *conn = s->conn;
    struct vlc_h2_frame *f;

    vlc_h2_stream_lock(s);
    while ((f = s->recv_head) == NULL && !s->recv_end && !s->interrupted)
    {
        mutex_cleanup_push(&conn->lock);
        vlc_cond_wait(&s->recv_wait, &conn->lock);
        vlc_cleanup_pop();
    }

    if (f == NULL)
    {
        int err = s->recv_err;

        vlc_h2_stream_unlock(s);
        if (err)
        {
            errno = err;
            return vlc_http_error;
        }
        return NULL;
    }

    s->recv_head = f->next;
    if (f->next == NULL)
    {
        assert(s->recv_tailp == &f->next);
        s->recv_tailp = &s->recv_head;
    }

    /* Credit the receive window if missing credit exceeds 50%. */
    uint_fast32_t credit = VLC_H2_INIT_WINDOW - s->recv_cwnd;
    if (credit >= (VLC_H2_INIT_WINDOW / 2)
     && !vlc_h2_conn_queue(conn, vlc_h2_frame_window_update(s->id, credit)))
        s->recv_cwnd += credit;

    vlc_h2_stream_unlock(s);

    /* This, err, unconventional code to avoid copying data. */
    block_t *block = block_heap_Alloc(f, sizeof (*f) + vlc_h2_frame_size(f));
    if (unlikely(block == NULL))
    {
        vlc_h2_stream_error(conn, s->id, VLC_H2_INTERNAL_ERROR);
        return vlc_http_error;
    }

    size_t len;
    uint8_t *buf = vlc_h2_frame_data_get(f, &len);

    assert(block->i_buffer >= len);
    assert(block->p_buffer <= buf);
    assert(block->p_buffer + block->i_buffer >= buf + len);
    block->p_buffer = buf;
    block->i_buffer = len;
    return block;
}

/**
 * Terminates a stream.
 *
 * Sends an HTTP/2 stream reset, removes the stream from the HTTP/2 connection
 * and deletes any stream resource.
 */
static void vlc_h2_stream_close(struct vlc_http_stream *stream, bool aborted)
{
    struct vlc_h2_stream *s =
        container_of(stream, struct vlc_h2_stream, stream);
    struct vlc_h2_conn *conn = s->conn;
    bool destroy = false;
    uint_fast32_t code = VLC_H2_NO_ERROR;

    vlc_mutex_lock(&conn->lock);
    if (s->older != NULL)
        s->older->newer = s->newer;
    if (s->newer != NULL)
        s->newer->older = s->older;
    else
    {
        assert(conn->streams == s);
        conn->streams = s->older;
        destroy = (conn->streams == NULL) && conn->released;
    }
    vlc_mutex_unlock(&conn->lock);

    if (s->recv_hdr != NULL || s->recv_head != NULL || !s->recv_end)
        code = VLC_H2_CANCEL;
    (void) aborted;

    vlc_h2_stream_error(conn, s->id, code);

    if (s->recv_hdr != NULL)
        vlc_http_msg_destroy(s->recv_hdr);

    for (struct vlc_h2_frame *f = s->recv_head, *next; f != NULL; f = next)
    {
        next = f->next;
        free(f);
    }

    vlc_cond_destroy(&s->recv_wait);
    free(s);

    if (destroy)
        vlc_h2_conn_destroy(conn);
}

static const struct vlc_http_stream_cbs vlc_h2_stream_callbacks =
{
    vlc_h2_stream_wait,
    vlc_h2_stream_read,
    vlc_h2_stream_close,
};

/**
 * Creates a stream.
 *
 * Allocates a locally-initiated stream identifier on an HTTP/2 connection and
 * queue stream headers for sending.
 *
 * Headers are sent asynchronously. To obtain the result and answer from the
 * other end, use vlc_http_stream_recv_headers().
 *
 * \param msg HTTP message headers (including response status or request)
 * \return an HTTP stream, or NULL on error
 */
static struct vlc_http_stream *vlc_h2_stream_open(struct vlc_http_conn *c,
                                                const struct vlc_http_msg *msg)
{
    struct vlc_h2_conn *conn = container_of(c, struct vlc_h2_conn, conn);
    struct vlc_h2_stream *s = malloc(sizeof (*s));
    if (unlikely(s == NULL))
        return NULL;

    s->stream.cbs = &vlc_h2_stream_callbacks;
    s->conn = conn;
    s->newer = NULL;
    s->recv_end = false;
    s->recv_err = 0;
    s->recv_hdr = NULL;
    s->recv_cwnd = VLC_H2_INIT_WINDOW;
    s->recv_head = NULL;
    s->recv_tailp = &s->recv_head;
    vlc_cond_init(&s->recv_wait);

    vlc_mutex_lock(&conn->lock);
    assert(!conn->released); /* Caller is buggy! */

    if (conn->next_id > 0x7ffffff)
    {   /* Out of stream identifiers */
        vlc_http_dbg(CO(conn), "no more stream identifiers");
        goto error;
    }

    s->id = conn->next_id;
    conn->next_id += 2;

    struct vlc_h2_frame *f = vlc_http_msg_h2_frame(msg, s->id, true);
    if (f == NULL)
        goto error;

    vlc_h2_conn_queue(conn, f);

    s->older = conn->streams;
    if (s->older != NULL)
        s->older->newer = s;
    conn->streams = s;
    vlc_mutex_unlock(&conn->lock);
    return &s->stream;

error:
    vlc_mutex_unlock(&conn->lock);
    vlc_cond_destroy(&s->recv_wait);
    free(s);
    return NULL;
}

/* Global/Connection frame callbacks */

/** Reports an HTTP/2 peer connection setting */
static void vlc_h2_setting(void *ctx, uint_fast16_t id, uint_fast32_t value)
{
    struct vlc_h2_conn *conn = ctx;

    vlc_http_dbg(CO(conn), "setting: %s (0x%04"PRIxFAST16"): %"PRIuFAST32,
                 vlc_h2_setting_name(id), id, value);
}

/** Reports end of HTTP/2 peer settings */
static int vlc_h2_settings_done(void *ctx)
{
    struct vlc_h2_conn *conn = ctx;

    return vlc_h2_conn_queue(conn, vlc_h2_frame_settings_ack());
}

/** Reports a ping received from HTTP/2 peer */
static int vlc_h2_ping(void *ctx, uint_fast64_t opaque)
{
    struct vlc_h2_conn *conn = ctx;

    return vlc_h2_conn_queue_prio(conn, vlc_h2_frame_pong(opaque));
}

/** Reports a local HTTP/2 connection failure */
static void vlc_h2_error(void *ctx, uint_fast32_t code)
{
    struct vlc_h2_conn *conn = ctx;

    /* NOTE: This function is used both w/ and w/o conn->lock. Care. */
    if (code != VLC_H2_NO_ERROR)
        vlc_http_err(CO(conn), "local error: %s (0x%"PRIxFAST32")",
                     vlc_h2_strerror(code), code);
    else
        vlc_http_dbg(CO(conn), "local shutdown");
    /* NOTE: currently, the peer cannot create a stream, so ID=0 */
    vlc_h2_conn_queue(conn, vlc_h2_frame_goaway(0, code));
}

/** Reports a remote HTTP/2 connection error */
static int vlc_h2_reset(void *ctx, uint_fast32_t last_seq, uint_fast32_t code)
{
    struct vlc_h2_conn *conn = ctx;

    vlc_http_err(CO(conn), "peer error: %s (0x%"PRIxFAST32")",
                 vlc_h2_strerror(code), code);
    vlc_http_dbg(CO(conn), "last stream: %"PRIuFAST32, last_seq);

    /* NOTE: currently, the peer cannot create a stream, so ID=0 */
    vlc_h2_conn_queue(conn, vlc_h2_frame_goaway(0, VLC_H2_NO_ERROR));

    /* Prevent adding new streams on this end. */
    conn->next_id = 0x80000000;

    /* Reject any stream newer than last_seq */
    for (struct vlc_h2_stream *s = conn->streams; s != NULL; s = s->older)
        if (s->id > last_seq)
            vlc_h2_stream_reset(s, VLC_H2_REFUSED_STREAM);

    return 0;
}

static void vlc_h2_window_status(void *ctx, uint32_t *restrict rcwd)
{
    struct vlc_h2_conn *conn = ctx;

    /* Maintain connection receive window to insanely large values.
     * Congestion control is done per stream instead. */
    if (*rcwd < (1 << 30)
     && vlc_h2_conn_queue_prio(conn,
                               vlc_h2_frame_window_update(0, 1 << 30)) == 0)
        *rcwd += 1 << 30;
}

/** HTTP/2 frames parser callbacks table */
static const struct vlc_h2_parser_cbs vlc_h2_parser_callbacks =
{
    vlc_h2_setting,
    vlc_h2_settings_done,
    vlc_h2_ping,
    vlc_h2_error,
    vlc_h2_reset,
    vlc_h2_window_status,
    vlc_h2_stream_lookup,
    vlc_h2_stream_error,
    vlc_h2_stream_headers,
    vlc_h2_stream_data,
    vlc_h2_stream_end,
    vlc_h2_stream_reset,
};

/**
 * Receives TLS data.
 *
 * Receives bytes from the peer through a TLS session.
 * @note This may be a cancellation point.
 * The caller is responsible for serializing reads on a given connection.
 */
static ssize_t vlc_https_recv(vlc_tls_t *tls, void *buf, size_t len)
{
    struct pollfd ufd;
    struct iovec iov;
    size_t count = 0;

    ufd.fd = vlc_tls_GetFD(tls);
    ufd.events = POLLIN;
    iov.iov_base = buf;
    iov.iov_len = len;

    while (iov.iov_len > 0)
    {
        int canc = vlc_savecancel();
        ssize_t val = tls->readv(tls, &iov, 1);

        vlc_restorecancel(canc);

        if (val == 0)
            break;

        if (val >= 0)
        {
            iov.iov_base = (char *)iov.iov_base + val;
            iov.iov_len -= val;
            count += val;
            continue;
        }

        if (errno != EINTR && errno != EAGAIN)
            return count ? (ssize_t)count : -1;

        poll(&ufd, 1, -1);
    }

    return count;
}

/**
 * Receives an HTTP/2 frame through TLS.
 *
 * This function allocates memory for and receives a whole HTTP/2 frame from a
 * TLS session.
 *
 * The caller must "own" the read side of the TLS session.
 *
 * @note This is a blocking function and may be a thread cancellation point.
 *
 * @return a frame or NULL if the connection failed
 */
static struct vlc_h2_frame *vlc_h2_frame_recv(struct vlc_tls *tls)
{
    uint8_t header[9];
    ssize_t r = vlc_https_recv(tls, header, 9);
    /* TODO: actually block only until third byte */
    if (r < 3)
        return NULL;

    uint_fast32_t len = 9 + ((header[0] << 16) | (header[1] << 8) | header[2]);

    struct vlc_h2_frame *f = malloc(sizeof (*f) + len);
    if (unlikely(f == NULL))
        return NULL;

    f->next = NULL;
    memcpy(f->data, header, r);
    len -= r;

    if (len > 0)
    {
        vlc_cleanup_push(free, f);
        if (vlc_https_recv(tls, f->data + r, len) < (ssize_t)len)
        {
            free(f);
            f = NULL;
        }
        vlc_cleanup_pop();
    }
    return f;
}

static void cleanup_parser(void *data)
{
    vlc_h2_parse_destroy(data);
}

/** HTTP/2 receive thread */
static void *vlc_h2_recv_thread(void *data)
{
    struct vlc_h2_conn *conn = data;
    struct vlc_h2_frame *frame;
    struct vlc_h2_parser *parser;
    int canc, val;

    canc = vlc_savecancel();
    parser = vlc_h2_parse_init(conn, &vlc_h2_parser_callbacks);
    if (unlikely(parser == NULL))
        goto fail;

    vlc_cleanup_push(cleanup_parser, parser);
    do
    {
        vlc_restorecancel(canc);
        frame = vlc_h2_frame_recv(conn->conn.tls);
        canc = vlc_savecancel();

        if (frame == NULL)
        {
            vlc_http_dbg(CO(conn), "connection shutdown");
            break;
        }

        vlc_h2_frame_dump(CO(conn), frame, "in");
        vlc_mutex_lock(&conn->lock);
        val = vlc_h2_parse(parser, frame);
        vlc_mutex_unlock(&conn->lock);
    }
    while (val == 0);

    vlc_cleanup_pop();
    vlc_h2_parse_destroy(parser);
fail:
    /* Terminate any remaining stream */
    for (struct vlc_h2_stream *s = conn->streams; s != NULL; s = s->older)
        vlc_h2_stream_reset(s, VLC_H2_CANCEL);
    return NULL;
}

static void vlc_h2_conn_destroy(struct vlc_h2_conn *conn)
{
    assert(conn->streams == NULL);

    vlc_h2_error(conn, VLC_H2_NO_ERROR);

    vlc_cancel(conn->thread);
    vlc_join(conn->thread, NULL);
    vlc_mutex_destroy(&conn->lock);

    vlc_h2_output_destroy(conn->out);
    vlc_tls_Shutdown(conn->conn.tls, true);

    vlc_tls_Close(conn->conn.tls);
    free(conn);
}

static void vlc_h2_conn_release(struct vlc_http_conn *c)
{
    struct vlc_h2_conn *conn = container_of(c, struct vlc_h2_conn, conn);
    bool destroy;

    vlc_mutex_lock(&conn->lock);
    assert(!conn->released);

    conn->released = true;
    destroy = (conn->streams == NULL);
    vlc_mutex_unlock(&conn->lock);

    if (destroy)
        vlc_h2_conn_destroy(conn);
}

static const struct vlc_http_conn_cbs vlc_h2_conn_callbacks =
{
    vlc_h2_stream_open,
    vlc_h2_conn_release,
};

struct vlc_http_conn *vlc_h2_conn_create(void *ctx, struct vlc_tls *tls)
{
    struct vlc_h2_conn *conn = malloc(sizeof (*conn));
    if (unlikely(conn == NULL))
        return NULL;

    conn->conn.cbs = &vlc_h2_conn_callbacks;
    conn->conn.tls = tls;
    conn->out = vlc_h2_output_create(tls, true);
    conn->opaque = ctx;
    conn->streams = NULL;
    conn->next_id = 1; /* TODO: server side */
    conn->released = false;

    if (unlikely(conn->out == NULL))
        goto error;

    vlc_mutex_init(&conn->lock);

    if (vlc_h2_conn_queue(conn, vlc_h2_frame_settings())
     || vlc_clone(&conn->thread, vlc_h2_recv_thread, conn,
                  VLC_THREAD_PRIORITY_INPUT))
    {
        vlc_mutex_destroy(&conn->lock);
        vlc_h2_output_destroy(conn->out);
        goto error;
    }
    return &conn->conn;
error:
    free(conn);
    return NULL;
}
