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

#ifndef VLC_VLM_H
#define VLC_VLM_H 1

/**
 * \file
 * This file defines VLM core functions and structures in vlc
 */

#include <vlc_input.h>

/**
 * \defgroup server VLM
 * VLM is the server core in vlc that allows streaming of multiple media streams
 * at the same time. It provides broadcast, schedule and video on demand features
 * for streaming using several streaming and network protocols.
 * @{
 */

/* VLM media */
typedef struct
{
    int64_t     id;
    bool  b_enabled;

    /* */
    char *psz_name;

    /* */
    int  i_input;
    char **ppsz_input;

    int  i_option;
    char **ppsz_option;

    char *psz_output;

    /* */
    bool b_vod;
    struct
    {
        bool b_loop;
    } broadcast;
    struct
    {
        char *psz_mux;
    } vod;

} vlm_media_t;

/* VLM media instance */
typedef struct
{
    char *psz_name;

    int64_t     i_time;
    int64_t     i_length;
    double      d_position;
    bool  b_paused;
    int         i_rate;     // normal is INPUT_RATE_DEFAULT
} vlm_media_instance_t;

#if 0
typedef struct
{

} vlm_schedule_t
#endif

/* VLM control query */
enum vlm_query_e
{
    /* --- Media control */
    /* Get all medias */
    VLM_GET_MEDIAS,                     /* arg1=vlm_media_t ***, int *pi_media      */
    /* Delete all medias */
    VLM_CLEAR_MEDIAS,                   /* no arg */

    /* Add a new media */
    VLM_ADD_MEDIA,                      /* arg1=vlm_media_t* arg2=int64_t *p_id         res=can fail */
    /* Delete an existing media */
    VLM_DEL_MEDIA,                      /* arg1=int64_t id */
    /* Change properties of an existing media (all fields but id and b_vod) */
    VLM_CHANGE_MEDIA,                   /* arg1=vlm_media_t*                            res=can fail */
    /* Get 1 media by it's ID */
    VLM_GET_MEDIA,                      /* arg1=int64_t id arg2=vlm_media_t **  */
    /* Get media ID from its name */
    VLM_GET_MEDIA_ID,                   /* arg1=const char *psz_name arg2=int64_t*  */

    /* Media instance control XXX VOD control are for internal use only */
    /* Get all media instances */
    VLM_GET_MEDIA_INSTANCES,            /* arg1=int64_t id arg2=vlm_media_instance_t *** arg3=int *pi_instance */
    /* Delete all media instances */
    VLM_CLEAR_MEDIA_INSTANCES,          /* arg1=int64_t id */
    /* Control broadcast instance */
    VLM_START_MEDIA_BROADCAST_INSTANCE, /* arg1=int64_t id, arg2=const char *psz_instance_name, int i_input_index  res=can fail */
    /* Control VOD instance */
    VLM_START_MEDIA_VOD_INSTANCE,       /* arg1=int64_t id, arg2=const char *psz_instance_name, int i_input_index char *psz_vod_output res=can fail */
    /* Stop an instance */
    VLM_STOP_MEDIA_INSTANCE,            /* arg1=int64_t id, arg2=const char *psz_instance_name      res=can fail */
    /* Pause an instance */
    VLM_PAUSE_MEDIA_INSTANCE,           /* arg1=int64_t id, arg2=const char *psz_instance_name      res=can fail */
    /* Get instance position time (in microsecond) */
    VLM_GET_MEDIA_INSTANCE_TIME,        /* arg1=int64_t id, arg2=const char *psz_instance_name arg3=int64_t *   */
    /* Set instance position time (in microsecond) */
    VLM_SET_MEDIA_INSTANCE_TIME,        /* arg1=int64_t id, arg2=const char *psz_instance_name arg3=int64_t     */
    /* Get instance position ([0.0 .. 1.0]) */
    VLM_GET_MEDIA_INSTANCE_POSITION,    /* arg1=int64_t id, arg2=const char *psz_instance_name arg3=double *   */
    /* Set instance position ([0.0 .. 1.0]) */
    VLM_SET_MEDIA_INSTANCE_POSITION,    /* arg1=int64_t id, arg2=const char *psz_instance_name arg3=double     */

    /* Schedule control */
    VLM_CLEAR_SCHEDULES,                /* no arg */
    /* TODO: missing schedule control */

    /* */
};


/* VLM specific - structures and functions */

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


