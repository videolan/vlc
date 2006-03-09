/*****************************************************************************
 * net_error.c: Network error handling
 *****************************************************************************
 * Copyright (C) 2006 Rémi Denis-Courmont
 * $Id$
 *
 * Author : Rémi Denis-Courmont <rem # videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <errno.h>
#include "network.h"

#if defined (WIN32) || defined (UNDER_CE)
const char *net_strerror( int value )
{
    /* There doesn't seem to be any portable error message generation for
     * Winsock errors. Some old versions had s_error, but it appears to be
     * gone, and is not documented.
     */

    switch( value )
    {
        /* Feel free to add any error message as you see fit */
        case WSAENETUNREACH:
            return "Destination unreachable";

        case WSAETIMEDOUT:
            return "Connection timed out";

        case WSAECONNREFUSED:
            return "Connection refused";

        default:
        {
            static char errmsg[14 + 5 + 1];
            /* Given PE don't support thread-local storage, this cannot be
             * implemented in a thread-safe manner, I'm afraid. */

            if( ((unsigned)value) > 99999 ) /* avoid overflow */
                return "Invalid error code";

            sprintf( errmsg, "Winsock error %u", (unsigned)value );
            return errmsg;
        }
    }

    return strerror( value );
}
#endif
