/*****************************************************************************
 * h2frame_test.c: HTTP/2 frame formatting tests
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
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "h2frame.h"
#include <vlc_common.h>
#include "conn.h"

static char dummy;
static char *const CTX = &dummy;

static unsigned settings;

/* Callbacks */
void vlc_http_dbg(void *stream, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stream, fmt, ap);
    va_end(ap);
}

static void vlc_h2_setting(void *ctx, uint_fast16_t id, uint_fast32_t value)
{
    assert(ctx == CTX);
    settings++;

    fprintf(stderr, "* Setting %s: %"PRIuFAST32"\n",
            vlc_h2_setting_name(id), value);

    switch (id)
    {
        case VLC_H2_SETTING_HEADER_TABLE_SIZE:
            assert(value == VLC_H2_MAX_HEADER_TABLE);
            break;
        case VLC_H2_SETTING_ENABLE_PUSH:
            assert(value == 0);
            break;
        case VLC_H2_SETTING_MAX_CONCURRENT_STREAMS:
            assert(value == VLC_H2_MAX_STREAMS);
            break;
        case VLC_H2_SETTING_INITIAL_WINDOW_SIZE:
            assert(value == VLC_H2_INIT_WINDOW);
            break;
        case VLC_H2_SETTING_MAX_FRAME_SIZE:
            assert(value == VLC_H2_MAX_FRAME);
            break;
        case VLC_H2_SETTING_MAX_HEADER_LIST_SIZE:
            assert(value == VLC_H2_MAX_HEADER_LIST);
            break;
        default:
            assert(!"Known setting");
            break;
    }
}

static unsigned settings_acked;

static int vlc_h2_settings_done(void *ctx)
{
    assert(ctx == CTX);
    settings_acked++;
    (void) ctx;
    return 0;
}

#define PING_VALUE UINT64_C(0x1122334455667788)

static unsigned pings;

static int vlc_h2_ping(void *ctx, uint_fast64_t opaque)
{
    assert(ctx == CTX);
    assert(opaque == PING_VALUE);
    pings++;
    return 0;
}

static uint_fast32_t remote_error;

static void vlc_h2_error(void *ctx, uint_fast32_t code)
{
    assert(ctx == CTX);
    (void) code;
}

static int vlc_h2_reset(void *ctx, uint_fast32_t last_seq, uint_fast32_t code)
{
    assert(ctx == CTX);
    assert(last_seq == 0);
    remote_error = code;
    return 0;
}

static void vlc_h2_window_status(void *ctx, uint32_t *rcwd)
{
    assert(ctx == CTX);
    *rcwd = (1u << 31) - 1;
}

static void vlc_h2_window_update(void *ctx, uint_fast32_t credit)
{
    assert(ctx == CTX);
    assert(credit == 0x1000);
}

#define STREAM_ID 0x76543210
char stream_cookie; /* dummy unique value */

static void *vlc_h2_stream_lookup(void *ctx, uint_fast32_t id)
{
    assert(ctx == CTX);

    if (id != STREAM_ID)
        return NULL;
    return &stream_cookie;
}

static const char *const resp_hdrv[][2] = {
    { ":status",          "200" },
    { "cache-control",    "private" },
    { "date",             "Mon, 21 Oct 2013 20:13:22 GMT" },
    { "location",         "https://www.example.com" },
    { "content-encoding", "gzip" },
    { "set-cookie",       "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; "
                          "version=1" },
};
static const unsigned resp_hdrc = sizeof (resp_hdrv) / sizeof (resp_hdrv[0]);

static unsigned stream_header_tables;

static void vlc_h2_stream_headers(void *ctx, unsigned count,
                                  const char *const hdrs[][2])
{
    assert(ctx == &stream_cookie);
    assert(count == resp_hdrc);

    for (unsigned i = 0; i < count; i++)
    {
        assert(!strcmp(hdrs[i][0], resp_hdrv[i][0]));
        assert(!strcmp(hdrs[i][1], resp_hdrv[i][1]));
    }

    stream_header_tables++;
}

#define MESSAGE "Hello world!"

static unsigned stream_blocks;

static int vlc_h2_stream_data(void *ctx, struct vlc_h2_frame *f)
{
    size_t len;
    const uint8_t *buf = vlc_h2_frame_data_get(f, &len);

    assert(ctx == &stream_cookie);
    assert(len == sizeof (MESSAGE));
    assert(!memcmp(buf, MESSAGE, len));
    stream_blocks++;
    free(f);
    return 0;
}

static unsigned stream_ends;

static void vlc_h2_stream_end(void *ctx)
{
    assert(ctx == &stream_cookie);
    stream_ends++;
}

