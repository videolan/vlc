/*****************************************************************************
 * h2frame.c: HTTP/2 frame formatting
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <vlc_common.h>

#include "conn.h"
#include "hpack.h"
#include "h2frame.h"

static struct vlc_h2_frame *
vlc_h2_frame_alloc(uint_fast8_t type, uint_fast8_t flags,
                   uint_fast32_t stream_id, size_t length)
{
    assert((stream_id >> 31) == 0);

    if (unlikely(length >= (1u << 24)))
    {
        errno = EINVAL;
        return NULL;
    }

    struct vlc_h2_frame *f = malloc(sizeof (*f) + 9 + length);
    if (unlikely(f == NULL))
        return NULL;

    f->next = NULL;
    f->data[0] = length >> 16;
    f->data[1] = length >> 8;
    f->data[2] = length;
    f->data[3] = type;
    f->data[4] = flags;
    SetDWBE(f->data + 5, stream_id);
    return f;
}

#define vlc_h2_frame_payload(f) ((f)->data + 9)

static uint_fast32_t vlc_h2_frame_length(const struct vlc_h2_frame *f)
{
    const uint8_t *buf = f->data;
    return (buf[0] << 16) | (buf[1] << 8) | buf[2];
}

size_t vlc_h2_frame_size(const struct vlc_h2_frame *f)
{
    return 9 + vlc_h2_frame_length(f);
}

static uint_fast8_t vlc_h2_frame_type(const struct vlc_h2_frame *f)
{
    return f->data[3];
}

static uint_fast8_t vlc_h2_frame_flags(const struct vlc_h2_frame *f)
{
    return f->data[4];
}

static uint_fast32_t vlc_h2_frame_id(const struct vlc_h2_frame *f)
{
    return GetDWBE(f->data + 5) & 0x7FFFFFFF;
}

enum {
    VLC_H2_FRAME_DATA,
    VLC_H2_FRAME_HEADERS,
    VLC_H2_FRAME_PRIORITY,
    VLC_H2_FRAME_RST_STREAM,
    VLC_H2_FRAME_SETTINGS,
    VLC_H2_FRAME_PUSH_PROMISE,
    VLC_H2_FRAME_PING,
    VLC_H2_FRAME_GOAWAY,
    VLC_H2_FRAME_WINDOW_UPDATE,
    VLC_H2_FRAME_CONTINUATION,
};

static const char *vlc_h2_type_name(uint_fast8_t type)
{
    static const char names[][14] = {
        [VLC_H2_FRAME_DATA]          = "DATA",
        [VLC_H2_FRAME_HEADERS]       = "HEADERS",
        [VLC_H2_FRAME_PRIORITY]      = "PRIORITY",
        [VLC_H2_FRAME_RST_STREAM]    = "RST_STREAM",
        [VLC_H2_FRAME_SETTINGS]      = "SETTINGS",
        [VLC_H2_FRAME_PUSH_PROMISE]  = "PUSH_PROMISE",
        [VLC_H2_FRAME_PING]          = "PING",
        [VLC_H2_FRAME_GOAWAY]        = "GOAWAY",
        [VLC_H2_FRAME_WINDOW_UPDATE] = "WINDOW_UPDATE",
        [VLC_H2_FRAME_CONTINUATION]  = "CONTINUATION",
    };

    if (type >= (sizeof (names) / sizeof (names[0])) || names[type][0] == '\0')
        return "<unknown>";
    return names[type];
}

enum {
    VLC_H2_DATA_END_STREAM = 0x01,
    VLC_H2_DATA_PADDED     = 0x08,
};

enum {
    VLC_H2_HEADERS_END_STREAM  = 0x01,
    VLC_H2_HEADERS_END_HEADERS = 0x04,
    VLC_H2_HEADERS_PADDED      = 0x08,
    VLC_H2_HEADERS_PRIORITY    = 0x20,
};

enum {
    VLC_H2_SETTINGS_ACK = 0x01,
};

enum {
    VLC_H2_PUSH_PROMISE_END_HEADERS = 0x04,
    VLC_H2_PUSH_PROMISE_PADDED      = 0x08,
};

enum {
    VLC_H2_PING_ACK = 0x01,
};

enum {
    VLC_H2_CONTINUATION_END_HEADERS = 0x04,
};

struct vlc_h2_frame *
vlc_h2_frame_headers(uint_fast32_t stream_id, uint_fast32_t mtu, bool eos,
                     unsigned count, const char *const headers[][2])
{
    struct vlc_h2_frame *f;
    uint8_t flags = eos ? VLC_H2_HEADERS_END_STREAM : 0;

    size_t len = hpack_encode(NULL, 0, headers, count);

    if (likely(len <= mtu))
    {   /* Most common case: single frame - with zero copy */
        flags |= VLC_H2_HEADERS_END_HEADERS;

        f = vlc_h2_frame_alloc(VLC_H2_FRAME_HEADERS, flags, stream_id, len);
        if (unlikely(f == NULL))
            return NULL;

        hpack_encode(vlc_h2_frame_payload(f), len, headers, count);
        return f;
    }

    /* Edge case: HEADERS frame then CONTINUATION frame(s) */
    uint8_t *payload = malloc(len);
    if (unlikely(payload == NULL))
        return NULL;

    hpack_encode(payload, len, headers, count);

    struct vlc_h2_frame **pp = &f, *n;
    const uint8_t *offset = payload;
    uint_fast8_t type = VLC_H2_FRAME_HEADERS;

    f = NULL;

    while (len > mtu)
    {
        n = vlc_h2_frame_alloc(type, flags, stream_id, mtu);
        if (unlikely(n == NULL))
            goto error;

        memcpy(vlc_h2_frame_payload(n), offset, mtu);
        *pp = n;
        pp = &n->next;

        type = VLC_H2_FRAME_CONTINUATION;
        flags = 0;
        offset += mtu;
        len -= mtu;
    }

    flags |= VLC_H2_CONTINUATION_END_HEADERS;

    n = vlc_h2_frame_alloc(type, flags, stream_id, len);
    if (unlikely(n == NULL))
        goto error;

    memcpy(vlc_h2_frame_payload(n), offset, len);
    *pp = n;

    free(payload);
    return f;

