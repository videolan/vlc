/*****************************************************************************
 * vlc.c: the vlc player
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: vlc.c,v 1.2 2002/06/27 19:05:17 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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
#include <signal.h>                               /* SIGHUP, SIGINT, SIGKILL */
#include <stdio.h>                                              /* fprintf() */
#include <stdlib.h>                                  /* putenv(), strtol(),  */

#include <vlc/vlc.h>

/*****************************************************************************
 * main: parse command line, start interface and spawn threads
 *****************************************************************************/
int main(int i_argc, char *ppsz_argv[], char *ppsz_env[])
{
    vlc_t *p_vlc;
    vlc_error_t err;

#ifdef SYS_LINUX
#   ifdef DEBUG
    /* Activate malloc checking routines to detect heap corruptions. */
    putenv( "MALLOC_CHECK_=2" );

    /* Disable the ugly Gnome crash dialog so that we properly segfault */
    putenv( "GNOME_DISABLE_CRASH_DIALOG=1" );
#   endif
#endif

    /* Create the vlc structure */
    p_vlc = vlc_create();
    if( p_vlc == NULL )
    {
        return -1;
    }

    /* Initialize vlc */
    err = vlc_init( p_vlc, i_argc, ppsz_argv );
    if( err != VLC_SUCCESS )
    {
        vlc_destroy( p_vlc );
        return err;
    }

    //vlc_add( p_vlc, "/home/sam/videolan/streams/mpeg/axe.mpeg" );

    /* Run vlc, in non-blocking mode */
    err = vlc_run( p_vlc );

    /* Add background interfaces */
    //{ int i; for( i=10; i--; ) vlc_add_intf( p_vlc, "dummy", 0 ); }
    //vlc_add_intf( p_vlc, "dummy", VLC_FALSE );
    //vlc_add_intf( p_vlc, "logger", VLC_FALSE );
    vlc_add_intf( p_vlc, "rc", VLC_FALSE );

    /* Add a blocking interface */
    err = vlc_add_intf( p_vlc, NULL, VLC_TRUE );
    if( err != VLC_SUCCESS )
    {
        vlc_end( p_vlc );
        vlc_destroy( p_vlc );
        return err;
    }

    /* Finish the interface */
    vlc_stop( p_vlc );

    /* Finish all threads */
    vlc_end( p_vlc );

    /* Destroy the vlc structure */
    vlc_destroy( p_vlc );

    return 0;
}

