/*****************************************************************************
 * win32_specific.c: Win32 specific features 
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: win32_specific.c,v 1.19 2002/11/10 18:04:24 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdlib.h>                                                /* free() */

#include <vlc/vlc.h>

#if !defined( UNDER_CE )
#   include <fcntl.h>
#   include <winsock2.h>
#endif

/*****************************************************************************
 * system_Init: initialize winsock and misc other things.
 *****************************************************************************/
void system_Init( vlc_t *p_this, int *pi_argc, char *ppsz_argv[] )
{
#if !defined( UNDER_CE )
    WSADATA Data;
    int i_err;

    /* WinSock Library Init. */
    i_err = WSAStartup( MAKEWORD( 1, 1 ), &Data );

    if( i_err )
    {
        fprintf( stderr, "error: can't initiate WinSocks, error %i\n", i_err );
    }

    /* Set the default file-translation mode */
    _fmode = _O_BINARY;
#endif
}

/*****************************************************************************
 * system_Configure: check for system specific configuration options.
 *****************************************************************************/
void system_Configure( vlc_t *p_this )
{
#if !defined( UNDER_CE )
    p_this->p_libvlc->b_fast_mutex = config_GetInt( p_this, "fast-mutex" );
    p_this->p_libvlc->i_win9x_cv = config_GetInt( p_this, "win9x-cv-method" );

    /* Raise default priority of the current process */
#ifndef ABOVE_NORMAL_PRIORITY_CLASS
#   define ABOVE_NORMAL_PRIORITY_CLASS 0x00008000
#endif
    if( !SetPriorityClass( GetCurrentProcess(),
			   ABOVE_NORMAL_PRIORITY_CLASS ) )
    {
        if( !SetPriorityClass( GetCurrentProcess(),
			       HIGH_PRIORITY_CLASS ) )
	    msg_Dbg( p_this, "can't raise process priority" );
	else
	    msg_Dbg( p_this, "raised process priority" );
    }
    else
	msg_Dbg( p_this, "raised process priority" );
#endif
}

/*****************************************************************************
 * system_End: terminate winsock.
 *****************************************************************************/
void system_End( vlc_t *p_this )
{
#if !defined( UNDER_CE )
    WSACleanup();
#endif
}
