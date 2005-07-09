/*****************************************************************************
 * error.c: error handling routine
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>

/*****************************************************************************
 * vlc_error: strerror() equivalent
 *****************************************************************************
 * This function returns a string describing the error code passed in the
 * argument. A list of all errors can be found in include/vlc/vlc.h.
 *****************************************************************************/
char const * vlc_error ( int i_err )
{
    switch( i_err )
    {
        case VLC_SUCCESS:
            return "no error";

        case VLC_ENOMEM:
            return "not enough memory";
        case VLC_ETHREAD:
            return "thread error";
        case VLC_ETIMEOUT:
            return "timeout";

        case VLC_ENOMOD:
            return "module not found";

        case VLC_ENOOBJ:
            return "object not found";
        case VLC_EBADOBJ:
            return "bad object type";

        case VLC_ENOVAR:
            return "variable not found";
        case VLC_EBADVAR:
            return "bad variable value";

        case VLC_EEXIT:
            return "program exited";
        case VLC_EGENERIC:
            return "generic error";
        default:
            return "unknown error";
    }
}

