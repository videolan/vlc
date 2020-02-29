/*****************************************************************************
 * vlm_internal.h: Internal vlm structures
 *****************************************************************************
 * Copyright (C) 1998-2006 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef LIBVLC_VLM_INTERNAL_H
#define LIBVLC_VLM_INTERNAL_H 1

#include <vlc_vlm.h>
#include <vlc_player.h>
#include "input_interface.h"

/* Private */
typedef struct
{
    /* instance name */
    char *psz_name;

    /* "playlist" index */
    int i_index;

    vlc_object_t *p_parent;
    input_item_t      *p_item;
    vlc_player_t *player;
    vlc_player_listener_id *listener;

} vlm_media_instance_sys_t;


typedef struct
{
    struct vlc_object_t obj;
    vlm_media_t cfg;

    /* actual input instances */
    int                      i_instance;
    vlm_media_instance_sys_t **instance;
} vlm_media_sys_t;

typedef struct
{
    /* names "schedule" is reserved */
    char    *psz_name;
    bool b_enabled;
    /* list of commands to execute on date */
    int i_command;
    char **command;

    /* the date of 1st execution */
    time_t date;

    /* if != 0, repeat period in seconds */
    time_t period;
    /* number of times you have to repeat
       i_repeat < 0 : endless repeat     */
    int i_repeat;
} vlm_schedule_sys_t;


struct vlm_t
{
    struct vlc_object_t obj;

    vlc_mutex_t  lock;
    vlc_thread_t thread;
    vlc_mutex_t  lock_manage;
    vlc_cond_t   wait_manage;
    unsigned     users;

    /* tell vlm thread there is work to do */
    bool         input_state_changed;
    bool         exiting;
    /* */
    int64_t        i_id;

    /* Media list */
    int                i_media;
    vlm_media_sys_t    **media;

    /* Schedule list */
    int            i_schedule;
    vlm_schedule_sys_t **schedule;
};

int vlm_ControlInternal( vlm_t *p_vlm, int i_query, ... );
int ExecuteCommand( vlm_t *, const char *, vlm_message_t ** );
void vlm_ScheduleDelete( vlm_t *vlm, vlm_schedule_sys_t *sched );

#endif