static int vlc_h2_stream_error(void *ctx, uint_fast32_t id, uint_fast32_t code)
{
    assert(ctx == CTX);

    switch (id)
    {
        case STREAM_ID + 2:
            assert(code == VLC_H2_REFUSED_STREAM);
            break;
        case STREAM_ID + 4:
            assert(code == VLC_H2_PROTOCOL_ERROR);
            break;
        case STREAM_ID + 6:
            assert(code == VLC_H2_FRAME_SIZE_ERROR);
            break;
        default:
            assert(0);
    }
    return 0;
}

static int vlc_h2_stream_reset(void *ctx, uint_fast32_t code)
{
    assert(ctx == &stream_cookie);
    assert(code == VLC_H2_CANCEL);
    return 0;
}

static void vlc_h2_stream_window_update(void *ctx, uint_fast32_t credit)
{
    assert(ctx == &stream_cookie);
    (void) credit;
}

/* Frame formatting */
static struct vlc_h2_frame *resize(struct vlc_h2_frame *f, size_t size)
{   /* NOTE: increasing size would require realloc() */
    f->data[0] = size >> 16;
    f->data[1] = size >> 8;
    f->data[2] = size;
    return f;
}

static struct vlc_h2_frame *retype(struct vlc_h2_frame *f, unsigned char type)
{
    f->data[3] = type;
    return f;
}

static struct vlc_h2_frame *reflag(struct vlc_h2_frame *f, unsigned char flags)
{
    f->data[4] = flags;
    return f;
}

static struct vlc_h2_frame *globalize(struct vlc_h2_frame *f)
{
    memset(f->data + 5, 0, 4);
    return f;
}

static struct vlc_h2_frame *localize(struct vlc_h2_frame *f)
{
    f->data[5] = (STREAM_ID >> 24) & 0xff;
    f->data[6] = (STREAM_ID >> 16) & 0xff;
    f->data[7] = (STREAM_ID >>  8) & 0xff;
    f->data[8] =  STREAM_ID        & 0xff;
    return f;
}

static struct vlc_h2_frame *response(bool eos)
{
    /* Use ridiculously small MTU to test headers fragmentation */
    return vlc_h2_frame_headers(STREAM_ID, 16, eos, resp_hdrc, resp_hdrv);
}

static struct vlc_h2_frame *data(bool eos)
{
    return vlc_h2_frame_data(STREAM_ID, MESSAGE, sizeof (MESSAGE), eos);
}

static struct vlc_h2_frame *priority(void)
{
    return localize(resize(retype(data(false), 0x2), 5));
}

static struct vlc_h2_frame *rst_stream(void)
{
    return vlc_h2_frame_rst_stream(STREAM_ID, VLC_H2_CANCEL);
}

static struct vlc_h2_frame *ping(void)
{
    return vlc_h2_frame_ping(PING_VALUE);
}

static struct vlc_h2_frame *goaway(void)
{
    return vlc_h2_frame_goaway(0, VLC_H2_NO_ERROR);
}

static struct vlc_h2_frame *unknown(void)
{
    return retype(ping(), 200);
}

/* Test harness */
static unsigned test_raw_seqv(struct vlc_h2_parser *p, va_list ap)
{
    struct vlc_h2_frame *f;
    unsigned i = 0;

    assert(p != NULL);

    while ((f = va_arg(ap, struct vlc_h2_frame *)) != NULL)
        if (vlc_h2_parse(p, f) == 0)
            i++;

    return i;
}

static unsigned test_raw_seq(struct vlc_h2_parser *p, ...)
{
    va_list ap;
    unsigned i;

    assert(p != NULL);
    va_start(ap, p);
    i = test_raw_seqv(p, ap);
    va_end(ap);
    return i;
}

static const struct vlc_h2_parser_cbs vlc_h2_frame_test_callbacks =
{
    vlc_h2_setting,
    vlc_h2_settings_done,
    vlc_h2_ping,
    vlc_h2_error,
    vlc_h2_reset,
    vlc_h2_window_status,
    vlc_h2_window_update,
    vlc_h2_stream_lookup,
    vlc_h2_stream_error,
    vlc_h2_stream_headers,
    vlc_h2_stream_data,
    vlc_h2_stream_end,
    vlc_h2_stream_reset,
    vlc_h2_stream_window_update,
};

static unsigned test_seq(void *ctx, ...)
{
    struct vlc_h2_parser *p;
    va_list ap;
    unsigned i;

    settings = settings_acked = 0;
    pings = 0;
    remote_error = -1;
    stream_header_tables = stream_blocks = stream_ends = 0;

    p = vlc_h2_parse_init(ctx, &vlc_h2_frame_test_callbacks);
    assert(p != NULL);

    i = test_raw_seq(p, vlc_h2_frame_settings(), vlc_h2_frame_settings_ack(),
                     NULL);
    assert(i == 2);
    assert(settings >= 1);
    assert(settings_acked == 1);

    va_start(ap, ctx);
    i = test_raw_seqv(p, ap);
    va_end(ap);

    assert(test_raw_seq(p, goaway(), NULL) == 1);
    assert(remote_error == VLC_H2_NO_ERROR);

    vlc_h2_parse_destroy(p);
    return i;
}

