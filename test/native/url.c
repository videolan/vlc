/*****************************************************************************
 * url.c: Test for url encoding/decoding stuff
 *****************************************************************************
 * Copyright (C) 2006 Rémi Denis-Courmont
 * $Id: url.c 15178 2006-04-11 16:18:39Z courmisch $
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

#include "../pyunit.h"
#include <vlc/vlc.h>
#include "vlc_url.h"

 PyObject * test_decode (const char *in, const char *out)
{
    char *res;

    printf ("\"%s\" -> \"%s\" ?\n", in, out);
    res = decode_URI_duplicate (in);
    ASSERT( res != NULL, "" );
    if (res == NULL)
        exit (1);

    ASSERT( strcmp( res, out ) == NULL, "" );

    Py_INCREF( Py_None );
    return Py_None;
}

 PyObject *url_decode_test( PyObject *self, PyObject *args )
{
    (void)setvbuf (stdout, NULL, _IONBF, 0);
    if( !test_decode ("this_should_not_be_modified_1234",
                     "this_should_not_be_modified_1234") ) return NULL;

    if( ! test_decode ("This+should+be+modified+1234!",
                 "This should be modified 1234!") ) return NULL;;

    if( !test_decode ("This%20should%20be%20modified%201234!",
                 "This should be modified 1234!")) return NULL;;

    if( ! test_decode ("%7E", "~")) return NULL;;

    /* tests with invalid input */
    if(!test_decode ("%", "%")) return NULL;;
    if(!test_decode ("%2", "%2")) return NULL;;
    if(!test_decode ("%0000", "")) return NULL;;

    /* UTF-8 tests */
    if(!test_decode ("T%C3%a9l%c3%A9vision+%e2%82%Ac",
                      "Télévision €" ) ) return NULL;
    if(!test_decode ("T%E9l%E9vision", "T?l?vision")) return NULL;
    if(!test_decode ("%C1%94%C3%a9l%c3%A9vision",
                         "??élévision") ) return NULL; /* overlong */

    Py_INCREF( Py_None);
    return Py_None;
}
