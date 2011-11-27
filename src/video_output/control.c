/*****************************************************************************
 * control.c : vout internal control
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
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
#include "control.h"

/* */
void vout_control_cmd_Init(vout_control_cmd_t *cmd, int type)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = type;
}

void vout_control_cmd_Clean(vout_control_cmd_t *cmd)
{
    switch (cmd->type) {
    case VOUT_CONTROL_SUBPICTURE:
        if (cmd->u.subpicture)
            subpicture_Delete(cmd->u.subpicture);
        break;
    case VOUT_CONTROL_OSD_TITLE:
    case VOUT_CONTROL_CHANGE_FILTERS:
    case VOUT_CONTROL_CHANGE_SUB_SOURCES:
    case VOUT_CONTROL_CHANGE_SUB_FILTERS:
        free(cmd->u.string);
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
    vlc_cond_init(&ctrl->wait_acknowledge);

    ctrl->is_dead = false;
    ctrl->is_sleeping = false;
    ctrl->can_sleep = true;
    ctrl->is_processing = false;
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

    vlc_mutex_destroy(&ctrl->lock);
    vlc_cond_destroy(&ctrl->wait_request);
    vlc_cond_destroy(&ctrl->wait_acknowledge);
}

void vout_control_Dead(vout_control_t *ctrl)
{
    vlc_mutex_lock(&ctrl->lock);
    ctrl->is_dead = true;
    vlc_cond_broadcast(&ctrl->wait_acknowledge);
    vlc_mutex_unlock(&ctrl->lock);

}

void vout_control_WaitEmpty(vout_control_t *ctrl)
{
    vlc_mutex_lock(&ctrl->lock);
    while ((ctrl->cmd.i_size > 0 || ctrl->is_processing) && !ctrl->is_dead)
        vlc_cond_wait(&ctrl->wait_acknowledge, &ctrl->lock);
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
    if (ctrl->is_sleeping)
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
    cmd.u.boolean = boolean;
    vout_control_Push(ctrl, &cmd);
}
void vout_control_PushInteger(vout_control_t *ctrl, int type, int integer)
{
    vout_control_cmd_t cmd;

    vout_control_cmd_Init(&cmd, type);
    cmd.u.integer = integer;
    vout_control_Push(ctrl, &cmd);
}
void vout_control_PushTime(vout_control_t *ctrl, int type, mtime_t time)
{
    vout_control_cmd_t cmd;

    vout_control_cmd_Init(&cmd, type);
    cmd.u.time = time;
    vout_control_Push(ctrl, &cmd);
}
void vout_control_PushMessage(vout_control_t *ctrl, int type, int channel, const char *string)
{
    vout_control_cmd_t cmd;

    vout_control_cmd_Init(&cmd, type);
    cmd.u.message.channel = channel;
    cmd.u.message.string = strdup(string);
    vout_control_Push(ctrl, &cmd);
}
void vout_control_PushPair(vout_control_t *ctrl, int type, int a, int b)
{
    vout_control_cmd_t cmd;

    vout_control_cmd_Init(&cmd, type);
    cmd.u.pair.a = a;
    cmd.u.pair.b = b;
    vout_control_Push(ctrl, &cmd);
}
void vout_control_PushString(vout_control_t *ctrl, int type, const char *string)
{
    vout_control_cmd_t cmd;

    vout_control_cmd_Init(&cmd, type);
    cmd.u.string = strdup(string);
    vout_control_Push(ctrl, &cmd);
}

int vout_control_Pop(vout_control_t *ctrl, vout_control_cmd_t *cmd,
                     mtime_t deadline, mtime_t timeout)
{
    vlc_mutex_lock(&ctrl->lock);
    if (ctrl->cmd.i_size <= 0) {
        ctrl->is_processing = false;
        vlc_cond_broadcast(&ctrl->wait_acknowledge);

        const mtime_t max_deadline = mdate() + timeout;

        /* Supurious wake up are perfectly fine */
        if (deadline <= VLC_TS_INVALID) {
            ctrl->is_sleeping = true;
            if (ctrl->can_sleep)
                vlc_cond_timedwait(&ctrl->wait_request, &ctrl->lock, max_deadline);
            ctrl->is_sleeping = false;
        } else {
            vlc_cond_timedwait(&ctrl->wait_request, &ctrl->lock, __MIN(deadline, max_deadline));
        }
    }

    bool has_cmd;
    if (ctrl->cmd.i_size > 0) {
        has_cmd = true;
        *cmd = ARRAY_VAL(ctrl->cmd, 0);
        ARRAY_REMOVE(ctrl->cmd, 0);

        ctrl->is_processing = true;
    } else {
        has_cmd = false;
        ctrl->can_sleep = true;
    }
    vlc_mutex_unlock(&ctrl->lock);

    return has_cmd ? VLC_SUCCESS : VLC_EGENERIC;
}

