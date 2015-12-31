/*****************************************************************************
 * hpack.c: HPACK Header Compression for HTTP/2
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

#ifdef DEC_TEST
# undef NDEBUG
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "hpack.h"

/** Static Table header names */
static const char hpack_names[][28] =
{
    ":authority", ":method", ":method", ":path", ":path", ":scheme", ":scheme",
    ":status", ":status", ":status", ":status", ":status", ":status",
    ":status", "accept-charset", "accept-encoding", "accept-language",
    "accept-ranges", "accept", "access-control-allow-origin", "age", "allow",
    "authorization", "cache-control", "content-disposition",
    "content-encoding", "content-language", "content-length",
    "content-location", "content-range", "content-type", "cookie", "date",
    "etag", "expect", "expires", "from", "host", "if-match",
    "if-modified-since", "if-none-match", "if-range", "if-unmodified-since",
    "last-modified", "link", "location", "max-forwards", "proxy-authenticate",
    "proxy-authorization", "range", "referer", "refresh", "retry-after",
    "server", "set-cookie", "strict-transport-security", "transfer-encoding",
    "user-agent", "vary", "via", "www-authenticate",
};

/** Static Table header values */
static const char hpack_values[][14] =
{
    "", "GET", "POST", "/", "/index.html", "http", "https", "200", "204",
    "206", "304", "400", "404", "500", "", "gzip, deflate"
};

struct hpack_decoder
{
    char **table;
    size_t entries;
    size_t size;
    size_t max_size;
};

struct hpack_decoder *hpack_decode_init(size_t header_table_size)
{
    struct hpack_decoder *dec = malloc(sizeof (*dec));
    if (dec == NULL)
        return NULL;

    dec->table = NULL;
    dec->entries = 0;
    dec->size = 0;
    dec->max_size = header_table_size;
    return dec;
}

void hpack_decode_destroy(struct hpack_decoder *dec)
{
    for (unsigned i = 0; i < dec->entries; i++)
        free(dec->table[i]);
    free(dec->table);
    free(dec);
}

/**
 * Decodes an HPACK unsigned variable length integer.
 * @return the value on success, -1 on error (and sets errno).
 */
static int_fast32_t hpack_decode_int(unsigned n,
                                     const uint8_t **restrict datap,
                                     size_t *restrict lengthp)
{
    const uint8_t *p = *datap;
    size_t length = *lengthp;

    assert(n >= 1 && n <= 8);
    assert(length >= 1);

    unsigned mask = (1 << n) - 1;
    uint_fast32_t i = *(p++) & mask;
    length--;

    if (i == mask)
    {
        unsigned shift = 0;
        uint8_t b;

        do
        {
            if (length-- < 1)
            {
                errno = EINVAL;
                return -1;
            }

            if (shift >= 28)
            {
                errno = ERANGE;
                return -1;
            }

            b = *(p++);
            i += (b & 0x7F) << shift;
            shift += 7;
        }
        while (b & 0x80);
    }

    *datap = p;
    *lengthp = length;
    return i;
}

/**
 * Decodes a raw string literal.
 */
static char *hpack_decode_str_raw(const uint8_t *data, size_t length)
{
    char *s = malloc(length + 1);
    if (s != NULL)
    {
        memcpy(s, data, length);
        s[length] = '\0';
    }
    return s;
}

