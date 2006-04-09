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

#undef NDEBUG
#include <assert.h>

int main (void)
{
    const char url1[] = "this_should_not_be_modified_1234";
    const char url2[] = "This+should+be+modified+1234!";
    const char url3[] = "This%20should%20be%20modified%201234!";

    char *durl = decode_URI_duplicate (url1);
    assert (durl != NULL);
    assert (!strcmp (durl, url1));
    free (durl);

    durl = decode_URI_duplicate (url2);
    assert (durl != NULL);
    assert (!strcmp (durl, "This should be modified 1234!"));
    free (durl);

    durl = decode_URI_duplicate (url3);
    assert (durl != NULL);
    assert (!strcmp (durl, "This should be modified 1234!"));
    free (durl);

    return 0;
}
