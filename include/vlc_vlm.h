/*****************************************************************************
 * vlc_vlm.h: VLM core structures
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Simon Latapie <garf@videolan.org>
 *          Laurent Aimar <fenrir@videolan.org>
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

#ifndef _VLC_VLM_H
#define _VLC_VLM_H 1

/* VLM specific - structures and functions */
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

    input_item_t   item;
    input_thread_t *p_input;

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

/* ok, here is the structure of a vlm_message:
   The parent node is ( name_of_the_command , NULL ), or
   ( name_of_the_command , message_error ) on error.
   If a node has children, it should not have a value (=NULL).*/
struct vlm_message_t
{
    char *psz_name;
    char *psz_value;

    int           i_child;
    vlm_message_t **child;
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


#define vlm_New( a ) __vlm_New( VLC_OBJECT(a) )
VLC_EXPORT( vlm_t *, __vlm_New, ( vlc_object_t * ) );
VLC_EXPORT( void, vlm_Delete, ( vlm_t * ) );
VLC_EXPORT( int, vlm_ExecuteCommand, ( vlm_t *, const char *, vlm_message_t ** ) );
VLC_EXPORT( void, vlm_MessageDelete, ( vlm_message_t * ) );
VLC_EXPORT( vlm_media_t *, vlm_MediaNew, ( vlm_t *, const char *, int ) );
VLC_EXPORT( void, vlm_MediaDelete, ( vlm_t *, vlm_media_t *, const char * ) );
VLC_EXPORT( int, vlm_MediaSetup, ( vlm_t *, vlm_media_t *, const char *, const char * ) );
VLC_EXPORT( int, vlm_MediaControl, ( vlm_t *, vlm_media_t *, const char *, const char *, const char * ) );
VLC_EXPORT( vlm_schedule_t *, vlm_ScheduleNew, ( vlm_t *, const char * ) );
VLC_EXPORT( void, vlm_ScheduleDelete, ( vlm_t *, vlm_schedule_t *, const char * ) );
VLC_EXPORT( int, vlm_ScheduleSetup, ( vlm_schedule_t *, const char *, const char * ) );
VLC_EXPORT( int, vlm_MediaVodControl, ( void *, vod_media_t *, const char *, int, va_list ) );
VLC_EXPORT( int, vlm_Save, ( vlm_t *, const char * ) );
VLC_EXPORT( int, vlm_Load, ( vlm_t *, const char * ) );

#endif
