/*****************************************************************************
 * win32_specific.c: Win32 specific features 
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: win32_specific.c,v 1.14 2002/08/11 08:30:01 gbazin Exp $
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
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                              /* strdup() */
#include <stdlib.h>                                                /* free() */
#include <fcntl.h>

#include <winsock2.h>

#include <vlc/vlc.h>

/*****************************************************************************
 * system_Init: initialize winsock and misc other things.
 *****************************************************************************/
void system_Init( vlc_t *p_this, int *pi_argc, char *ppsz_argv[] )
{
    WSADATA Data;
    int i_err;
    HINSTANCE hInstLib;

    /* dynamically get the address of SignalObjectAndWait */
    if( (GetVersion() < 0x80000000) )
    {
        /* We are running on NT/2K/XP, we can use SignalObjectAndWait */
        hInstLib = LoadLibrary( "kernel32" );
        if( hInstLib)
            p_this->p_vlc->SignalObjectAndWait =
                (SIGNALOBJECTANDWAIT)GetProcAddress( hInstLib,
                                                     "SignalObjectAndWait" );
    }
    else p_this->p_vlc->SignalObjectAndWait = NULL;

    /* WinSock Library Init. */
    i_err = WSAStartup( MAKEWORD( 1, 1 ), &Data );

    if( i_err )
    {
        fprintf( stderr, "error: can't initiate WinSocks, error %i\n", i_err );
    }

    p_this->p_vlc->b_fast_mutex = 0;
    p_this->p_vlc->i_win9x_cv = 0;

    _fmode = _O_BINARY;  /* sets the default file-translation mode */
}

/*****************************************************************************
 * system_Configure: check for system specific configuration options.
 *****************************************************************************/
void system_Configure( vlc_t *p_this )
{
    p_this->p_vlc->b_fast_mutex = config_GetInt( p_this, "fast-mutex" );
    p_this->p_vlc->i_win9x_cv = config_GetInt( p_this, "win9x-cv-method" );
}

/*****************************************************************************
 * system_End: terminate winsock.
 *****************************************************************************/
void system_End( vlc_t *p_this )
{
    WSACleanup();
}
