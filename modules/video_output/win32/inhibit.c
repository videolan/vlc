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

struct vlc_inhibit_sys
{
    EXECUTION_STATE  prev_state;
};

static void Inhibit (vlc_inhibit_t *ih, unsigned mask)
{
    vlc_inhibit_sys_t *sys = ih->p_sys;
    bool suspend = (mask & VLC_INHIBIT_DISPLAY) != 0;
    if (suspend)
        /* Prevent monitor from powering off */
        sys->prev_state =
                SetThreadExecutionState( ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_CONTINUOUS );
    else
        SetThreadExecutionState( sys->prev_state );
}

static int OpenInhibit (vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    ih->p_sys = vlc_obj_malloc(obj, sizeof(vlc_inhibit_sys_t));
    if (unlikely(ih->p_sys == NULL))
        return VLC_ENOMEM;

    ih->inhibit = Inhibit;
    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_shortname (N_("Windows screensaver"))
    set_description (N_("Windows screen saver inhibition"))
    set_category (CAT_ADVANCED)
    set_subcategory (SUBCAT_ADVANCED_MISC)
    set_capability ("inhibit", 10)
    set_callbacks (OpenInhibit, NULL)
vlc_module_end ()
