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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

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
#define DO_TEST_DECODE( a, b ) if( !test_decode( a, b) ) return NULL;
     DO_TEST_DECODE ("this_should_not_be_modified_1234",
                     "this_should_not_be_modified_1234");
     DO_TEST_DECODE ("This+should+be+modified+1234!",
                 "This should be modified 1234!");
     DO_TEST_DECODE ("This%20should%20be%20modified%201234!",
                 "This should be modified 1234!");
     DO_TEST_DECODE ("%7E", "~");

    /* tests with invalid input */
    DO_TEST_DECODE ("%", "%" );
    DO_TEST_DECODE ("%2", "%2");
    DO_TEST_DECODE ("%0000", "")

    /* UTF-8 tests */
    DO_TEST_DECODE ("T%C3%a9l%c3%A9vision+%e2%82%Ac", "Télévision €" );
    DO_TEST_DECODE ("T%E9l%E9vision", "T?l?vision");
    DO_TEST_DECODE ("%C1%94%C3%a9l%c3%A9vision",  "??élévision");

#define DO_TEST_B64( a, b ) if( !test_b64( a, b ) ) return NULL;
    /* Base 64 tests */
    DO_TEST_B64 ("", "") ;
    DO_TEST_B64("d", "ZA==");
    DO_TEST_B64("ab", "YWI=");
    DO_TEST_B64("abc", "YWJj");
    DO_TEST_B64 ("abcd", "YWJjZA==");

    Py_INCREF( Py_None);
    return Py_None;
}
