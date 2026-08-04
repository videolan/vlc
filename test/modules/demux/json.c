/*****************************************************************************
 * json.c: test for the JSON parsing library
 *****************************************************************************
 * Copyright (C) 2026 Alexandre Janniaux <ajanni@videolabs.io>
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
# include "config.h"
#endif

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>

#include "../modules/demux/json/json.h"

struct reader {
    const char *buf;
    size_t len;
};

size_t json_read(void *opaque, void *buf, size_t max)
{
    struct reader *reader = opaque;
    size_t len = reader->len < max ? reader->len : max;

    memcpy(buf, reader->buf, len);
    reader->buf += len;
    reader->len -= len;
    return len;
}

void json_parse_error(void *opaque, const char *msg)
{
    (void) opaque;
    fprintf(stderr, "json parse error: %s\n", msg);
}

static int parse(const char *doc, struct json_object *obj)
{
    struct reader reader = { doc, strlen(doc) };

    return json_parse(&reader, obj);
}

/* Checks that `escaped`, used as the body of a JSON string, decodes
 * to `expected`. */
static void assert_unescapes_to(const char *escaped, const char *expected)
{
    char *doc;

    assert(asprintf(&doc, "{ \"v\": \"%s\" }", escaped) >= 0);

    struct json_object obj;
    assert(parse(doc, &obj) == 0);

    const char *value = json_get_str(&obj, "v");
    assert(value != NULL);

    if (strcmp(value, expected) != 0)
        fprintf(stderr, "unescaping \"%s\": got \"%s\", expected \"%s\"\n",
                escaped, value, expected);
    assert(strcmp(value, expected) == 0);

    json_free(&obj);
    free(doc);
}

static void test_two_character_escapes(void)
{
    assert_unescapes_to("\\\"", "\"");
    assert_unescapes_to("\\\\", "\\");
    assert_unescapes_to("\\/", "/");
    assert_unescapes_to("\\b", "\b");
    assert_unescapes_to("\\f", "\f");
    assert_unescapes_to("\\n", "\n");
    assert_unescapes_to("\\r", "\r");
    assert_unescapes_to("\\t", "\t");

    /* an escape must not swallow the character behind it */
    assert_unescapes_to("a\\\"b\\\\c\\/d", "a\"b\\c/d");
    assert_unescapes_to("\\\\\\\"", "\\\"");
    assert_unescapes_to("\\\"\\\"", "\"\"");
}

int main(void)
{
    test_two_character_escapes();
    return 0;
}