static int hpack_decode_byte_huffman(const uint8_t *restrict end,
                                     int *restrict bit_offset)
{
    static const unsigned char tab[256] = {
        /*  5 bits */
         48,  49,  50,  97,  99, 101, 105, 111, 115, 116,
        /*  6 bits */
         32,  37,  45,  46,  47,  51,  52,  53,  54,  55,  56,  57,  61,  65,
         95,  98, 100, 102, 103, 104, 108, 109, 110, 112, 114, 117,
        /*  7 bits */
         58,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,
         79,  80,  81,  82,  83,  84,  85,  86,  87,  89, 106, 107, 113, 118,
        119, 120, 121, 122,
        /*  8 bits */
         38,  42,  44,  59,  88,  90,
        /* 10 bits */
         33,  34,  40,  41,  63,
        /* 11 bits */
         39,  43, 124,
        /* 12 bits */
         35,  62,
        /* 13 bits */
          0,  36,  64,  91,  93, 126,
        /* 14 bits */
         94, 125,
        /* 15 bits */
         60,  96, 123,
        /* 19 bits */
         92, 195, 208,
        /* 20 bits */
        128, 130, 131, 162, 184, 194, 224, 226,
        /* 21 bits */
        153, 161, 167, 172, 176, 177, 179, 209, 216, 217, 227, 229, 230,
        /* 22 bits */
        129, 132, 133, 134, 136, 146, 154, 156, 160, 163, 164, 169, 170, 173,
        178, 181, 185, 186, 187, 189, 190, 196, 198, 228, 232, 233,
        /* 23 bits */
          1, 135, 137, 138, 139, 140, 141, 143, 147, 149, 150, 151, 152, 155,
        157, 158, 165, 166, 168, 174, 175, 180, 182, 183, 188, 191, 197, 231,
        239,
        /* 24 bits */
          9, 142, 144, 145, 148, 159, 171, 206, 215, 225, 236, 237,
        /* 25 bits */
        199, 207, 234, 235,
        /* 26 bits */
        192, 193, 200, 201, 202, 205, 210, 213, 218, 219, 238, 240, 242, 243,
        255,
        /* 27 bits */
        203, 204, 211, 212, 214, 221, 222, 223, 241, 244, 245, 246, 247, 248,
        250, 251, 252, 253, 254,
        /* 28 bits */
          2,   3,   4,   5,   6,   7,   8,  11,  12,  14,  15,  16,  17,  18,
         19,  20,  21,  23,  24,  25,  26,  27,  28,  29,  30,  31, 127, 220,
        249,
        /* 30 bits */
         10,  13,  22,
    };
    static const unsigned char values[30] = {
        0,  0,  0,  0, 10, 26, 32,  6,  0,  5,  3,  2,  6,  2,  3,
        0,  0,  0,  3,  8, 13, 26, 29, 12,  4, 15, 19, 29,  0,  3
    };
    const unsigned char *p = tab;
    uint_fast32_t code = 0, offset = 0;
    unsigned shift = -*bit_offset;

    for (unsigned i = 0; i < 30; i++)
    {
        code <<= 1;

        /* Read one bit */
        if (*bit_offset)
        {
            shift = (shift - 1) & 7;
            code |= (end[*bit_offset >> 3] >> shift) & 1;
            (*bit_offset)++;
        }
        else
            code |= 1; /* EOS is all ones */

        assert(code >= offset);
        if ((code - offset) < values[i])
            return p[code - offset];
        p += values[i];
        offset = (offset + values[i]) * 2;
    }

    assert(p - tab == 256);

    if (code == 0x3fffffff)
        return 256; /* EOS */

    errno = EINVAL;
    return -1;
}

/**
 * Decodes an Huffman-encoded string literal.
 */
static char *hpack_decode_str_huffman(const uint8_t *data, size_t length)
{
    unsigned char *str = malloc(length * 2 + 1);
    if (str == NULL)
        return NULL;

    size_t len = 0;
    int bit_offset = -8 * length;
    data += length;

    for (;;)
    {
        int c = hpack_decode_byte_huffman(data, &bit_offset);
        if (c < 0)
        {
            errno = EINVAL;
            goto error;
        }

        /* NOTE: EOS (256) is converted to nul terminator */
        str[len++] = c;

        if (c == 256)
            break;
    }

    return (char *)str;

error:
    free(str);
    return NULL;
}

/**
 * Decodes a string literal.
 * @return a heap-allocated nul-terminated string success,
 *         NULL on error (and sets errno).
 */
static char *hpack_decode_str(const uint8_t **restrict datap,
                              size_t *restrict lengthp)
{
    if (*lengthp < 1)
    {
        errno = EINVAL;
        return NULL;
    }

    bool huffman = ((*datap)[0] & 0x80) != 0;
    int_fast32_t len = hpack_decode_int(7, datap, lengthp);
    if (len < 0)
        return NULL;

    if ((size_t)len > *lengthp)
    {
        errno = EINVAL;
        return NULL;
    }

    if (len > 65535) /* Stick to a sane limit */
    {
        errno = ERANGE;
        return NULL;
    }

    const uint8_t *buf = *datap;

    *datap += len;
    *lengthp -= len;

    return (huffman ? hpack_decode_str_huffman : hpack_decode_str_raw)
            (buf, len);
}

