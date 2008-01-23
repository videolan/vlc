/*****************************************************************************
 * browser_open.c: platform-independent opening of a web browser
 *****************************************************************************
 * Copyright (C) 2004 Commonwealth Scientific and Industrial Research
 *                    Organisation (CSIRO) Australia
 * Copyright (C) 2004 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Andre Pang <Andre.Pang@csiro.au>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "xstrcat.h"
#include "browser_open.h"

int browser_Open( const char *psz_url )
{
#ifdef __APPLE__
    char *psz_open_commandline;

    psz_open_commandline = strdup( "/usr/bin/open " );
    psz_open_commandline = xstrcat( psz_open_commandline, psz_url );

    return system( psz_open_commandline );

#elif defined( UNDER_CE )
    return -1;

#elif defined( WIN32 )
    char *psz_open_commandline;

    psz_open_commandline = strdup( "explorer " );
    xstrcat( psz_open_commandline, psz_url );

    return system( psz_open_commandline );

#else
    /* Assume we're on a UNIX of some sort */
    char *psz_open_commandline;
    int i_ret;

    /* Debian uses www-browser */
    psz_open_commandline = strdup( "www-browser" );
    xstrcat( psz_open_commandline, psz_url );
    i_ret = system( psz_open_commandline );

    if( i_ret == 0 ) return 0;

    free( psz_open_commandline );

    /* Try mozilla */
    psz_open_commandline = strdup( "mozilla" );
    xstrcat( psz_open_commandline, psz_url );
    return system( psz_open_commandline );
#endif
}

