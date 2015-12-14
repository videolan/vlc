/*****************************************************************************
 * hpackenc.c: HPACK Header Compression for HTTP/2 encoder
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

#ifdef ENC_TEST
# undef NDEBUG
#endif

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "hpack.h"

/*
 * This is curently the simplest possible HPACK compressor: it does not
 * compress anything and is stateless.
 * TODO:
 *  - use static Huffman compression when useful,
 *  - use header names and values from the static HPACK table,
 *  - become stateful and let the caller specify the indexed flag,
 *  - let caller specify the never-indexed flag.
 */

static size_t hpack_encode_int(uint8_t *restrict buf, size_t size,
                               uintmax_t value, unsigned n)
{
    assert(n >= 1 && n <= 8);

    unsigned mask = (1 << n) - 1;
    size_t ret = 1;

    if (value < mask)
    {
        if (size > 0)
            buf[0] |= value;
        return 1;
    }

    if (size > 0)
       *(buf++) |= mask;
    value -= mask;

    while (value >= 128)
    {
        if (ret++ < size)
            *(buf++) = 0x80 | (value & 0x7F);
        value >>= 7;
    }

    if (ret++ < size)
        *(buf++) = value;
    return ret;
}

static size_t hpack_encode_str_raw(uint8_t *restrict buf, size_t size,
                                   const char *str)
{
    size_t len = strlen(str);

    if (size > 0)
        *buf = 0;

    size_t ret = hpack_encode_int(buf, size, len, 7);
    if (ret < size)
    {
        buf += ret;
        size -= ret;

        memcpy(buf, str, (len <= size) ? len : size);
    }
    ret += len;
    return ret;
}

static size_t hpack_encode_str_raw_lower(uint8_t *restrict buf, size_t size,
                                         const char *str)
{
    size_t len = strlen(str);

    if (size > 0)
        *buf = 0;

    size_t ret = hpack_encode_int(buf, size, len, 7);
    if (ret < size)
    {
        buf += ret;
        size -= ret;

        for (size_t i = 0; i < len && i < size; i++)
            if (str[i] < 'A' || str[i] > 'Z')
                buf[i] = str[i];
            else
                buf[i] = str[i] - 'A' + 'a';
    }
    ret += len;
    return ret;
}

size_t hpack_encode_hdr_neverindex(uint8_t *restrict buf, size_t size,
                                   const char *name, const char *value)
{
    size_t ret = 1, val;

    if (size > 0)
    {
        *(buf++) = 0x10;
        size--;
    }

    val = hpack_encode_str_raw_lower(buf, size, name);
    if (size >= val)
    {
        buf += val;
        size -= val;
    }
    else
        size = 0;
    ret += val;

    val = hpack_encode_str_raw(buf, size, value);
    if (size >= val)
    {
        buf += val;
        size -= val;
    }
    else
        size = 0;
    ret += val;

    return ret;
}

size_t hpack_encode(uint8_t *restrict buf, size_t size,
                    const char *const headers[][2], unsigned count)
{
    size_t ret = 0;

    while (count > 0)
    {
        size_t val = hpack_encode_hdr_neverindex(buf, size, headers[0][0],
                                                 headers[0][1]);
        if (size >= val)
        {
            buf += val;
            size -= val;
        }
        else
            size = 0;

        ret += val;
        headers++;
        count--;
    }
    return ret;
}

/*** Test cases ***/
#ifdef ENC_TEST
# include <stdarg.h>
# include <stdlib.h>
# include <stdio.h>

static void test_integer(unsigned n, uintmax_t value)
{
    uint8_t buf[3];
    size_t ret;

    for (unsigned i = 0; i < 3; i++)
    {
        ret = hpack_encode_int(buf, sizeof (buf), value, n);
        assert(ret > 0);
    }
}

static void test_integers(void)
{
    test_integer(5, 10);
    test_integer(5, 1337);
    test_integer(8, 42);
}

static void test_lowercase(const char *str)
{
    char c;

    while ((c = *(str++)) != '\0')
        assert(c < 'A' || c > 'Z');
}

static void test_block(const char *name, ...)
{
    const char *headers[16][2];
    unsigned count = 0;
    va_list ap;

    va_start(ap, name);
    while (name != NULL)
    {
        headers[count][0] = name;
        headers[count][1] = va_arg(ap, const char *);
        name = va_arg(ap, const char *);
        count++;
    }
    va_end(ap);

    printf(" %u header(s):\n", count);

    uint8_t buf[1024];

    size_t length = hpack_encode(NULL, 0, headers, count);

    for (size_t i = 0; i < sizeof (buf); i++)
        assert(hpack_encode(buf, i, headers, count) == length);

    memset(buf, 0xAA, sizeof (buf));
    assert(hpack_encode(buf, length, headers, count) == length);

    char *eheaders[16][2];

    struct hpack_decoder *dec = hpack_decode_init(4096);
    assert(dec != NULL);

    int ecount = hpack_decode(dec, buf, length, eheaders, 16);
    assert((unsigned)ecount == count);

    hpack_decode_destroy(dec);

    for (unsigned i = 0; i < count; i++)
    {
        printf("  %s: %s\n", eheaders[i][0], eheaders[i][1]);
        test_lowercase(eheaders[i][0]);
        assert(!strcasecmp(eheaders[i][0], headers[i][0]));
        assert(!strcmp(eheaders[i][1], headers[i][1]));
        free(eheaders[i][1]);
        free(eheaders[i][0]);
    }
}

static void test_reqs(void)
{
    test_block(NULL);
    test_block(":method", "GET", ":scheme", "http", ":path", "/",
               ":authority", "www.example.com", NULL);
    test_block(":method", "GET", ":scheme", "http", ":path", "/",
               ":authority", "www.example.com", "Cache-Control", "no-cache",
               NULL);
    test_block(":method", "GET", ":scheme", "https", ":path", "/index.html",
               ":authority", "www.example.com", "custom-key", "custom-value",
               NULL);
}

static void test_resps(void)
{
    test_block(":status", "302", "Cache-Control", "private",
               "Date", "Mon, 21 Oct 2013 20:13:21 GMT",
               "Location", "https://www.example.com", NULL);
    test_block(":status", "307", "Cache-Control", "private",
               "Date", "Mon, 21 Oct 2013 20:13:21 GMT",
               "Location", "https://www.example.com", NULL);
    test_block(":status", "200", "Cache-Control", "private",
               "Date", "Mon, 21 Oct 2013 20:13:22 GMT",
               "Location", "https://www.example.com",
               "Content-Encoding", "gzip",
               "Set-Cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; "
                   "max-age=3600; version=1",
               NULL);
}

int main(void)
{
    test_integers();
    test_reqs();
    test_resps();
}
#endif /* TEST */
