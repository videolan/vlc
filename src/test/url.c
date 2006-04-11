/*****************************************************************************
 * url.c: Test for url encoding/decoding stuff
 *****************************************************************************
 * Copyright (C) 2006 Rémi Denis-Courmont
 * $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <vlc/vlc.h>
#include "vlc_url.h"

#include <stdio.h>
#include <stdlib.h>

void test_decode (const char *in, const char *out)
{
    char *res;

    printf ("\"%s\" -> \"%s\" ?\n", in, out);
    res = decode_URI_duplicate (in);
    if (res == NULL)
        exit (1);

    if (strcmp (res, out))
        exit (2);

    free (res);
}

int main (void)
{
    (void)setvbuf (stdout, NULL, _IONBF, 0);
    test_decode ("this_should_not_be_modified_1234",
                 "this_should_not_be_modified_1234");

    test_decode ("This+should+be+modified+1234!",
                 "This should be modified 1234!");

    test_decode ("This%20should%20be%20modified%201234!",
                 "This should be modified 1234!");

    /* tests with invalid input */
    test_decode ("%", "%");
    test_decode ("%2", "%2");
    test_decode ("%0000", "");

    /* UTF-8 tests */
    test_decode ("T%C3%a9l%c3%A9vision+%e2%82%Ac", "Télévision €");
    test_decode ("T%E9l%E9vision", "T?l?vision");
    test_decode ("%C1%94%C3%a9l%c3%A9vision", "??élévision"); /* overlong */

    return 0;
}