error:
    while (f != NULL)
    {
        n = f->next;
        free(f);
        f = n;
    }
    free(payload);
    return NULL;
}

struct vlc_h2_frame *
vlc_h2_frame_data(uint_fast32_t stream_id, const void *buf, size_t len,
                  bool eos)
{
    struct vlc_h2_frame *f;
    uint8_t flags = eos ? VLC_H2_DATA_END_STREAM : 0;

    f = vlc_h2_frame_alloc(VLC_H2_FRAME_DATA, flags, stream_id, len);
    if (likely(f != NULL))
        memcpy(vlc_h2_frame_payload(f), buf, len);
    return f;
}

struct vlc_h2_frame *
vlc_h2_frame_rst_stream(uint_fast32_t stream_id, uint_fast32_t error_code)
{
    struct vlc_h2_frame *f = vlc_h2_frame_alloc(VLC_H2_FRAME_RST_STREAM, 0,
                                                stream_id, 4);
    if (likely(f != NULL))
        SetDWBE(vlc_h2_frame_payload(f), error_code);
    return f;
}

struct vlc_h2_frame *vlc_h2_frame_settings(void)
{
    unsigned n = (VLC_H2_MAX_HEADER_TABLE != VLC_H2_DEFAULT_MAX_HEADER_TABLE)
               + 1 /* ENABLE_PUSH */
#if defined(VLC_H2_MAX_STREAMS)
               + 1
#endif
               + (VLC_H2_INIT_WINDOW != VLC_H2_DEFAULT_INIT_WINDOW)
               + (VLC_H2_MAX_FRAME != VLC_H2_DEFAULT_MAX_FRAME)
#if defined(VLC_H2_MAX_HEADER_LIST)
               + 1
#endif
    ;
    struct vlc_h2_frame *f = vlc_h2_frame_alloc(VLC_H2_FRAME_SETTINGS, 0, 0,
                                                n * 6);
    if (unlikely(f == NULL))
        return NULL;

    uint8_t *p = vlc_h2_frame_payload(f);

#if (VLC_H2_MAX_HEADER_TABLE != VLC_H2_DEFAULT_MAX_HEADER_TABLE)
    SetWBE(p, VLC_H2_SETTING_HEADER_TABLE_SIZE);
    SetDWBE(p + 2, VLC_H2_MAX_HEADER_TABLE);
    p += 6;
#endif

