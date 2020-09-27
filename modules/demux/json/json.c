/*****************************************************************************
 * json/json.c: JSON parsing library
 *****************************************************************************
 * Copyright © 2020 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vlc_common.h>
#include <vlc_charset.h>
#include "json.h"

#ifdef WORDS_BIGENDIAN
# define ENDIAN(x) x "BE"
#else
# define ENDIAN(x) x "LE"
#endif

char *json_unescape(const char *in, size_t inlen)
{
    /* 1) Convert UTF-8 to UTF-16.
     * This will catch any invalid UTF-8 byte sequence.
     */
    size_t buflen = 2 * (inlen + 1);
    void *buf = malloc(buflen);

    if (unlikely(buf == NULL))
        return NULL;

    vlc_iconv_t hd = vlc_iconv_open(ENDIAN("UTF-16") , "UTF-8");

    if (unlikely(hd == (vlc_iconv_t)-1)) {
        free(buf);
        return NULL;
    }

    char *out = buf;
    size_t outlen = buflen;
    size_t val = vlc_iconv(hd, &in, &inlen, &out, &outlen);

    vlc_iconv_close(hd);

    if (val == (size_t)-1) {
        free(buf);
        return NULL;
    }

    /* 2) Unescape in UTF-16 (in place).
     */
    const uint16_t *in2 = buf, *end2 = in2 + ((buflen - outlen) / 2);
    uint16_t *out2 = buf;

    while (in2 < end2) {
        uint16_t c = *(in2++);

        if (c == '\\') {
            switch (*(in2++)) {
                case 'b':
                    c = '\b';
                    break;
                case 'f':
                    c = '\f';
                    break;
                case 'n':
                    c = '\n';
                    break;
                case 'r':
                    c = '\r';
                    break;
                case 't':
                    c = '\t';
                    break;
                case 'u': {
                    char hex[5] = { in2[0], in2[1], in2[2], in2[3], 0 };

                    /* Tokeniser requires 4 hex-digits, so this cannot fail. */
                    if (sscanf(hex, "%4"SCNx16, &c) != 1)
                        vlc_assert_unreachable();

                    in2 += 4;
                    break;
                }
                default:
                    /* Invalid escape is not allowed by tokeniser. */
                    vlc_assert_unreachable();
            }
	}

        assert(out2 < in2); /* Safely in place */
        *(out2++) = c;
    }

    /* 3) Convert back to UTF-8.
     * This will catch any invalid sequence of escaped UTF-16 surrogates.
     */
    char *ret = FromCharset(ENDIAN("UTF-16"), buf, (char *)out2 - (char *)buf);

    free(buf);
    return ret;
}

const struct json_value *json_get(const struct json_object *obj,
                                  const char *name)
{
    for (size_t i = 0; i < obj->count; i++)
        if (!strcmp(obj->members[i].name, name))
            return &obj->members[i].value;

    return NULL;
}

const char *json_get_str(const struct json_object *obj, const char *name)
{
    const struct json_value *v = json_get(obj, name);

    return (v != NULL && v->type == JSON_STRING) ? v->string : NULL;
}

double json_get_num(const struct json_object *obj, const char *name)
{
    const struct json_value *v = json_get(obj, name);

    return (v != NULL && v->type == JSON_NUMBER) ? v->number : NAN;
}
