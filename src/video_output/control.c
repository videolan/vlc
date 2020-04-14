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

/* */
void vout_control_cmd_Init(vout_control_cmd_t *cmd, int type)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = type;
}

void vout_control_cmd_Clean(vout_control_cmd_t *cmd)
{
    switch (cmd->type) {
    case VOUT_CONTROL_CHANGE_FILTERS:
        free(cmd->string);
        break;
    default:
        break;
    }
}

/* */
void vout_control_Init(vout_control_t *ctrl)
{
    vlc_mutex_init(&ctrl->lock);
    vlc_cond_init(&ctrl->wait_request);
    vlc_cond_init(&ctrl->wait_available);

    ctrl->is_held = false;
    ctrl->is_waiting = false;
    ctrl->is_dead = false;
    ctrl->can_sleep = true;
    ARRAY_INIT(ctrl->cmd);
}

void vout_control_Clean(vout_control_t *ctrl)
{
    /* */
    for (int i = 0; i < ctrl->cmd.i_size; i++) {
        vout_control_cmd_t cmd = ARRAY_VAL(ctrl->cmd, i);
        vout_control_cmd_Clean(&cmd);
    }
    ARRAY_RESET(ctrl->cmd);
}

void vout_control_Dead(vout_control_t *ctrl)
{
    vlc_mutex_lock(&ctrl->lock);
    ctrl->is_dead = true;
    vlc_mutex_unlock(&ctrl->lock);
}

void vout_control_Push(vout_control_t *ctrl, vout_control_cmd_t *cmd)
{
    vlc_mutex_lock(&ctrl->lock);
    if (!ctrl->is_dead) {
        ARRAY_APPEND(ctrl->cmd, *cmd);
        vlc_cond_signal(&ctrl->wait_request);
    } else {
        vout_control_cmd_Clean(cmd);
    }
    vlc_mutex_unlock(&ctrl->lock);
}

void vout_control_Wake(vout_control_t *ctrl)
{
    vlc_mutex_lock(&ctrl->lock);
    ctrl->can_sleep = false;
    vlc_cond_signal(&ctrl->wait_request);
    vlc_mutex_unlock(&ctrl->lock);
}

void vout_control_PushVoid(vout_control_t *ctrl, int type)
{
    vout_control_cmd_t cmd;

    vout_control_cmd_Init(&cmd, type);
    vout_control_Push(ctrl, &cmd);
}
void vout_control_PushBool(vout_control_t *ctrl, int type, bool boolean)
{
    vout_control_cmd_t cmd;

    vout_control_cmd_Init(&cmd, type);
    cmd.boolean = boolean;
    vout_control_Push(ctrl, &cmd);
}

void vout_control_PushString(vout_control_t *ctrl, int type, const char *string)
{
    vout_control_cmd_t cmd;

    vout_control_cmd_Init(&cmd, type);
    cmd.string = string ? strdup(string) : NULL;
    vout_control_Push(ctrl, &cmd);
}

void vout_control_Hold(vout_control_t *ctrl)
{
    vlc_mutex_lock(&ctrl->lock);
    while (ctrl->is_held || !ctrl->is_waiting)
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

int vout_control_Pop(vout_control_t *ctrl, vout_control_cmd_t *cmd,
                     vlc_tick_t deadline)
{
    vlc_mutex_lock(&ctrl->lock);

    if (ctrl->cmd.i_size <= 0) {
        /* Spurious wakeups are perfectly fine */
        if (deadline != VLC_TICK_INVALID && ctrl->can_sleep) {
            ctrl->is_waiting = true;
            vlc_cond_signal(&ctrl->wait_available);
            vlc_cond_timedwait(&ctrl->wait_request, &ctrl->lock, deadline);
            ctrl->is_waiting = false;
        }
    }

    while (ctrl->is_held)
        vlc_cond_wait(&ctrl->wait_available, &ctrl->lock);

    bool has_cmd;
    if (ctrl->cmd.i_size > 0) {
        has_cmd = true;
        *cmd = ARRAY_VAL(ctrl->cmd, 0);
        ARRAY_REMOVE(ctrl->cmd, 0);
    } else {
        has_cmd = false;
        ctrl->can_sleep = true;
    }
    vlc_mutex_unlock(&ctrl->lock);

    return has_cmd ? VLC_SUCCESS : VLC_EGENERIC;
}