    SetWBE(p, VLC_H2_SETTING_ENABLE_PUSH);
    SetDWBE(p + 2, 0);
    p += 6;

#if defined(VLC_H2_MAX_STREAMS)
    SetWBE(p, VLC_H2_SETTING_MAX_CONCURRENT_STREAMS);
    SetDWBE(p + 2, VLC_H2_MAX_STREAMS);
    p += 6;
#endif

#if (VLC_H2_INIT_WINDOW != VLC_H2_DEFAULT_INIT_WINDOW)
# if (VLC_H2_INIT_WINDOW > 2147483647)
#  error Illegal initial window value
# endif
    SetWBE(p, VLC_H2_SETTING_INITIAL_WINDOW_SIZE);
    SetDWBE(p + 2, VLC_H2_INIT_WINDOW);
    p += 6;
#endif

#if (VLC_H2_MAX_FRAME != VLC_H2_DEFAULT_MAX_FRAME)
# if (VLC_H2_MAX_FRAME < 16384 || VLC_H2_MAX_FRAME > 16777215)
#  error Illegal maximum frame size
# endif
    SetWBE(p, VLC_H2_SETTING_MAX_FRAME_SIZE);
    SetDWBE(p + 2, VLC_H2_MAX_FRAME);
    p += 6;
#endif

#if defined(VLC_H2_MAX_HEADER_LIST)
    SetWBE(p, VLC_H2_SETTING_MAX_HEADER_LIST_SIZE);
    SetDWBE(p + 2, VLC_H2_MAX_HEADER_LIST);
    p += 6;
#endif

    return f;
}

struct vlc_h2_frame *vlc_h2_frame_settings_ack(void)
{
    return vlc_h2_frame_alloc(VLC_H2_FRAME_SETTINGS, VLC_H2_SETTINGS_ACK, 0,
                              0);
}

const char *vlc_h2_setting_name(uint_fast16_t id)
{
    static const char names[][20] = {
        [0]                                     = "Unknown setting",
        [VLC_H2_SETTING_HEADER_TABLE_SIZE]      = "Header table size",
        [VLC_H2_SETTING_ENABLE_PUSH]            = "Enable push",
        [VLC_H2_SETTING_MAX_CONCURRENT_STREAMS] = "Concurrent streams",
        [VLC_H2_SETTING_INITIAL_WINDOW_SIZE]    = "Initial window size",
        [VLC_H2_SETTING_MAX_FRAME_SIZE]         = "Frame size",
        [VLC_H2_SETTING_MAX_HEADER_LIST_SIZE]   = "Header list size",
    };

    if (id >= sizeof (names) / sizeof (names[0]) || names[id][0] == '\0')
        id = 0;
    return names[id];
}

struct vlc_h2_frame *vlc_h2_frame_ping(uint64_t opaque)
{
    struct vlc_h2_frame *f = vlc_h2_frame_alloc(VLC_H2_FRAME_PING, 0, 0, 8);
    if (likely(f != NULL))
        memcpy(vlc_h2_frame_payload(f), &opaque, 8);
    return f;
}

struct vlc_h2_frame *vlc_h2_frame_pong(uint64_t opaque)
{
    struct vlc_h2_frame *f = vlc_h2_frame_alloc(VLC_H2_FRAME_PING,
                                                VLC_H2_PING_ACK, 0, 8);
    if (likely(f != NULL))
        memcpy(vlc_h2_frame_payload(f), &opaque, 8);
    return f;
}

struct vlc_h2_frame *
vlc_h2_frame_goaway(uint_fast32_t last_stream_id, uint_fast32_t error_code)
{
    struct vlc_h2_frame *f = vlc_h2_frame_alloc(VLC_H2_FRAME_GOAWAY, 0, 0, 8);
    if (likely(f != NULL))
    {
        uint8_t *p = vlc_h2_frame_payload(f);

        SetDWBE(p, last_stream_id);
        SetDWBE(p + 4, error_code);
    }
    return f;
}

struct vlc_h2_frame *
vlc_h2_frame_window_update(uint_fast32_t stream_id, uint_fast32_t credit)
{
    assert((stream_id >> 31) == 0);

    struct vlc_h2_frame *f = vlc_h2_frame_alloc(VLC_H2_FRAME_WINDOW_UPDATE,
                                                0, stream_id, 4);
    if (likely(f != NULL))
    {
        uint8_t *p = vlc_h2_frame_payload(f);

        SetDWBE(p, credit);
    }
    return f;
}

