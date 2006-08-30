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

#include "../pyunit.h"
#include <vlc/vlc.h>
#include "vlc_url.h"

typedef char * (*conv_t ) (const char *);

PyObject * test (conv_t f, const char *in, const char *out)
{
    char *res;

    printf ("\"%s\" -> \"%s\" ?\n", in, out);
    res = f(in);
    ASSERT( res != NULL, "NULL result" );
    ASSERT( strcmp( res, out ) == NULL, "" );

    Py_INCREF( Py_None );
    return Py_None;
}

static inline PyObject * test_decode( const char *in, const char *out)
{
    return test( decode_URI_duplicate, in, out );
}

static inline PyObject* test_b64( const char *in, const char *out )
{
    return test( vlc_b64_encode, in, out );
}

PyObject *url_test( PyObject *self, PyObject *args )
{
    printf( "\n" );
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

    /* Base 64 tests */
    test_b64 ("", "");
    test_b64 ("d", "ZA==");
    test_b64 ("ab", "YQG=");
    test_b64 ("abc", "YQGI");
    test_b64 ("abcd", "YQGIZA==");

    Py_INCREF( Py_None);
    return Py_None;
}
