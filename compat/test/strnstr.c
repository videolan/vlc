/*****************************************************************************
 * strnstr.c: Test for strnstr implementation API
 *****************************************************************************
 * Copyright © 2015 VideoLAN & VLC authors
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#include "config.h"

#include <stdbool.h>
#undef NDEBUG
#include <assert.h>
#include <string.h>

const char* haystack = "Lorem ipsum dolor sit amet";

static void test( const char* haystack, const char* needle, size_t len, bool res )
{
    if ( len == 0 )
        len = strlen( haystack );
    char* str = strnstr( haystack, needle, len );
    assert( res == ( str != NULL ) );
}

int main(void)
{
    test( haystack, "Lorem", 0, true );
    test( haystack, "Sea Otters", 0, false );
    test( haystack, "", 0, true );
    test( haystack, "Lorem ipsum dolor sit amet, but bigger", 0, false );
    test( haystack, haystack, 0, true );
    test( haystack, "amet", 0, true );
    test( haystack, "dolor", 5, false );
}
