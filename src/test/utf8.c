/*****************************************************************************
 * utf8.c: Test for UTF-8 encoding/decoding stuff
 *****************************************************************************
 * Copyright (C) 2006 Rémi Denis-Courmont
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
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_charset.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static void test_towc(const char *in, size_t want_len, uint32_t want_cp)
{
    uint32_t cp;
    size_t len;

    if (want_len != (size_t)-1)
        printf("\"%s\" is U+%04"PRIX32" (%zu bytes)\n", in, want_cp, want_len);
    else
        printf("Invalid sequence of %zu bytes\n", strlen(in));

    len = vlc_towc(in, &cp);

    if (len != want_len)
    {
        printf(" ERROR: length mismatch: %zd\n", len);
        exit(1);
    }

    if (len != (size_t)-1 && want_cp != cp)
    {
        printf(" ERROR: code point mismatch: %04"PRIX32"\n", cp);
        exit(1);
    }
}

static void test (const char *in, const char *out)
{
    bool isutf8 = !strcmp (in, out);
    char *str = strdup (in);
    if (str == NULL)
        abort ();

    if (isutf8)
        printf ("\"%s\" should be accepted...\n", in);
    else
        printf ("\"%s\" should be rewritten as \"%s\"...\n", in, out);

    if ((IsUTF8 (in) != NULL) != isutf8)
    {
        printf (" ERROR: IsUTF8 (%s) failed\n", in);
        exit (1);
    }

    if ((EnsureUTF8 (str) != NULL) != isutf8)
    {
        printf (" ERROR: EnsureUTF8 (%s) failed\n", in);
        exit (2);
    }

    if (strcmp (str, out))
    {
        printf (" ERROR: got \"%s\"\n", str);
        exit (3);
    }

    if ((EnsureUTF8 (str) == NULL) || IsUTF8 (str) == NULL)
    {
        printf (" ERROR: EnsureUTF8 (%s) is not UTF-8\n", in);
        exit (4);
    }
    free (str);
}

static void test_strcasestr (const char *h, const char *n, ssize_t offset)
{
    printf ("\"%s\" should %sbe found in \"%s\"...\n", n,
            (offset != -1) ? "" : "not ", h);

    const char *ret = vlc_strcasestr (h, n);
    if (offset == -1)
    {
        if (ret != NULL)
        {
            printf ("ERROR: got \"%s\"\n", ret);
            exit (10);
        }
    }
    else
    {
        if (ret == NULL)
        {
            printf ("ERROR: not found\n");
            exit (11);
        }
        if ((ret - h) != offset)
        {
            printf ("ERROR: got \"%s\" instead of \"%s\"\n",
                    ret, h + offset);
            exit (12);
        }
    }
}


int main (void)
{
    (void)setvbuf (stdout, NULL, _IONBF, 0);

    /* Valid sequences */
    test_towc("", 0, 0);
    test_towc("\n", 1, '\n');
    test_towc("\x7F", 1, 0x7F);
    test_towc("\xC3\xA9", 2, 0xE9);
    test_towc("\xDF\xBF", 2, 0x7FF);
    test_towc("\xE2\x82\xAC", 3, 0x20AC);
    test_towc("\xEF\xBF\xBF", 3, 0xFFFF);
    test_towc("\xF0\x90\x80\x81", 4, 0x10001);
    test_towc("\xF4\x80\x80\x81", 4, 0x100001);
    test_towc("\xF4\x8F\xBF\xBF", 4, 0x10FFFF);
    /* Overlongs */
    test_towc("\xC0\x80", -1, 0);
    test_towc("\xC1\xBF", -1, 0x7F);
    test_towc("\xE0\x80\x80", -1, 0);
    test_towc("\xE0\x9F\xBF", -1, 0x7FF);
    test_towc("\xF0\x80\x80\x80", -1, 0);
    test_towc("\xF0\x8F\xBF\xBF", -1, 0xFFFF);
    /* Out of range */
    test_towc("\xF4\x90\x80\x80", -1, 0x110000);
    test_towc("\xF7\xBF\xBF\xBF", -1, 0x1FFFFF);
    /* Surrogates */
    test_towc("\xED\x9F\xBF", 3, 0xD7FF);
    test_towc("\xED\xA0\x80", -1, 0xD800);
    test_towc("\xED\xBF\xBF", -1, 0xDFFF);
    test_towc("\xEE\x80\x80", 3, 0xE000);
    /* Spurious continuation byte */
    test_towc("\x80", -1, 0);
    test_towc("\xBF", -1, 0);
    /* Missing continuation byte */
    test_towc("\xDF", -1, 0x7FF);
    test_towc("\xEF", -1, 0xFFFF);
    test_towc("\xF4", -1, 0x10FFFF);
    test_towc("\xEF\xBF", -1, 0xFFFF);
    test_towc("\xF4\xBF\xBF", -1, 0x10FFFF);

    test ("", "");
    test ("this_should_not_be_modified_1234",
          "this_should_not_be_modified_1234");

    test ("\xFF", "?"); // invalid byte
    test ("\xEF\xBB\xBFHello", "\xEF\xBB\xBFHello"); // BOM
    test ("\x00\xE9", ""); // no conversion past end of string

    test ("T\xC3\xA9l\xC3\xA9vision \xE2\x82\xAC", "Télévision €");
    test ("T\xE9l\xE9vision", "T?l?vision");
    test ("\xC1\x94\xC3\xa9l\xC3\xA9vision", "??élévision"); /* overlong */

    test ("Hel\xF0\x83\x85\x87lo", "Hel????lo"); /* more overlong */

    test_strcasestr ("", "", 0);
    test_strcasestr ("", "a", -1);
    test_strcasestr ("a", "", 0);
    test_strcasestr ("heLLo", "l", 2);
    test_strcasestr ("heLLo", "lo", 3);
    test_strcasestr ("heLLo", "llo", 2);
    test_strcasestr ("heLLo", "la", -1);
    test_strcasestr ("heLLo", "oa", -1);
    test_strcasestr ("Télé", "é", 1);
    test_strcasestr ("Télé", "élé", 1);
    test_strcasestr ("Télé", "léé", -1);

    return 0;
}
