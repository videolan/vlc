/*****************************************************************************
 * xdg-screensaver.c
 *****************************************************************************
 * Copyright (C) 2008 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_inhibit.h>
#include <spawn.h>
#include <sys/wait.h>

static int Open (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname ("XDG-screensaver" )
    set_description (N_("XDG screen saver inhibition") )
    set_capability ("inhibit", 10 )
    set_callbacks (Open, Close)
    set_category (CAT_ADVANCED)
    set_subcategory (SUBCAT_ADVANCED_MISC)
vlc_module_end ()

struct vlc_inhibit_sys
{
    pid_t pid;
    char id[11];
};

static void Inhibit (vlc_inhibit_t *ih, bool suspend);

static int Open (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    ih->p_sys = p_sys;
    ih->inhibit = Inhibit;

    p_sys->pid = 0;
    snprintf (p_sys->id, sizeof (p_sys->id), "0x%08"PRIx32, ih->window_id);

    return VLC_SUCCESS;
}

static void Close (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *p_sys = ih->p_sys;

    if (p_sys->pid)
    {
        int status;

        while (waitpid (p_sys->pid, &status, 0) == -1);
    }
    free (p_sys);
}

static void Inhibit (vlc_inhibit_t *ih, bool suspend)
{
    vlc_inhibit_sys_t *p_sys = ih->p_sys;
    pid_t pid;

    /* xdg-screensaver can take quite a while to start up (e.g. 1 second).
     * So we avoid _waiting_ for it unless we really need to (clean up). */
    /* TODO: it would be even faster to cache the current state, and
     * wait in a separate thread until the target state is reached. */
    if (p_sys->pid)
    {
        int status;

        while (waitpid (p_sys->pid, &status, 0) == -1);
        p_sys->pid = 0;
    }

    char *argv[4] = {
        (char *)"xdg-screensaver",
        suspend ? (char *)"suspend" : (char *)"resume",
        p_sys->id,
        NULL,
    };
    if (posix_spawnp (&pid, "xdg-screensaver", NULL, NULL, argv, environ) == 0)
    {
        msg_Dbg (ih, "started xdg-screensaver (PID = %d)", (int)pid);
        ih->p_sys->pid = pid;
    }
    else
        msg_Warn (ih, "could not start xdg-screensaver");
}