const char *vlc_h2_strerror(uint_fast32_t code)
{
    static const char names[][20] = {
        [VLC_H2_NO_ERROR]            = "No error",
        [VLC_H2_PROTOCOL_ERROR]      = "Protocol error",
        [VLC_H2_INTERNAL_ERROR]      = "Internal error",
        [VLC_H2_FLOW_CONTROL_ERROR]  = "Flow control error",
        [VLC_H2_SETTINGS_TIMEOUT]    = "Settings time-out",
        [VLC_H2_STREAM_CLOSED]       = "Stream closed",
        [VLC_H2_FRAME_SIZE_ERROR]    = "Frame size error",
        [VLC_H2_REFUSED_STREAM]      = "Refused stream",
        [VLC_H2_CANCEL]              = "Cancellation",
        [VLC_H2_COMPRESSION_ERROR]   = "Compression error",
        [VLC_H2_CONNECT_ERROR]       = "CONNECT error",
        [VLC_H2_ENHANCE_YOUR_CALM]   = "Excessive load",
        [VLC_H2_INADEQUATE_SECURITY] = "Inadequate security",
        [VLC_H2_HTTP_1_1_REQUIRED]   = "Required HTTP/1.1",
    };

    if (code >= sizeof (names) / sizeof (names[0]) || names[code][0] == '\0')
        return "Unknown error";
    return names[code];
}

void vlc_h2_frame_dump(void *opaque, const struct vlc_h2_frame *f,
                       const char *msg)
{
    size_t len = vlc_h2_frame_length(f);
    uint_fast8_t type = vlc_h2_frame_type(f);
    uint_fast8_t flags = vlc_h2_frame_flags(f);
    uint_fast32_t sid = vlc_h2_frame_id(f);

    if (sid != 0)
        vlc_http_dbg(opaque, "%s %s (0x%02"PRIxFAST8") frame of %zu bytes, "
                     "flags 0x%02"PRIxFAST8", stream %"PRIuFAST32, msg,
                     vlc_h2_type_name(type), type, len,  flags, sid);
    else
        vlc_http_dbg(opaque, "%s %s (0x%02"PRIxFAST8") frame of %zu bytes, "
                     "flags 0x%02"PRIxFAST8", global", msg,
                     vlc_h2_type_name(type), type, len,  flags);
}

const uint8_t *(vlc_h2_frame_data_get)(const struct vlc_h2_frame *f,
                                       size_t *restrict lenp)
{
    assert(vlc_h2_frame_type(f) == VLC_H2_FRAME_DATA);

    size_t len = vlc_h2_frame_length(f);
    uint_fast8_t flags = vlc_h2_frame_flags(f);
    const uint8_t *ptr = vlc_h2_frame_payload(f);

    /* At this point, the frame has already been validated by the parser. */
    if (flags & VLC_H2_DATA_PADDED)
    {
        assert(len >= 1u && len >= 1u + ptr[0]);
        len -= 1u + *(ptr++);
    }

    *lenp = len;
    return ptr;
}

typedef int (*vlc_h2_parser)(struct vlc_h2_parser *, struct vlc_h2_frame *,
                             size_t, uint_fast32_t);

/** HTTP/2 incoming frames parser */
struct vlc_h2_parser
{
    void *opaque;
    const struct vlc_h2_parser_cbs *cbs;

    vlc_h2_parser parser; /*< Parser state / callback for next frame */

    struct
    {
        uint32_t sid; /*< Ongoing stream identifier */
        bool eos; /*< End of stream after headers block */
        size_t len; /*< Compressed headers buffer length */
        uint8_t *buf; /*< Compressed headers buffer base address */
        struct hpack_decoder *decoder; /*< HPACK decompressor state */
    } headers; /*< Compressed headers reception state */

    uint32_t rcwd_size; /*< Receive congestion window (bytes) */
};

static int vlc_h2_parse_generic(struct vlc_h2_parser *, struct vlc_h2_frame *,
                                size_t, uint_fast32_t);
static int vlc_h2_parse_headers_block(struct vlc_h2_parser *,
                                      struct vlc_h2_frame *, size_t,
                                      uint_fast32_t);

static int vlc_h2_parse_error(struct vlc_h2_parser *p, uint_fast32_t code)
{
    p->cbs->error(p->opaque, code);
    return -1;
}

static int vlc_h2_stream_error(struct vlc_h2_parser *p, uint_fast32_t id,
                               uint_fast32_t code)
{
    return p->cbs->stream_error(p->opaque, id, code);
}