static char *hpack_lookup_name(const struct hpack_decoder *dec,
                               uint_fast32_t idx)
{
    if (idx == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    idx--;
    if (idx < sizeof (hpack_names) / sizeof (hpack_names[0]))
        return strdup(hpack_names[idx]);

    idx -= sizeof (hpack_names) / sizeof (hpack_names[0]);
    if (idx < dec->entries)
    {
        const char *entry = dec->table[dec->entries - (idx + 1)];
        return strdup(entry);
    }

    errno = EINVAL;
    return NULL;
}

static char *hpack_lookup_value(const struct hpack_decoder *dec,
                                uint_fast32_t idx)
{
    if (idx == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    idx--;
    if (idx < sizeof (hpack_values) / sizeof (hpack_values[0]))
        return strdup(hpack_values[idx]);
    if (idx < sizeof (hpack_names) / sizeof (hpack_names[0]))
        return strdup("");

    idx -= sizeof (hpack_names) / sizeof (hpack_names[0]);
    if (idx < dec->entries)
    {
        const char *entry = dec->table[dec->entries - (idx + 1)];
        return strdup(entry + strlen(entry) + 1);
    }

    errno = EINVAL;
    return NULL;
}

static void hpack_decode_evict(struct hpack_decoder *dec)
{
    /* Eviction: count how many entries to evict */
    size_t evicted = 0;
    while (dec->size > dec->max_size)
    {
        assert(evicted < dec->entries);

        size_t namelen = strlen(dec->table[evicted]);
        size_t valuelen = strlen(dec->table[evicted] + namelen + 1);

        assert(dec->size >= 32 + namelen + valuelen);
        dec->size -= 32 + namelen + valuelen;
        evicted++;
    }

    /* Eviction: remove oldest entries */
    if (evicted > 0)
    {
        for (size_t i = 0; i < evicted; i++)
            free(dec->table[i]);

        dec->entries -= evicted;
        memmove(dec->table, dec->table + evicted,
                sizeof (dec->table[0]) * dec->entries);
    }
}

static int hpack_append_hdr(struct hpack_decoder *dec,
                            const char *name, const char *value)
{
    size_t namelen = strlen(name), valuelen = strlen(value);
    char *entry = malloc(namelen + valuelen + 2);
    if (entry == NULL)
        return -1;
    memcpy(entry, name, namelen + 1);
    memcpy(entry + namelen + 1, value, valuelen + 1);

    char **newtab = realloc(dec->table,
                            sizeof (dec->table[0]) * (dec->entries + 1));
    if (newtab == NULL)
    {
        free(entry);
        return -1;
    }

    dec->table = newtab;
    dec->table[dec->entries] = entry;
    dec->entries++;
    dec->size += 32 + namelen + valuelen;

    hpack_decode_evict(dec);
    return 0;
}

static int hpack_decode_hdr_indexed(struct hpack_decoder *dec,
                                    const uint8_t **restrict datap,
                                    size_t *restrict lengthp,
                                    char **restrict namep,
                                    char **restrict valuep)
{
    int_fast32_t idx = hpack_decode_int(7, datap, lengthp);
    if (idx < 0)
        return -1;

    char *name = hpack_lookup_name(dec, idx);
    if (name == NULL)
        return -1;

    char *value = hpack_lookup_value(dec, idx);
    if (value == NULL)
    {
        free(name);
        return -1;
    }

    *namep = name;
    *valuep = value;
    return 0;
}

static int hpack_decode_hdr_index(struct hpack_decoder *dec,
                                  const uint8_t **restrict datap,
                                  size_t *restrict lengthp,
                                  char **restrict namep,
                                  char **restrict valuep)
{
    int_fast32_t idx = hpack_decode_int(6, datap, lengthp);
    if (idx < 0)
        return -1;

    char *name;

    if (idx != 0)
        name = hpack_lookup_name(dec, idx);
    else
        name = hpack_decode_str(datap, lengthp);
    if (name == NULL)
        return -1;

    char *value = hpack_decode_str(datap, lengthp);
    if (value == NULL)
    {
        free(name);
        return -1;
    }

    if (hpack_append_hdr(dec, name, value))
    {
        free(value);
        free(name);
        return -1;
    }

    *namep = name;
    *valuep = value;
    return 0;
}

static int hpack_decode_hdr_noindex(struct hpack_decoder *dec,
                                    const uint8_t **restrict datap,
                                    size_t *restrict lengthp,
                                    char **restrict namep,
                                    char **restrict valuep)
{
    int_fast32_t idx = hpack_decode_int(4, datap, lengthp);
    if (idx < 0)
        return -1;

    char *name;

    if (idx != 0)
        name = hpack_lookup_name(dec, idx);
    else
        name = hpack_decode_str(datap, lengthp);
    if (name == NULL)
        return -1;

    char *value = hpack_decode_str(datap, lengthp);
    if (value == NULL)
    {
        free(name);
        return -1;
    }

    *namep = name;
    *valuep = value;
    return 0;
}

static int hpack_decode_tbl_update(struct hpack_decoder *dec,
                                   const uint8_t **restrict datap,
                                   size_t *restrict lengthp,
                                   char **restrict name,
                                   char **restrict value)
{
    int_fast32_t max = hpack_decode_int(5, datap, lengthp);
    if (max < 0)
        return -1;

    if ((size_t)max > dec->max_size)
    {   /* Increasing the maximum is not permitted per the specification */
        errno = EINVAL;
        return -1;
    }

    *value = *name = NULL;
    dec->max_size = max;
    hpack_decode_evict(dec);
    return 0;
}

static int hpack_decode_hdr(struct hpack_decoder *dec,
                            const uint8_t **restrict datap,
                            size_t *restrict lengthp,
                            char **restrict namep,
                            char **restrict valuep)
{
    int (*cb)(struct hpack_decoder *, const uint8_t **, size_t *,
              char **, char **);

    assert(*lengthp >= 1);

    uint8_t b = **datap;

    if (b & 0x80)
        cb = hpack_decode_hdr_indexed;
    else if (b & 0x40)
        cb = hpack_decode_hdr_index;
    else if (b & 0x20)
        cb = hpack_decode_tbl_update;
    else
    /* NOTE: never indexed and not indexed are treated identically */
        cb = hpack_decode_hdr_noindex;

    return cb(dec, datap, lengthp, namep, valuep);
}

int hpack_decode(struct hpack_decoder *dec, const uint8_t *data,
                 size_t length, char *headers[][2], unsigned max)
{
    unsigned count = 0;

    while (length > 0)
    {
        char *name, *value;
        int val = hpack_decode_hdr(dec, &data, &length, &name, &value);
        if (val < 0)
            goto error;

        assert((name == NULL) == (value == NULL));
        if (name == NULL)
            continue;

        if (count < max)
        {
            headers[count][0] = name;
            headers[count][1] = value;
        }
        else
        {
            free(value);
            free(name);
        }
        count++;
    }
    return count;

error:
    while (count > 0)
    {
        count--;
        free(headers[count][1]);
        free(headers[count][0]);
    }
    return -1;
}

/*** Test cases ***/
#ifdef DEC_TEST
# include <stdarg.h>
# include <stdio.h>

static void test_integer(unsigned n, const uint8_t *buf, size_t len,
                         int_fast32_t value)
{
    printf("%s(%u, %zu byte(s))...\n", __func__, n, len);

    /* Check too short buffers */
    for (size_t i = 1; i < len; i++)
    {
        const uint8_t *cutbuf = buf;
        size_t cutlen = i;
        assert(hpack_decode_int(n, &cutbuf, &cutlen) == -1);
    }

    /* Check succesful decoding */
    const uint8_t *end = buf + len;
    int_fast32_t v = hpack_decode_int(n, &buf, &len);

    assert(v == value);
    assert(buf == end);
    assert(len == 0);
}

static void test_integers(void)
{
    /* Decoding 10 using a 5-bits prefix */
    for (unsigned i = 0; i < 8; i++)
    {
        uint8_t data[1] = { (i << 5) | 0xA };
        test_integer(5, data, 1, 10);
    }

    /* Decoding 1337 using a 5-bits prefix */
    for (unsigned i = 0; i < 8; i++)
    {
        uint8_t data[3] = { (i << 5) | 0x1F, 0x9A, 0x0A };
        test_integer(5, data, 3, 1337);
    }

    /* Decoding 42 using a 8-bits prefix */
    uint8_t data[1] = { 42 };
    test_integer(8, data, 1, 42);
}

static void test_header(const char *str, size_t len,
                        const char *name, const char *value)
{
    printf("%s(%zu bytes, \"%s\", \"%s\")...\n", __func__, len, name, value);

    struct hpack_decoder *dec = hpack_decode_init(4096);
    assert(dec != NULL);

    const uint8_t *buf = (const uint8_t *)str;
    char *n, *v;

    /* Check too short buffers */
    for (size_t i = 1; i < len; i++)
    {
        const uint8_t *cutbuf = buf;
        size_t cutlen = i;

        assert(hpack_decode_hdr(dec, &cutbuf, &cutlen, &n, &v) == -1);
    }

    /* Check succesful decoding */
    int ret = hpack_decode_hdr(dec, &buf, &len, &n, &v);
    assert(ret == 0);
    assert(!strcmp(name, n));
    assert(!strcmp(value, v));
    free(v);
    free(n);

    hpack_decode_destroy(dec);
}

static void test_headers(void)
{
    test_header("@\x0a""custom-key""\x0d""custom-header", 26,
                "custom-key", "custom-header");
    test_header("\x04\x0c""/sample/path", 14, ":path", "/sample/path");
    test_header("\x10\x08""password""\x06""secret", 17, "password", "secret");
    test_header("\x82", 1, ":method", "GET");
}

static void test_block(struct hpack_decoder *dec, const char *req, size_t len,
                       ...)
{
    printf("%s(%zu bytes)...\n", __func__, len);

    va_list ap;

    const uint8_t *buf = (const uint8_t *)req;
    char *headers[16][2];
    int count = hpack_decode(dec, buf, len, headers, 16);

    printf(" %d headers:\n", count);
    assert(count >= 0);

    va_start(ap, len);
    for (int i = 0; i < count; i++)
    {
        const char *name = va_arg(ap, const char *);
        const char *value = va_arg(ap, const char *);

        printf("  %s: %s\n", headers[i][0], headers[i][1]);
        assert(!strcmp(name, headers[i][0]));
        assert(!strcmp(value, headers[i][1]));
        free(headers[i][1]);
        free(headers[i][0]);
    }
    assert(va_arg(ap, const char *) == NULL);
}

static void test_reqs(void)
{
    struct hpack_decoder *dec = hpack_decode_init(4096);
    assert(dec != NULL);

    test_block(dec, NULL, 0, NULL);
    test_block(dec, "\x82\x86\x84\x41\x0f""www.example.com", 20,
               ":method", "GET", ":scheme", "http", ":path", "/",
               ":authority", "www.example.com", NULL);
    test_block(dec, "\x82\x86\x84\xbe\x58\x08""no-cache", 14,
               ":method", "GET", ":scheme", "http", ":path", "/",
               ":authority", "www.example.com", "cache-control", "no-cache",
               NULL);
    test_block(dec,
               "\x82\x87\x85\xbf\x40\x0a""custom-key""\x0c""custom-value", 29,
               ":method", "GET", ":scheme", "https", ":path", "/index.html",
               ":authority", "www.example.com", "custom-key", "custom-value",
               NULL);

    hpack_decode_destroy(dec);
}

static void test_reqs_huffman(void)
{
    struct hpack_decoder *dec = hpack_decode_init(4096);
    assert(dec != NULL);

    test_block(dec, "\x82\x86\x84\x41\x8c\xf1\xe3\xc2\xe5\xf2\x3a\x6b\xa0\xab"
               "\x90\xf4\xff", 17,
               ":method", "GET", ":scheme", "http", ":path", "/",
               ":authority", "www.example.com", NULL);
    test_block(dec, "\x82\x86\x84\xbe\x58\x86\xa8\xeb\x10\x64\x9c\xbf", 12,
               ":method", "GET", ":scheme", "http", ":path", "/",
               ":authority", "www.example.com", "cache-control", "no-cache",
               NULL);
    test_block(dec, "\x82\x87\x85\xbf\x40\x88\x25\xa8\x49\xe9\x5b\xa9\x7d\x7f"
               "\x89\x25\xa8\x49\xe9\x5b\xb8\xe8\xb4\xbf", 24,
               ":method", "GET", ":scheme", "https", ":path", "/index.html",
               ":authority", "www.example.com", "custom-key", "custom-value",
               NULL);

    hpack_decode_destroy(dec);
}

static void test_resps(void)
{
    struct hpack_decoder *dec = hpack_decode_init(256);
    assert(dec != NULL);

    test_block(dec, "\x48\x03""302""\x58\x07""private"
               "\x61\x1d""Mon, 21 Oct 2013 20:13:21 GMT"
               "\x6e\x17""https://www.example.com", 70,
               ":status", "302", "cache-control", "private",
               "date", "Mon, 21 Oct 2013 20:13:21 GMT",
               "location", "https://www.example.com", NULL);
    test_block(dec, "\x48\x03""307""\xc1\xc0\xbf", 8,
               ":status", "307", "cache-control", "private",
               "date", "Mon, 21 Oct 2013 20:13:21 GMT",
               "location", "https://www.example.com", NULL);
    test_block(dec, "\x88\xc1\x61\x1d""Mon, 21 Oct 2013 20:13:22 GMT"
               "\xc0\x5a\x04""gzip""\x77\x38""foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU;"
               " max-age=3600; version=1", 98,
               ":status", "200", "cache-control", "private",
               "date", "Mon, 21 Oct 2013 20:13:22 GMT",
               "location", "https://www.example.com",
               "content-encoding", "gzip",
               "set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; "
                   "max-age=3600; version=1",
               NULL);

    hpack_decode_destroy(dec);
}

static void test_resps_huffman(void)
{
    struct hpack_decoder *dec = hpack_decode_init(256);
    assert(dec != NULL);

    test_block(dec, "\x48\x82\x64\x02\x58\x85\xae\xc3\x77\x1a\x4b\x61\x96\xd0"
               "\x7a\xbe\x94\x10\x54\xd4\x44\xa8\x20\x05\x95\x04\x0b\x81\x66"
               "\xe0\x82\xa6\x2d\x1b\xff\x6e\x91\x9d\x29\xad\x17\x18\x63\xc7"
               "\x8f\x0b\x97\xc8\xe9\xae\x82\xae\x43\xd3", 54,
               ":status", "302", "cache-control", "private",
               "date", "Mon, 21 Oct 2013 20:13:21 GMT",
               "location", "https://www.example.com", NULL);
    test_block(dec, "\x48\x83\x64\x0e\xff\xc1\xc0\xbf", 8,
               ":status", "307", "cache-control", "private",
               "date", "Mon, 21 Oct 2013 20:13:21 GMT",
               "location", "https://www.example.com", NULL);
    test_block(dec, "\x88\xc1\x61\x96\xd0\x7a\xbe\x94\x10\x54\xd4\x44\xa8\x20"
               "\x05\x95\x04\x0b\x81\x66\xe0\x84\xa6\x2d\x1b\xff\xc0\x5a\x83"
               "\x9b\xd9\xab\x77\xad\x94\xe7\x82\x1d\xd7\xf2\xe6\xc7\xb3\x35"
               "\xdf\xdf\xcd\x5b\x39\x60\xd5\xaf\x27\x08\x7f\x36\x72\xc1\xab"
               "\x27\x0f\xb5\x29\x1f\x95\x87\x31\x60\x65\xc0\x03\xed\x4e\xe5"
               "\xb1\x06\x3d\x50\x07", 79,
               ":status", "200", "cache-control", "private",
               "date", "Mon, 21 Oct 2013 20:13:22 GMT",
               "location", "https://www.example.com",
               "content-encoding", "gzip",
               "set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; "
                   "max-age=3600; version=1",
               NULL);

    hpack_decode_destroy(dec);
}


int main(void)
{
    test_integers();
    test_headers();
    test_reqs();
    test_reqs_huffman();
    test_resps();
    test_resps_huffman();
}
#endif /* TEST */
