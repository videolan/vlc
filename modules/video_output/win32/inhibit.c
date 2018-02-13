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
    vlc_sem_t sem;
    vlc_mutex_t mutex;
    vlc_cond_t cond;
    vlc_thread_t thread;
    bool signaled;
    unsigned int mask;
};

static void Inhibit (vlc_inhibit_t *ih, unsigned mask)
{
    vlc_inhibit_sys_t *sys = ih->p_sys;
    vlc_mutex_lock(&sys->mutex);
    sys->mask = mask;
    sys->signaled = true;
    vlc_mutex_unlock(&sys->mutex);
    vlc_cond_signal(&sys->cond);
}

static void RestoreStateOnCancel( void* p_opaque )
{
    VLC_UNUSED(p_opaque);
    SetThreadExecutionState( ES_CONTINUOUS );
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 1, NULL, 0);
}

static void* Run(void* obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t*)obj;
    vlc_inhibit_sys_t *sys = ih->p_sys;
    EXECUTION_STATE prev_state = ES_CONTINUOUS;

    vlc_sem_post(&sys->sem);
    while (true)
    {
        unsigned int mask;

        vlc_mutex_lock(&sys->mutex);
        mutex_cleanup_push(&sys->mutex);
        vlc_cleanup_push(RestoreStateOnCancel, ih);
        while (!sys->signaled)
            vlc_cond_wait(&sys->cond, &sys->mutex);
        mask = sys->mask;
        sys->signaled = false;
        vlc_mutex_unlock(&sys->mutex);
        vlc_cleanup_pop();
        vlc_cleanup_pop();

        bool suspend = (mask & VLC_INHIBIT_DISPLAY) != 0;
        if (suspend)
        {
            /* Prevent monitor from powering off */
            prev_state = SetThreadExecutionState( ES_DISPLAY_REQUIRED |
                                                  ES_SYSTEM_REQUIRED |
                                                  ES_CONTINUOUS );
            SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 0, NULL, 0);
        }
        else
        {
            SetThreadExecutionState( prev_state );
            SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 1, NULL, 0);
        }
    }
    vlc_assert_unreachable();
}

static void CloseInhibit (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t*)obj;
    vlc_inhibit_sys_t* sys = ih->p_sys;
    vlc_cancel(sys->thread);
    vlc_join(sys->thread, NULL);
    vlc_cond_destroy(&sys->cond);
    vlc_mutex_destroy(&sys->mutex);
    vlc_sem_destroy(&sys->sem);
}

static int OpenInhibit (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *sys = ih->p_sys =
            vlc_obj_malloc(obj, sizeof(vlc_inhibit_sys_t));
    if (unlikely(ih->p_sys == NULL))
        return VLC_ENOMEM;

    vlc_sem_init(&sys->sem, 0);
    vlc_mutex_init(&sys->mutex);
    vlc_cond_init(&sys->cond);
    sys->signaled = false;

    /* SetThreadExecutionState always needs to be called from the same thread */
    if (vlc_clone(&sys->thread, Run, ih, VLC_THREAD_PRIORITY_LOW))
    {
        vlc_cond_destroy(&sys->cond);
        vlc_mutex_destroy(&sys->mutex);
        vlc_sem_destroy(&sys->sem);
        return VLC_EGENERIC;
    }

    vlc_sem_wait(&sys->sem);

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
