/*****************************************************************************
 * extras.c: Extra libc functions for some systems.
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: extras.c,v 1.2 2002/11/06 09:26:25 sam Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
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
#include <string.h>                                              /* strdup() */
#include <stdlib.h>

#include <vlc/vlc.h>

#ifndef HAVE_STRNDUP
/*****************************************************************************
 * strndup: returns a malloc'd copy of at most n bytes of string 
 * Does anyone know whether or not it will be present in Jaguar?
 *****************************************************************************/
char *strndup( const char *string, size_t n )
{
    char *psz;
    size_t len = strlen( string );

    len = __MIN( len, n ); 
    psz = (char*)malloc( len + 1 );

    if( psz != NULL )
    {
        memcpy( (void*)psz, (const void*)string, len );
        psz[ len ] = 0;
    }

    return( psz );
}
#endif /* HAVE_STRNDUP */
