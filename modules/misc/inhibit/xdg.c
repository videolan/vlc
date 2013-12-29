/*****************************************************************************
 * xdg-screensaver.c
 *****************************************************************************
 * Copyright (C) 2008 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_inhibit.h>
#include <assert.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>

static int Open (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname (N_("XDG-screensaver") )
    set_description (N_("XDG screen saver inhibition") )
    set_capability ("inhibit", 10 )
    set_callbacks (Open, Close)
    set_category (CAT_ADVANCED)
    set_subcategory (SUBCAT_ADVANCED_MISC)
vlc_module_end ()

struct vlc_inhibit_sys
{
    vlc_timer_t timer;
    posix_spawnattr_t attr;
};

extern char **environ;

static void Timer (void *data)
{
    vlc_inhibit_t *ih = data;
    vlc_inhibit_sys_t *sys = ih->p_sys;
    char *argv[3] = {
        (char *)"xdg-screensaver", (char *)"reset", NULL
    };
    pid_t pid;

    int err = posix_spawnp (&pid, "xdg-screensaver", NULL, &sys->attr,
                            argv, environ);
    if (err == 0)
    {
        int status;

        while (waitpid (pid, &status, 0) == -1);
    }
    else
        msg_Warn (ih, "error starting xdg-screensaver: %s",
                  vlc_strerror_c(err));
}

static void Inhibit (vlc_inhibit_t *ih, unsigned mask)
{
    vlc_inhibit_sys_t *sys = ih->p_sys;
    bool suspend = (mask & VLC_INHIBIT_DISPLAY) != 0;
    mtime_t delay = suspend ? 30 * CLOCK_FREQ : INT64_C(0);

    vlc_timer_schedule (sys->timer, false, delay, delay);
}

static int Open (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    posix_spawnattr_init (&p_sys->attr);
    /* Reset signal handlers to default and clear mask in the child process */
    {
        sigset_t set;

        sigemptyset (&set);
        posix_spawnattr_setsigmask (&p_sys->attr, &set);
        sigaddset (&set, SIGPIPE);
        posix_spawnattr_setsigdefault (&p_sys->attr, &set);
        posix_spawnattr_setflags (&p_sys->attr, POSIX_SPAWN_SETSIGDEF
                                              | POSIX_SPAWN_SETSIGMASK);
    }

    ih->p_sys = p_sys;
    if (vlc_timer_create (&p_sys->timer, Timer, ih))
    {
        posix_spawnattr_destroy (&p_sys->attr);
        free (p_sys);
        return VLC_ENOMEM;
    }

    ih->inhibit = Inhibit;
    return VLC_SUCCESS;
}

static void Close (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *p_sys = ih->p_sys;

    vlc_timer_destroy (p_sys->timer);
    posix_spawnattr_destroy (&p_sys->attr);
    free (p_sys);
}
