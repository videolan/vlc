/*****************************************************************************
 * vlc.c: the vlc player
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: vlc.c,v 1.15 2002/10/14 16:46:55 sam Exp $
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
#include <time.h>                                                  /* time() */

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
    int i_ret;

    fprintf( stderr, "VideoLAN Client %s\n", VLC_Version() );

#ifdef HAVE_PUTENV
#   ifdef DEBUG
    /* Activate malloc checking routines to detect heap corruptions. */
    putenv( "MALLOC_CHECK_=2" );

    /* Disable the ugly Gnome crash dialog so that we properly segfault */
    putenv( "GNOME_DISABLE_CRASH_DIALOG=1" );
#   endif

    /* If the user isn't using VLC_VERBOSE, set it to 0 by default */
    if( getenv( "VLC_VERBOSE" ) == NULL )
    {
        putenv( "VLC_VERBOSE=0" );
    }
#endif

    /* Create a libvlc structure */
    i_ret = VLC_Create();
    if( i_ret < 0 )
    {
        return i_ret;
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
    i_ret = VLC_Init( 0, i_argc, ppsz_argv );
    if( i_ret < 0 )
    {
        VLC_Destroy( 0 );
        return i_ret;
    }

    /* Run libvlc, in non-blocking mode */
    i_ret = VLC_Play( 0 );

    /* Add a blocking interface and keep the return value */
    i_ret = VLC_AddIntf( 0, NULL, VLC_TRUE );

    /* Finish the threads */
    VLC_Stop( 0 );

    /* Destroy the libvlc structure */
    VLC_Destroy( 0 );

    return i_ret;
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
    static time_t abort_time = 0;
    static volatile vlc_bool_t b_die = VLC_FALSE;

    /* Once a signal has been trapped, the termination sequence will be
     * armed and subsequent signals will be ignored to avoid sending signals
     * to a libvlc structure having been destroyed */

    if( !b_die )
    {
        b_die = VLC_TRUE;
        abort_time = time( NULL );

        fprintf( stderr, "signal %d received, terminating vlc - do it "
                         "again in case it gets stuck\n", i_signal );

        /* Acknowledge the signal received */
        VLC_Die( 0 );
    }
    else if( time( NULL ) > abort_time + 2 )
    {
        /* If user asks again 1 or 2 seconds later, die badly */
        signal( SIGINT,  SIG_DFL );
        signal( SIGHUP,  SIG_DFL );
        signal( SIGQUIT, SIG_DFL );
        signal( SIGALRM, SIG_DFL );
        signal( SIGPIPE, SIG_DFL );

        fprintf( stderr, "user insisted too much, dying badly\n" );

        abort();
    }
}
#endif

