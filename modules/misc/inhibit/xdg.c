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
#include <vlc_spawn.h>

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

static void Timer (void *data)
{
    static const char *const argv[] = { "xdg-screensaver", "reset", NULL };
    static const int fdv[] = { -1, 2, 2, -1 };
    vlc_inhibit_t *ih = data;
    pid_t pid;

    int err = vlc_spawnp(&pid, argv[0], fdv, argv);

    if (err == 0)
        vlc_waitpid(pid);
    else
        msg_Warn (ih, "error starting xdg-screensaver: %s",
                  vlc_strerror_c(err));
}

static void Inhibit (vlc_inhibit_t *ih, unsigned mask)
{
    vlc_timer_t timer = (void *)ih->p_sys;
    bool suspend = (mask & VLC_INHIBIT_DISPLAY) != 0;
    vlc_tick_t delay = suspend ? VLC_TICK_FROM_SEC(30): VLC_TIMER_DISARM;

    vlc_timer_schedule(timer, false, delay, delay);
}

static int Open (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_timer_t timer;

    if (vlc_timer_create(&timer, Timer, ih))
        return VLC_ENOMEM;

    ih->p_sys = (void *)timer;
    ih->inhibit = Inhibit;
    return VLC_SUCCESS;
}

static void Close (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_timer_t timer = (void *)ih->p_sys;

    vlc_timer_destroy(timer);
}
