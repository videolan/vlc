/*****************************************************************************
 * inhibit.c: Windows video output common code
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 *
 * Authors: Steve Lhomme <robux4@ycbcr.xyz>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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
#include <vlc_inhibit.h>
#include <vlc_plugin.h>
#include <assert.h>

struct vlc_inhibit_sys
{
    vlc_mutex_t mutex;
    vlc_cond_t cond;
    vlc_thread_t thread;
    unsigned int mask;
    bool exit;
};

static void Inhibit (vlc_inhibit_t *ih, unsigned mask)
{
    vlc_inhibit_sys_t *sys = ih->p_sys;
    vlc_mutex_lock(&sys->mutex);
    sys->mask = mask;
    vlc_cond_signal(&sys->cond);
    vlc_mutex_unlock(&sys->mutex);
}

static void* Run(void* obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t*)obj;
    vlc_inhibit_sys_t *sys = ih->p_sys;

    vlc_mutex_lock(&sys->mutex);
    while (!sys->exit)
    {
        EXECUTION_STATE state = ES_CONTINUOUS;

        if (sys->mask & VLC_INHIBIT_DISPLAY)
            /* Prevent monitor from powering off */
            state |= ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED;

        SetThreadExecutionState(state);
        vlc_cond_wait(&sys->cond, &sys->mutex);
    }
    vlc_mutex_unlock(&sys->mutex);
    SetThreadExecutionState(ES_CONTINUOUS);
    return NULL;
}

static void CloseInhibit (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t*)obj;
    vlc_inhibit_sys_t* sys = ih->p_sys;
    vlc_mutex_lock(&sys->mutex);
    sys->exit = true;
    vlc_cond_signal(&sys->cond);
    vlc_mutex_unlock(&sys->mutex);
    vlc_join(sys->thread, NULL);
}

static int OpenInhibit (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *sys = ih->p_sys =
            vlc_obj_malloc(obj, sizeof(vlc_inhibit_sys_t));
    if (unlikely(ih->p_sys == NULL))
        return VLC_ENOMEM;

    vlc_mutex_init(&sys->mutex);
    vlc_cond_init(&sys->cond);
    sys->mask = 0;
    sys->exit = false;

    /* SetThreadExecutionState always needs to be called from the same thread */
    if (vlc_clone(&sys->thread, Run, ih, VLC_THREAD_PRIORITY_LOW))
        return VLC_EGENERIC;

    ih->inhibit = Inhibit;
    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_shortname (N_("Windows screensaver"))
    set_description (N_("Windows screen saver inhibition"))
    set_category (CAT_ADVANCED)
    set_subcategory (SUBCAT_ADVANCED_MISC)
    set_capability ("inhibit", 10)
    set_callbacks (OpenInhibit, CloseInhibit)
vlc_module_end ()