static void *vlc_h2_stream_lookup(struct vlc_h2_parser *p, uint_fast32_t id)
{
    return p->cbs->stream_lookup(p->opaque, id);
}

static void vlc_h2_parse_headers_start(struct vlc_h2_parser *p,
                                       uint_fast32_t sid, bool eos)
{
    assert(sid != 0);
    assert(p->headers.sid == 0);

    p->parser = vlc_h2_parse_headers_block;
    p->headers.sid = sid;
    p->headers.eos = eos;
    p->headers.len = 0;
}

static int vlc_h2_parse_headers_append(struct vlc_h2_parser *p,
                                       const uint8_t *data, size_t len)
{
    assert(p->headers.sid != 0);

    if (p->headers.len + len > 65536)
        return vlc_h2_parse_error(p, VLC_H2_INTERNAL_ERROR);

    uint8_t *buf = realloc(p->headers.buf, p->headers.len + len);
    if (unlikely(buf == NULL))
        return vlc_h2_parse_error(p, VLC_H2_INTERNAL_ERROR);

    p->headers.buf = buf;
    memcpy(p->headers.buf + p->headers.len, data, len);
    p->headers.len += len;
    return 0;
}

static int vlc_h2_parse_headers_end(struct vlc_h2_parser *p)
{
    char *headers[VLC_H2_MAX_HEADERS][2];

    /* TODO: limit total decompressed size of the headers list */
    int n = hpack_decode(p->headers.decoder, p->headers.buf, p->headers.len,
                         headers, VLC_H2_MAX_HEADERS);
    if (n > VLC_H2_MAX_HEADERS)
    {
        for (unsigned i = 0; i < VLC_H2_MAX_HEADERS; i++)
        {
            free(headers[i][0]);
            free(headers[i][1]);
        }
        n = -1;
    }
    if (n < 0)
        return vlc_h2_parse_error(p, VLC_H2_COMPRESSION_ERROR);

    void *s = vlc_h2_stream_lookup(p, p->headers.sid);
    int val = 0;

    if (s != NULL)
    {
        const char *ch[n ? n : 1][2];

        for (int i = 0; i < n; i++)
            ch[i][0] = headers[i][0], ch[i][1] = headers[i][1];

        p->cbs->stream_headers(s, n, ch);

        if (p->headers.eos)
            p->cbs->stream_end(s);
    }
    else
        /* NOTE: The specification implies that the error should be sent for
         * the first header frame. But we actually want to receive the whole
         * fragmented headers block, to preserve the HPACK decoder state.
         * So we send the error at the last header frame instead. */
        val = vlc_h2_stream_error(p, p->headers.sid, VLC_H2_REFUSED_STREAM);

    for (int i = 0; i < n; i++)
    {
        free(headers[i][0]);
        free(headers[i][1]);
    }

    p->parser = vlc_h2_parse_generic;
    p->headers.sid = 0;
    return val;
}

/** Parses an HTTP/2 DATA frame */
static int vlc_h2_parse_frame_data(struct vlc_h2_parser *p,
                                   struct vlc_h2_frame *f, size_t len,
                                   uint_fast32_t id)
{
    uint_fast8_t flags = vlc_h2_frame_flags(f);
    const uint8_t *ptr = vlc_h2_frame_payload(f);

    if (id == 0)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_PROTOCOL_ERROR);
    }

    if (len > VLC_H2_MAX_FRAME)
    {
        free(f);
        return vlc_h2_stream_error(p, id, VLC_H2_FRAME_SIZE_ERROR);
    }

    if (flags & VLC_H2_DATA_PADDED)
    {
        if (len < 1 || len < (1u + ptr[0]))
        {
            free(f);
            return vlc_h2_stream_error(p, id, VLC_H2_FRAME_SIZE_ERROR);
        }
        len -= 1 + ptr[0];
    }

    if (len > p->rcwd_size)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_FLOW_CONTROL_ERROR);
    }

    p->rcwd_size -= len;
    p->cbs->window_status(p->opaque, &p->rcwd_size);

    void *s = vlc_h2_stream_lookup(p, id);
    if (s == NULL)
    {
        free(f);
        return vlc_h2_stream_error(p, id, VLC_H2_STREAM_CLOSED);
    }

    int ret = p->cbs->stream_data(s, f);
    /* Frame gets consumed here ^^ */

    if (flags & VLC_H2_DATA_END_STREAM)
        p->cbs->stream_end(s);
    return ret;
}

