/*****************************************************************************
 * vlm_internal.h: Internal vlm structures
 *****************************************************************************
 * Copyright (C) 1998-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _VLM_INTERNAL_H
#define _VLM_INTERNAL_H 1

#include <vlc_vlm.h>


enum
{
	VOD_TYPE = 0,
	BROADCAST_TYPE,
	SCHEDULE_TYPE,
};

typedef struct
{
    /* instance name */
    char *psz_name;

    /* "playlist" index */
    int i_index;

    vlc_bool_t      b_sout_keep;

    input_item_t    item;
    input_thread_t  *p_input;
    sout_instance_t *p_sout;

} vlm_media_instance_t;

struct vlm_media_t
{
    vlc_bool_t b_enabled;
    int      i_type;

    /* name "media" is reserved */
    char    *psz_name;
    input_item_t item;

    /* "playlist" */
    int     i_input;
    char    **input;

    int     i_option;
    char    **option;

    char    *psz_output;

    /* only for broadcast */
    vlc_bool_t b_loop;

    /* only for vod */
    vod_media_t *vod_media;
    char *psz_vod_output;
    char *psz_mux;

    /* actual input instances */
    int                  i_instance;
    vlm_media_instance_t **instance;
};

struct vlm_schedule_t
{
    /* names "schedule" is reserved */
    char    *psz_name;
    vlc_bool_t b_enabled;
    /* list of commands to execute on date */
    int i_command;
    char **command;

    /* the date of 1st execution */
    mtime_t i_date;

    /* if != 0 repeat schedule every (period) */
    mtime_t i_period;
    /* number of times you have to repeat
       i_repeat < 0 : endless repeat     */
    int i_repeat;
};


struct vlm_t
{
    VLC_COMMON_MEMBERS

    vlc_mutex_t lock;

    int            i_media;
    vlm_media_t    **media;

    int            i_vod;
    vod_t          *vod;

    int            i_schedule;
    vlm_schedule_t **schedule;
};

#endif
