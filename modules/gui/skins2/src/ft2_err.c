/*****************************************************************************
 * ft2_err.c: Provide a strerror() type function for freetype2
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 *
 * Authors: JP Dinger <jpd (at) videolan (dot) org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <ft2build.h>
#include "ft2_err.h"

/* Warning: This file includes the error definitions header twice.
 * Further, freetype2 errors are not contiguous, so instead of a sparse
 * array we first look up the actual entry by linear search and only
 * then return the actual error string. It could be improved to binary
 * search (assuming the freetype2 source stays sorted), but error
 * reporting shouldn't need to be efficient.
 */

#define FT_NOERRORDEF_( sym, num, str ) num,
#define FT_ERRORDEF_(   sym, num, str ) num,

static const unsigned short ft2_errorindex[] =
{
#include FT_ERROR_DEFINITIONS_H
};

#undef  FT_NOERRORDEF_
#undef  FT_ERRORDEF_


#define FT_NOERRORDEF_( sym, num, str ) str,
#define FT_ERRORDEF_(   sym, num, str ) str,

static const char *ft2_errorstrings[] =
{
#include FT_ERROR_DEFINITIONS_H
};

#undef  FT_NOERRORDEF_
#undef  FT_ERRORDEF_

enum { ft2_num_errors = sizeof(ft2_errorindex)/sizeof(*ft2_errorindex) };

const char *ft2_strerror(unsigned err)
{
    unsigned i;
    for( i=0; i<ft2_num_errors; ++i )
        if( err==ft2_errorindex[i] )
            break;

    return i<ft2_num_errors ? ft2_errorstrings[i] :
           "An error freetype2 neglected to specify";
}
