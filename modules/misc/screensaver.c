/*****************************************************************************
 * screensaver.c : disable screen savers when VLC is playing
 *****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Benjamin Pracht <bigben AT videolan DOT org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_inhibit.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void  Deactivate   ( vlc_object_t * );

static void Timer( void * );
static void Inhibit( vlc_inhibit_t *, bool );

struct vlc_inhibit_sys
{
    vlc_timer_t timer;
};


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("X Screensaver disabler") )
    set_capability( "inhibit", 5 )
    set_callbacks( Activate, Deactivate )
vlc_module_end ()

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    vlc_inhibit_t *p_ih = (vlc_inhibit_t*)p_this;
    vlc_inhibit_sys_t *p_sys;

    p_sys = p_ih->p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    if( vlc_timer_create( &p_sys->timer, Timer, p_ih ) )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    p_ih->inhibit = Inhibit;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: uninitialize and cleanup
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    vlc_inhibit_t *p_ih = (vlc_inhibit_t*)p_this;
    vlc_inhibit_sys_t *p_sys = p_ih->p_sys;

    vlc_timer_destroy( p_sys->timer );

    free( p_sys );
}

static void Inhibit( vlc_inhibit_t *p_ih, bool suspend )
{
    mtime_t d = suspend ? 30*CLOCK_FREQ : 0;
    vlc_timer_schedule( p_ih->p_sys->timer, false, d, d );
}

/*****************************************************************************
 * Execute: Spawns a process using execv()
 *****************************************************************************/
static void Execute( vlc_object_t *p_this, const char *const *ppsz_args )
{
    pid_t pid = fork();
    switch( pid )
    {
        case 0:     /* we're the child */
        {
            sigset_t set;
            sigemptyset (&set);
            pthread_sigmask (SIG_SETMASK, &set, NULL);

            /* We don't want output */
            if( ( freopen( "/dev/null", "w", stdout ) != NULL )
             && ( freopen( "/dev/null", "w", stderr ) != NULL ) )
                execv( ppsz_args[0] , (char *const *)ppsz_args );
            /* If the file we want to execute doesn't exist we exit() */
            exit( EXIT_FAILURE );
        }
        case -1:    /* we're the error */
            msg_Dbg( p_this, "Couldn't fork() while launching %s",
                     ppsz_args[0] );
            break;
        default:    /* we're the parent */
            /* Wait for the child to exit.
             * We will not deadlock because we ran "/bin/sh &" */
            while( waitpid( pid, NULL, 0 ) != pid);
            break;
    }
}

/*****************************************************************************
 * Run: main thread
 *****************************************************************************
 * This part of the module is in a separate thread so that we do not have
 * too much system() overhead.
 *****************************************************************************/
static void Timer( void *data )
{
    vlc_inhibit_t *p_ih = data;

    /* If there is a playing video output, disable xscreensaver */
    /* http://www.jwz.org/xscreensaver/faq.html#dvd */
    const char *const ppsz_xsargs[] = { "/bin/sh", "-c",
        "xscreensaver-command -deactivate &", (char*)NULL };
    Execute( VLC_OBJECT(p_ih), ppsz_xsargs );

    const char *const ppsz_gsargs[] = { "/bin/sh", "-c",
        "gnome-screensaver-command --poke &", (char*)NULL };
    Execute( VLC_OBJECT(p_ih), ppsz_gsargs );
}
