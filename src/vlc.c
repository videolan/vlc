/*****************************************************************************
 * vlc.c: the vlc player
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: vlc.c,v 1.10 2002/08/20 18:08:51 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Lots of other people, see the libvlc AUTHORS file
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
#include <signal.h>                               /* SIGHUP, SIGINT, SIGKILL */

#include <vlc/vlc.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
#ifndef WIN32
static void SigHandler  ( int i_signal );
#endif

/*****************************************************************************
 * main: parse command line, start interface and spawn threads
 *****************************************************************************/
int main( int i_argc, char *ppsz_argv[] )
{
    vlc_error_t err;

    fprintf( stderr, COPYRIGHT_MESSAGE "\n" );

#ifdef SYS_LINUX
#   ifdef DEBUG
    /* Activate malloc checking routines to detect heap corruptions. */
    putenv( "MALLOC_CHECK_=2" );

    /* Disable the ugly Gnome crash dialog so that we properly segfault */
    putenv( "GNOME_DISABLE_CRASH_DIALOG=1" );
#   endif
#endif

    /* Create a libvlc structure */
    err = vlc_create();
    if( err != VLC_SUCCESS )
    {
        return err;
    }

#ifndef WIN32
    /* Set the signal handlers. SIGTERM is not intercepted, because we need at
     * least one method to kill the program when all other methods failed, and
     * when we don't want to use SIGKILL.
     * Note that we set the signals after the vlc_create call. */
    signal( SIGINT,  SigHandler );
    signal( SIGHUP,  SigHandler );
    signal( SIGQUIT, SigHandler );

    /* Other signals */
    signal( SIGALRM, SIG_IGN );
    signal( SIGPIPE, SIG_IGN );
#endif

    /* Initialize libvlc */
    err = vlc_init( i_argc, ppsz_argv );
    if( err != VLC_SUCCESS )
    {
        vlc_destroy();
        return err;
    }

    /* Run libvlc, in non-blocking mode */
    err = vlc_run();

    /* Add background interfaces */
#if 0
    { int i; for( i=10; i--; ) vlc_add_intf( NULL, "dummy", 0 ); }
    vlc_add_intf( NULL, "dummy", VLC_FALSE );
    vlc_add_intf( NULL, "logger", VLC_FALSE );
    vlc_add_intf( NULL, "xosd", VLC_FALSE );
    vlc_add_intf( NULL, "gtk", VLC_FALSE );
    vlc_add_intf( NULL, "kde", VLC_FALSE );
    vlc_add_intf( "rc", VLC_FALSE );
#endif

    /* Add a blocking interface and keep the return value */
    err = vlc_add_intf( NULL, VLC_TRUE );

    /* Finish the threads and destroy the libvlc structure */
    vlc_destroy();

    return err;
}

#ifndef WIN32
/*****************************************************************************
 * SigHandler: system signal handler
 *****************************************************************************
 * This function is called when a fatal signal is received by the program.
 * It tries to end the program in a clean way.
 *****************************************************************************/
static void SigHandler( int i_signal )
{
    static mtime_t abort_time = 0;
    static volatile vlc_bool_t b_die = VLC_FALSE;

    /* Once a signal has been trapped, the termination sequence will be
     * armed and subsequent signals will be ignored to avoid sending signals
     * to a libvlc structure having been destroyed */

    if( !b_die )
    {
        b_die = VLC_TRUE;
        abort_time = mdate();

        fprintf( stderr, "signal %d received, terminating vlc - do it "
                         "again in case it gets stuck\n", i_signal );

        /* Acknowledge the signal received */
        vlc_die();
    }
    else if( mdate() > abort_time + 1000000 )
    {
        /* If user asks again 1 second later, die badly */
        signal( SIGINT,  SIG_DFL );
        signal( SIGHUP,  SIG_DFL );
        signal( SIGQUIT, SIG_DFL );
        signal( SIGALRM, SIG_DFL );
        signal( SIGPIPE, SIG_DFL );

        fprintf( stderr, "user insisted too much, dying badly\n" );

        exit( 1 );
    }
}
#endif