static unsigned test_bad_seq(void *ctx, ...)
{
    struct vlc_h2_parser *p;
    va_list ap;
    unsigned i;

    p = vlc_h2_parse_init(ctx, &vlc_h2_frame_test_callbacks);
    assert(p != NULL);

    i = test_raw_seq(p, vlc_h2_frame_settings(), vlc_h2_frame_settings_ack(),
                     NULL);
    assert(i == 2);

    va_start(ap, ctx);
    i = test_raw_seqv(p, ap);
    va_end(ap);

    i = test_raw_seq(p, goaway(), NULL);
    assert(i == 0); /* in failed state */
    vlc_h2_parse_destroy(p);
    return i;
}

static void test_preface_fail(void)
{
    struct vlc_h2_parser *p;

    p = vlc_h2_parse_init(CTX, &vlc_h2_frame_test_callbacks);
    assert(p != NULL);

    assert(test_raw_seq(p, ping(), ping(), NULL) == 0);

    vlc_h2_parse_destroy(p);
}

static void test_header_block_fail(void)
{
    struct vlc_h2_frame *hf = response(true);
    struct vlc_h2_frame *pf = ping();

    /* Check what happens if non-CONTINUATION frame after HEADERS */
    assert(hf != NULL && hf->next != NULL && pf != NULL);
    pf->next = hf->next;
    hf->next = pf;
    assert(test_bad_seq(CTX, hf, NULL) == 0);
}

int main(void)
{
    int ret;

    ret = test_seq(CTX, NULL);
    assert(ret == 0);

    ret = test_seq(CTX, ping(), vlc_h2_frame_pong(42), ping(), NULL);
    assert(ret == 3);
    assert(pings == 2);
    assert(stream_header_tables == 0);
    assert(stream_blocks == 0);
    assert(stream_ends == 0);

    ret = test_seq(CTX, response(true), NULL);
    assert(ret == 1);
    assert(pings == 0);
    assert(stream_header_tables == 1);
    assert(stream_blocks == 0);
    assert(stream_ends == 1);

    ret = test_seq(CTX, response(false), data(true), ping(),
                        response(false), data(false), data(true),
                        response(false), data(false), priority(), unknown(),
                        NULL);
    assert(ret == 10);
    assert(pings == 1);
    assert(stream_header_tables == 3);
    assert(stream_blocks == 4);
    assert(stream_ends == 2);

    ret = test_seq(CTX, rst_stream(),
                        vlc_h2_frame_window_update(0, 0x1000),
                        vlc_h2_frame_headers(STREAM_ID + 2,
                                             VLC_H2_DEFAULT_MAX_FRAME, true,
                                             resp_hdrc, resp_hdrv),
                        NULL);
    assert(ret == 3);
    assert(pings == 0);
    assert(stream_header_tables == 0);
    assert(stream_blocks == 0);
    assert(stream_ends == 0);

    test_preface_fail();
    test_header_block_fail();

    test_bad_seq(CTX, globalize(response(true)), NULL);
    test_bad_seq(CTX, resize(reflag(response(true), 0x08), 0), NULL);
    test_bad_seq(CTX, resize(reflag(response(true), 0x20), 4), NULL);
    test_bad_seq(CTX, globalize(data(true)), NULL);
    test_bad_seq(CTX, globalize(priority()), NULL);
    test_bad_seq(CTX, globalize(rst_stream()), NULL);
    test_bad_seq(CTX, localize(vlc_h2_frame_settings()), NULL);
    test_bad_seq(CTX, resize(vlc_h2_frame_settings(), 5), NULL);
    test_bad_seq(CTX, resize(ping(), 7), NULL);
    test_bad_seq(CTX, localize(ping()), NULL);
    test_bad_seq(CTX, localize(goaway()), NULL);
    test_bad_seq(CTX, resize(goaway(), 7), NULL);
    test_bad_seq(CTX, vlc_h2_frame_window_update(0, 0), NULL);
    test_bad_seq(CTX, resize(vlc_h2_frame_window_update(0, 0), 3), NULL);

    test_seq(CTX, resize(vlc_h2_frame_window_update(STREAM_ID + 6, 0), 5),
             NULL);
    test_seq(CTX, vlc_h2_frame_window_update(STREAM_ID + 4, 0), NULL);

    /* TODO: PUSH_PROMISE, PRIORITY, padding, unknown, invalid stuff... */

    /* Dummy API test */
    assert(vlc_h2_frame_data(1, NULL, 1 << 28, false) == NULL);

    for (unsigned i = 0; i < 65536; i++)
        assert(strlen(vlc_h2_setting_name(i)) > 0);
    for (unsigned i = 0; i < 100; i++) /* lets not try all 4 billions */
        assert(strlen(vlc_h2_strerror(i)) > 0);

    return 0;
}
