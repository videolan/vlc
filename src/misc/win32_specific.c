/*****************************************************************************
 * win32_specific.c: Win32 specific features 
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: win32_specific.c,v 1.7 2002/04/27 22:11:22 gbazin Exp $
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

#include <videolan/vlc.h>

/*****************************************************************************
 * system_Init: initialize winsock and misc other things.
 *****************************************************************************/
void system_Init( int *pi_argc, char *ppsz_argv[], char *ppsz_env[] )
{
    WSADATA Data;
    int i_err;
    HINSTANCE hInstLib;

    /* Allocate structure */
    p_main->p_sys = malloc( sizeof( main_sys_t ) );
    if( p_main->p_sys == NULL )
    {
        intf_ErrMsg( "init error: can't create p_main->p_sys (%s)",
		     strerror(ENOMEM) );
        exit(-1);
    }

    /* dynamically get the address of SignalObjectAndWait */
    hInstLib = LoadLibrary( "kernel32" );
    p_main->p_sys->SignalObjectAndWait =
        (SIGNALOBJECTANDWAIT)GetProcAddress( hInstLib, "SignalObjectAndWait" );

    /* WinSock Library Init. */
    i_err = WSAStartup( MAKEWORD( 1, 1 ), &Data );

    if( i_err )
    {
        fprintf( stderr, "error: can't initiate WinSocks, error %i", i_err );
    }

    _fmode = _O_BINARY;  /* sets the default file-translation mode */
}

/*****************************************************************************
 * system_Configure: check for system specific configuration options.
 *****************************************************************************/
void system_Configure( void )
{
    p_main->p_sys->b_fast_pthread = config_GetIntVariable( "fast_pthread" );
}

/*****************************************************************************
 * system_End: terminate winsock.
 *****************************************************************************/
void system_End( void )
{
    WSACleanup();
}
