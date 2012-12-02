/*****************************************************************************
 * error.c: error handling routine
 *****************************************************************************
 * Copyright (C) 2002-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

/*****************************************************************************
 * vlc_error: strerror() equivalent
 *****************************************************************************
 * This function returns a string describing the error code passed in the
 * argument. A list of all errors can be found in include/vlc_common.h.
 *****************************************************************************/
char const * vlc_error ( int i_err )
{
    switch( i_err )
    {
        case VLC_SUCCESS:
            return "no error";

        case VLC_ENOMEM:
            return "not enough memory";
        case VLC_ETIMEOUT:
            return "timeout";

        case VLC_ENOMOD:
            return "module not found";

        case VLC_ENOOBJ:
            return "object not found";

        case VLC_ENOVAR:
            return "variable not found";
        case VLC_EBADVAR:
            return "bad variable value";

        case VLC_EGENERIC:
            return "generic error";
        default:
            return "unknown error";
    }
}

