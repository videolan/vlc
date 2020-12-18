/*****************************************************************************
 * control.c : vout internal control
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <vlc_vout.h>
#include "vout_internal.h"
#include "control.h"

/* */
void vout_control_Init(vout_control_t *ctrl)
{
    vlc_mutex_init(&ctrl->lock);
    vlc_cond_init(&ctrl->wait_available);

    ctrl->is_held = false;
    ctrl->forced_awake = false;
    ARRAY_INIT(ctrl->cmd);
}

void vout_control_Clean(vout_control_t *ctrl)
{
    /* */
    ARRAY_RESET(ctrl->cmd);
}

void vout_control_PushMouse(vout_control_t *ctrl, const vlc_mouse_t *video_mouse)
{
    vlc_mutex_lock(&ctrl->lock);
    ARRAY_APPEND(ctrl->cmd, *video_mouse);
    vlc_cond_signal(&ctrl->wait_available);
    vlc_mutex_unlock(&ctrl->lock);
}

void vout_control_Wake(vout_control_t *ctrl)
{
    vlc_mutex_lock(&ctrl->lock);
    ctrl->forced_awake = true;
    vlc_cond_signal(&ctrl->wait_available);
    vlc_mutex_unlock(&ctrl->lock);
}

void vout_control_Hold(vout_control_t *ctrl)
{
    vlc_mutex_lock(&ctrl->lock);
    while (ctrl->is_held)
        vlc_cond_wait(&ctrl->wait_available, &ctrl->lock);
    ctrl->is_held = true;
    vlc_mutex_unlock(&ctrl->lock);
}

void vout_control_Release(vout_control_t *ctrl)
{
    vlc_mutex_lock(&ctrl->lock);
    assert(ctrl->is_held);
    ctrl->is_held = false;
    vlc_cond_signal(&ctrl->wait_available);
    vlc_mutex_unlock(&ctrl->lock);
}

int vout_control_Pop(vout_control_t *ctrl, vlc_mouse_t *mouse, vlc_tick_t deadline)
{
    bool has_cmd = false;
    vlc_mutex_lock(&ctrl->lock);

    while (ctrl->is_held)
        // wait until code outside the vout thread loop has finished doing things
        vlc_cond_wait(&ctrl->wait_available, &ctrl->lock);

    if (ctrl->cmd.i_size <= 0) {
        /* Spurious wakeups are perfectly fine */
        if (deadline != VLC_TICK_INVALID && !ctrl->forced_awake) {
            ctrl->is_held = true;
            // wait for something to happen while blocking vout_control_Hold
            // - new mouse state received
            // - control_Wake called: new picture arrived or terminating vout
            vlc_cond_timedwait(&ctrl->wait_available, &ctrl->lock, deadline);
            // allow vout_control_Hold again
            ctrl->is_held = false;
            vlc_cond_signal(&ctrl->wait_available);
        }
    }

    if (ctrl->cmd.i_size > 0) {
        has_cmd = true;
        *mouse = ARRAY_VAL(ctrl->cmd, 0);
        ARRAY_REMOVE(ctrl->cmd, 0);
        // keep forced_awake set, if it is, so we report all mouse states we have
        // after we were awaken when a new picture has been pushed by the decoder
        // see vout_control_Wake
    } else {
        ctrl->forced_awake = false;
    }
    vlc_mutex_unlock(&ctrl->lock);

    return has_cmd ? VLC_SUCCESS : VLC_EGENERIC;
}

