/*****************************************************************************
 * win32_specific.c: Win32 specific features 
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: win32_specific.c,v 1.2 2001/11/14 00:01:36 jlj Exp $
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
#include "defs.h"

#include <string.h>                                              /* strdup() */
#include <stdlib.h>                                                /* free() */
#include <fcntl.h>

#include <winsock2.h>

#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "win32_specific.h"

/*****************************************************************************
 * system_Init: initialize winsock.
 *****************************************************************************/
void system_Init( int *pi_argc, char *ppsz_argv[], char *ppsz_env[] )
{
    WSADATA Data;
    int i_err;

    /* WinSock Library Init. */
    i_err = WSAStartup( MAKEWORD( 1, 1 ), &Data );

    if( i_err )
    {
        fprintf( stderr, "error: can't initiate WinSocks, error %i", i_err );
    }

    _fmode = _O_BINARY;  /* sets the default file-translation mode */
}

/*****************************************************************************
 * system_End: terminate winsock.
 *****************************************************************************/
void system_End( void )
{
    WSACleanup();
}

