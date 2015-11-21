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
 * GNU General Public License for more details.
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

static_assert(VLC_H2_CONTINUATION_END_HEADERS == VLC_H2_HEADERS_END_HEADERS,
              "Oops");

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