/** Parses an HTTP/2 HEADERS frame */
static int vlc_h2_parse_frame_headers(struct vlc_h2_parser *p,
                                      struct vlc_h2_frame *f, size_t len,
                                      uint_fast32_t id)
{
    uint_fast8_t flags = vlc_h2_frame_flags(f);
    const uint8_t *ptr = vlc_h2_frame_payload(f);

    if (id == 0)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_PROTOCOL_ERROR);
    }

    if (len > VLC_H2_MAX_FRAME)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_FRAME_SIZE_ERROR);
    }

    if (flags & VLC_H2_HEADERS_PADDED)
    {
        if (len < 1 || len < (1u + ptr[0]))
        {
            free(f);
            return vlc_h2_parse_error(p, VLC_H2_FRAME_SIZE_ERROR);
        }
        len -= 1 + ptr[0];
        ptr++;
    }

    if (flags & VLC_H2_HEADERS_PRIORITY)
    {   /* Ignore priorities for now as we do not upload anything. */
        if (len < 5)
        {
            free(f);
            return vlc_h2_parse_error(p, VLC_H2_FRAME_SIZE_ERROR);
        }
        ptr += 5;
        len -= 5;
    }

    vlc_h2_parse_headers_start(p, id, flags & VLC_H2_HEADERS_END_STREAM);

    int ret = vlc_h2_parse_headers_append(p, ptr, len);

    if (ret == 0 && (flags & VLC_H2_HEADERS_END_HEADERS))
        ret = vlc_h2_parse_headers_end(p);

    free(f);
    return ret;
}

/** Parses an HTTP/2 PRIORITY frame */
static int vlc_h2_parse_frame_priority(struct vlc_h2_parser *p,
                                       struct vlc_h2_frame *f, size_t len,
                                       uint_fast32_t id)
{
    free(f);

    if (id == 0)
        return vlc_h2_parse_error(p, VLC_H2_PROTOCOL_ERROR);

    if (len != 5)
        return vlc_h2_stream_error(p, id, VLC_H2_FRAME_SIZE_ERROR);

    /* Ignore priorities for now as we do not upload much. */
    return 0;
}

/** Parses an HTTP/2 RST_STREAM frame */
static int vlc_h2_parse_frame_rst_stream(struct vlc_h2_parser *p,
                                         struct vlc_h2_frame *f, size_t len,
                                         uint_fast32_t id)
{
    if (id == 0)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_PROTOCOL_ERROR);
    }

    if (len != 4)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_FRAME_SIZE_ERROR);
    }

    void *s = vlc_h2_stream_lookup(p, id);
    uint_fast32_t code = GetDWBE(vlc_h2_frame_payload(f));

    free(f);

    if (s == NULL)
        return 0;
    return p->cbs->stream_reset(s, code);
}

/** Parses an HTTP/2 SETTINGS frame */
static int vlc_h2_parse_frame_settings(struct vlc_h2_parser *p,
                                       struct vlc_h2_frame *f, size_t len,
                                       uint_fast32_t id)
{
    const uint8_t *ptr = vlc_h2_frame_payload(f);

    if (id != 0)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_PROTOCOL_ERROR);
    }

    if (len % 6 || len > VLC_H2_MAX_FRAME)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_FRAME_SIZE_ERROR);
    }

    if (vlc_h2_frame_flags(f) & VLC_H2_SETTINGS_ACK)
    {
        free(f);
        if (len != 0)
            return vlc_h2_parse_error(p, VLC_H2_FRAME_SIZE_ERROR);
        /* Ignore ACKs for now as we never change settings. */
        return 0;
    }

    for (const uint8_t *end = ptr + len; ptr < end; ptr += 6)
        p->cbs->setting(p->opaque, GetWBE(ptr), GetDWBE(ptr + 2));

    free(f);
    return p->cbs->settings_done(p->opaque);
}

/** Parses an HTTP/2 PUSH_PROMISE frame */
static int vlc_h2_parse_frame_push_promise(struct vlc_h2_parser *p,
                                           struct vlc_h2_frame *f, size_t len,
                                           uint_fast32_t id)
{
    uint8_t flags = vlc_h2_frame_flags(f);
    const uint8_t *ptr = vlc_h2_frame_payload(f);

    if (id == 0)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_PROTOCOL_ERROR);
    }

    if (len > VLC_H2_MAX_FRAME)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_FRAME_SIZE_ERROR);
    }

    if (flags & VLC_H2_PUSH_PROMISE_PADDED)
    {
        if (len < 1 || len < (1u + ptr[0]))
        {
            free(f);
            return vlc_h2_parse_error(p, VLC_H2_FRAME_SIZE_ERROR);
        }
        len -= 1 + ptr[0];
        ptr++;
    }

    /* Not permitted by our settings. */
    free(f);
    return vlc_h2_parse_error(p, VLC_H2_PROTOCOL_ERROR);
}