#ifdef __cpluplus
extern "C" {
#endif

#define vlm_New( a ) __vlm_New( VLC_OBJECT(a) )
VLC_EXPORT( vlm_t *, __vlm_New, ( vlc_object_t * ) );
VLC_EXPORT( void,      vlm_Delete, ( vlm_t * ) );
VLC_EXPORT( int,       vlm_ExecuteCommand, ( vlm_t *, const char *, vlm_message_t ** ) );
VLC_EXPORT( int,       vlm_Control, ( vlm_t *p_vlm, int i_query, ... ) );

VLC_EXPORT( vlm_message_t *, vlm_MessageNew, ( const char *, const char *, ... ) LIBVLC_FORMAT( 2, 3 ) );
VLC_EXPORT( vlm_message_t *, vlm_MessageAdd, ( vlm_message_t *, vlm_message_t * ) );
VLC_EXPORT( void,            vlm_MessageDelete, ( vlm_message_t * ) );

/* media helpers */
static inline void vlm_media_Init( vlm_media_t *p_media )
{
    memset( p_media, 0, sizeof(vlm_media_t) );
    p_media->id = 0;    // invalid id
    p_media->psz_name = NULL;
    TAB_INIT( p_media->i_input, p_media->ppsz_input );
    TAB_INIT( p_media->i_option, p_media->ppsz_option );
    p_media->psz_output = NULL;
    p_media->b_vod = false;

    p_media->vod.psz_mux = NULL;
    p_media->broadcast.b_loop = false;
}

static inline void vlm_media_Copy( vlm_media_t *p_dst, vlm_media_t *p_src )
{
    int i;

    memset( p_dst, 0, sizeof(vlm_media_t) );
    p_dst->id = p_src->id;
    p_dst->b_enabled = p_src->b_enabled;
    if( p_src->psz_name )
        p_dst->psz_name = strdup( p_src->psz_name );

    for( i = 0; i < p_src->i_input; i++ )
        TAB_APPEND_CPP( char, p_dst->i_input, p_dst->ppsz_input, strdup(p_src->ppsz_input[i]) );
    for( i = 0; i < p_src->i_option; i++ )
        TAB_APPEND_CPP( char, p_dst->i_option, p_dst->ppsz_option, strdup(p_src->ppsz_option[i]) );

    if( p_src->psz_output )
        p_dst->psz_output = strdup( p_src->psz_output );

    p_dst->b_vod = p_src->b_vod;
    if( p_src->b_vod )
    {
        if( p_src->vod.psz_mux )
            p_dst->vod.psz_mux = strdup( p_src->vod.psz_mux );
    }
    else
    {
        p_dst->broadcast.b_loop = p_src->broadcast.b_loop;
    }
}
static inline void vlm_media_Clean( vlm_media_t *p_media )
{
    int i;
    free( p_media->psz_name );

    for( i = 0; i < p_media->i_input; i++ )
        free( p_media->ppsz_input[i]) ;
    TAB_CLEAN(p_media->i_input, p_media->ppsz_input );

    for( i = 0; i < p_media->i_option; i++ )
        free( p_media->ppsz_option[i]) ;
    TAB_CLEAN(p_media->i_option, p_media->ppsz_option );

    free( p_media->psz_output );
    if( p_media->b_vod )
        free( p_media->vod.psz_mux );
}
static inline vlm_media_t *vlm_media_New(void)
{
    vlm_media_t *p_media = (vlm_media_t *)malloc( sizeof(vlm_media_t) );
    if( p_media )
        vlm_media_Init( p_media );
    return p_media;
}
static inline void vlm_media_Delete( vlm_media_t *p_media )
{
    vlm_media_Clean( p_media );
    free( p_media );
}
static inline vlm_media_t *vlm_media_Duplicate( vlm_media_t *p_src )
{
    vlm_media_t *p_dst = vlm_media_New();
    if( p_dst )
        vlm_media_Copy( p_dst, p_src );
    return p_dst;
}

/* media instance helpers */
static inline void vlm_media_instance_Init( vlm_media_instance_t *p_instance )
{
    memset( p_instance, 0, sizeof(vlm_media_instance_t) );
    p_instance->psz_name = NULL;
    p_instance->i_time = 0;
    p_instance->i_length = 0;
    p_instance->d_position = 0.0;
    p_instance->b_paused = false;
    p_instance->i_rate = INPUT_RATE_DEFAULT;
}
static inline void vlm_media_instance_Clean( vlm_media_instance_t *p_instance )
{
    free( p_instance->psz_name );
}
static inline vlm_media_instance_t *vlm_media_instance_New(void)
{
    vlm_media_instance_t *p_instance = (vlm_media_instance_t *) malloc( sizeof(vlm_media_instance_t) );
    if( p_instance )
        vlm_media_instance_Init( p_instance );
    return p_instance;
}
static inline void vlm_media_instance_Delete( vlm_media_instance_t *p_instance )
{
    vlm_media_instance_Clean( p_instance );
    free( p_instance );
}

#ifdef __cpluplus
}
#endif

/**@}*/

#endif
