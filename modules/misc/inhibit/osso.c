/**
 * @file osso.c
 * @brief Maemo screen unblanking for VLC media player
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_inhibit.h>

#include <libosso.h>

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("OSSO"))
    set_description (N_("OSSO screen unblanking"))
    set_category (CAT_ADVANCED)
    set_subcategory (SUBCAT_ADVANCED_MISC)
    set_capability ("inhibit", 20)
    set_callbacks (Open, Close)
vlc_module_end ()

static void Inhibit (vlc_inhibit_t *, bool);

/* We keep a single context per process */
static struct
{
    vlc_mutex_t lock;
    unsigned refs;
    unsigned suspensions;
    osso_context_t *ctx;
    vlc_timer_t timer;
} osso = {
    .lock = VLC_STATIC_MUTEX,
    .refs = 0,
    .suspensions = 0,
};

static void vlc_osso_unblank (void *dummy)
{
    (void) dummy;

    vlc_mutex_lock (&osso.lock);
    osso_display_blanking_pause (osso.ctx);
    vlc_mutex_unlock (&osso.lock);
}

#define BLANKING   (NULL)
#define UNBLANKING ((vlc_inhibit_sys_t *)ih)

static int Open (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    int ret = VLC_EGENERIC;

    vlc_mutex_lock (&osso.lock);
    if (osso.refs++ == 0)
    {
        if (vlc_timer_create (&osso.timer, vlc_osso_unblank, NULL))
            goto out;

        osso.ctx = osso_initialize (PACKAGE, VERSION, 0, NULL);
        if (osso.ctx == NULL)
        {
            vlc_timer_destroy (osso.timer);
            goto out;
        }

        msg_Dbg (obj, "initialized OSSO context");
        ret = VLC_SUCCESS;
    }
out:
    vlc_mutex_unlock (&osso.lock);

    ih->p_sys = BLANKING;
    ih->inhibit = Inhibit;
    return ret;
}

static void Close (vlc_object_t *obj)
{
    Inhibit ((vlc_inhibit_t *)obj, false);

    vlc_mutex_lock (&osso.lock);
    if (--osso.refs == 0)
    {
        msg_Dbg (obj, "deinitializing OSSO context");
        vlc_timer_destroy (osso.timer);
        osso_deinitialize (osso.ctx);
    }
    vlc_mutex_unlock (&osso.lock);
}

static void Inhibit (vlc_inhibit_t *ih, bool unblank)
{
    if (unblank == (ih->p_sys != BLANKING))
        return; /* already in right state */

    vlc_mutex_lock (&osso.lock);
    if (unblank)
    {
        /* 10 seconds is the shortest blanking interval */
        mtime_t start = (mdate() / CLOCK_FREQ + 8) * CLOCK_FREQ;
        mtime_t interval = 9 * CLOCK_FREQ;

        osso_display_state_on (osso.ctx);
        if (osso.suspensions++ == 0) /* arm timer */
            vlc_timer_schedule (osso.timer, true, start, interval);
        ih->p_sys = UNBLANKING;
    }
    else
    {
        if (--osso.suspensions == 0) /* disarm timer */
            vlc_timer_schedule (osso.timer, false, 0, 0);
        ih->p_sys = BLANKING;
    }
    vlc_mutex_unlock (&osso.lock);
}

