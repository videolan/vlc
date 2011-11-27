/*****************************************************************************
 * utf8.c: Test for UTF-8 encoding/decoding stuff
 *****************************************************************************
 * Copyright (C) 2006 Rémi Denis-Courmont
 * $Id$
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