/** Parses an HTTP/2 PING frame */
static int vlc_h2_parse_frame_ping(struct vlc_h2_parser *p,
                                   struct vlc_h2_frame *f, size_t len,
                                   uint_fast32_t id)
{
    uint64_t opaque;

    if (id != 0)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_PROTOCOL_ERROR);
    }

    if (len != 8)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_FRAME_SIZE_ERROR);
    }

    if (vlc_h2_frame_flags(f) & VLC_H2_PING_ACK)
    {
        free(f);
        return 0;
    }

    memcpy(&opaque, vlc_h2_frame_payload(f), 8);
    free(f);

    return p->cbs->ping(p->opaque, opaque);
}

/** Parses an HTTP/2 GOAWAY frame */
static int vlc_h2_parse_frame_goaway(struct vlc_h2_parser *p,
                                     struct vlc_h2_frame *f, size_t len,
                                     uint_fast32_t id)
{
    const uint8_t *ptr = vlc_h2_frame_payload(f);

    if (id != 0)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_PROTOCOL_ERROR);
    }

    if (len < 8 || len > VLC_H2_MAX_FRAME)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_FRAME_SIZE_ERROR);
    }

    uint_fast32_t last_id = GetDWBE(ptr) & 0x7FFFFFFF;
    uint_fast32_t code = GetDWBE(ptr + 4);

    free(f);
    return p->cbs->reset(p->opaque, last_id, code);
}

/** Parses an HTTP/2 WINDOW_UPDATE frame */
static int vlc_h2_parse_frame_window_update(struct vlc_h2_parser *p,
                                            struct vlc_h2_frame *f, size_t len,
                                            uint_fast32_t id)
{
    free(f);

    if (len != 4)
    {
        if (id == 0)
            return vlc_h2_parse_error(p, VLC_H2_FRAME_SIZE_ERROR);
        return vlc_h2_stream_error(p, id, VLC_H2_FRAME_SIZE_ERROR);
    }

    /* Nothing to do as we do not send data for the time being. */
    return 0;
}

/** Parses an HTTP/2 CONTINUATION frame */
static int vlc_h2_parse_frame_continuation(struct vlc_h2_parser *p,
                                           struct vlc_h2_frame *f, size_t len,
                                           uint_fast32_t id)
{
    const uint8_t *ptr = vlc_h2_frame_payload(f);

    /* Stream ID must match with the previous frame. */
    if (id == 0 || id != p->headers.sid)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_PROTOCOL_ERROR);
    }

    if (len > VLC_H2_MAX_FRAME)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_FRAME_SIZE_ERROR);
    }

    int ret = vlc_h2_parse_headers_append(p, ptr, len);

    if (ret == 0 && (vlc_h2_frame_flags(f) & VLC_H2_CONTINUATION_END_HEADERS))
        ret = vlc_h2_parse_headers_end(p);

    free(f);
    return 0;
}

/** Parses an HTTP/2 frame of unknown type */
static int vlc_h2_parse_frame_unknown(struct vlc_h2_parser *p,
                                      struct vlc_h2_frame *f, size_t len,
                                      uint_fast32_t id)
{
    free(f);

    if (len > VLC_H2_MAX_FRAME)
    {
        if (id == 0)
            return vlc_h2_parse_error(p, VLC_H2_FRAME_SIZE_ERROR);
        return vlc_h2_stream_error(p, id, VLC_H2_FRAME_SIZE_ERROR);
    }

    /* Ignore frames of unknown type as specified. */
    return 0;
}

