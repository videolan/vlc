/*****************************************************************************
 * xscreensaver.c : disable screen savers when VLC is playing
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
#include <vlc_fs.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void  Deactivate   ( vlc_object_t * );

static void Timer( void * );
static void Inhibit( vlc_inhibit_t *, unsigned );

struct vlc_inhibit_sys
{
    vlc_timer_t timer;
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    int nullfd;
};

extern char **environ;

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

    int fd = vlc_open ("/dev/null", O_WRONLY);
    posix_spawn_file_actions_init (&p_sys->actions);
    if (fd != -1)
    {
        posix_spawn_file_actions_adddup2 (&p_sys->actions, fd, 1);
        posix_spawn_file_actions_adddup2 (&p_sys->actions, fd, 2);
        posix_spawn_file_actions_addclose (&p_sys->actions, fd);
    }
    p_sys->nullfd = fd;

    sigset_t set;
    posix_spawnattr_init (&p_sys->attr);
    sigemptyset (&set);
    posix_spawnattr_setsigmask (&p_sys->attr, &set);
   
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
    if (p_sys->nullfd != -1)
        close (p_sys->nullfd);
    posix_spawnattr_destroy (&p_sys->attr);
    posix_spawn_file_actions_destroy (&p_sys->actions);
    free( p_sys );
}

static void Inhibit( vlc_inhibit_t *p_ih, unsigned mask )
{
    bool suspend = (mask & VLC_INHIBIT_DISPLAY) != 0;
    mtime_t d = suspend ? 30*CLOCK_FREQ : 0;
    vlc_timer_schedule( p_ih->p_sys->timer, false, d, d );
}

/*****************************************************************************
 * Execute: Spawns a process using execv()
 *****************************************************************************/
static void Execute (vlc_inhibit_t *p_ih, const char *const *argv)
{
    vlc_inhibit_sys_t *p_sys = p_ih->p_sys;
    pid_t pid;

    if (posix_spawnp (&pid, argv[0], &p_sys->actions, &p_sys->attr,
                      (char **)argv, environ) == 0)
    {
        while (waitpid (pid, NULL, 0) != pid);
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
    const char *const ppsz_xsargs[] = {
        "xscreensaver-command", "-deactivate", (char*)NULL };
    Execute (p_ih, ppsz_xsargs);

    const char *const ppsz_gsargs[] = {
        "gnome-screensaver-command", "--poke", (char*)NULL };
    Execute (p_ih, ppsz_gsargs);
}