static const vlc_h2_parser vlc_h2_parsers[] = {
    [VLC_H2_FRAME_DATA]          = vlc_h2_parse_frame_data,
    [VLC_H2_FRAME_HEADERS]       = vlc_h2_parse_frame_headers,
    [VLC_H2_FRAME_PRIORITY]      = vlc_h2_parse_frame_priority,
    [VLC_H2_FRAME_RST_STREAM]    = vlc_h2_parse_frame_rst_stream,
    [VLC_H2_FRAME_SETTINGS]      = vlc_h2_parse_frame_settings,
    [VLC_H2_FRAME_PUSH_PROMISE]  = vlc_h2_parse_frame_push_promise,
    [VLC_H2_FRAME_PING]          = vlc_h2_parse_frame_ping,
    [VLC_H2_FRAME_GOAWAY]        = vlc_h2_parse_frame_goaway,
    [VLC_H2_FRAME_WINDOW_UPDATE] = vlc_h2_parse_frame_window_update,
    [VLC_H2_FRAME_CONTINUATION]  = vlc_h2_parse_frame_continuation,
};

/** Parses the HTTP/2 connection preface. */
static int vlc_h2_parse_preface(struct vlc_h2_parser *p,
                                struct vlc_h2_frame *f, size_t len,
                                uint_fast32_t id)
{
    /* The length must be within the specification default limits. */
    if (len > VLC_H2_DEFAULT_MAX_FRAME
    /* The type must SETTINGS. */
     || vlc_h2_frame_type(f) != VLC_H2_FRAME_SETTINGS
    /* The SETTINGS ACK flag must be clear. */
     || (vlc_h2_frame_flags(f) & VLC_H2_SETTINGS_ACK))
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_PROTOCOL_ERROR);
    }

    p->parser = vlc_h2_parse_generic;

    return vlc_h2_parse_frame_settings(p, f, len, id);
}

/** Parses any HTTP/2 frame. */
static int vlc_h2_parse_generic(struct vlc_h2_parser *p,
                                struct vlc_h2_frame *f, size_t len,
                                uint_fast32_t id)
{
    uint_fast8_t type = vlc_h2_frame_type(f);
    vlc_h2_parser func = vlc_h2_parse_frame_unknown;

    assert(p->headers.sid == 0);

    if (type < sizeof (vlc_h2_parsers) / sizeof (vlc_h2_parsers[0])
     && vlc_h2_parsers[type] != NULL)
        func = vlc_h2_parsers[type];

    return func(p, f, len, id);
}

static int vlc_h2_parse_headers_block(struct vlc_h2_parser *p,
                                      struct vlc_h2_frame *f, size_t len,
                                      uint_fast32_t id)
{
    assert(p->headers.sid != 0);

    /* After a HEADER, PUSH_PROMISE of CONTINUATION frame without the
     * END_HEADERS flag, must come a CONTINUATION frame. */
    if (vlc_h2_frame_type(f) != VLC_H2_FRAME_CONTINUATION)
    {
        free(f);
        return vlc_h2_parse_error(p, VLC_H2_PROTOCOL_ERROR);
    }

    return vlc_h2_parse_frame_continuation(p, f, len, id);
}

static int vlc_h2_parse_failed(struct vlc_h2_parser *p, struct vlc_h2_frame *f,
                               size_t len, uint_fast32_t id)
{
    free(f);
    (void) p; (void) len; (void) id;
    return -1;
}

int vlc_h2_parse(struct vlc_h2_parser *p, struct vlc_h2_frame *f)
{
    int ret = 0;

    while (f != NULL)
    {
        struct vlc_h2_frame *next = f->next;
        size_t len = vlc_h2_frame_length(f);
        uint_fast32_t id = vlc_h2_frame_id(f);

        f->next = NULL;
        ret = p->parser(p, f, len, id);
        if (ret)
            p->parser = vlc_h2_parse_failed;
        f = next;
    }

    return ret;
}

struct vlc_h2_parser *vlc_h2_parse_init(void *ctx,
                                        const struct vlc_h2_parser_cbs *cbs)
{
    struct vlc_h2_parser *p = malloc(sizeof (*p));
    if (unlikely(p == NULL))
        return NULL;

    p->opaque = ctx;
    p->cbs = cbs;
    p->parser = vlc_h2_parse_preface;
    p->headers.sid = 0;
    p->headers.buf = NULL;
    p->headers.len = 0;
    p->headers.decoder = hpack_decode_init(VLC_H2_MAX_HEADER_TABLE);
    if (unlikely(p->headers.decoder == NULL))
    {
        free(p);
        return NULL;
    }
    p->rcwd_size = 65535; /* initial per-connection value */
    return p;
}

void vlc_h2_parse_destroy(struct vlc_h2_parser *p)
{
    hpack_decode_destroy(p->headers.decoder);
    free(p->headers.buf);
    free(p);
}
