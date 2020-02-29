/*****************************************************************************
 * vlc_vlm.h: VLM core structures
 *****************************************************************************
 * Copyright (C) 2000, 2001 VLC authors and VideoLAN
 *
 * Authors: Simon Latapie <garf@videolan.org>
 *          Laurent Aimar <fenrir@videolan.org>
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

#ifndef VLC_VLM_H
#define VLC_VLM_H 1

#include <vlc_input.h>

/**
 * \defgroup server VLM
 * \ingroup interface
 * VLC stream manager
 *
 * VLM is the server core in vlc that allows streaming of multiple media streams
 * at the same time. It provides broadcast, schedule and video on demand features
 * for streaming using several streaming and network protocols.
 * @{
 * \file
 * VLC stream manager interface
 */

/** VLM media */
typedef struct
{
    int64_t     id;     /*< numeric id for vlm_media_t item */
    bool  b_enabled;    /*< vlm_media_t is enabled */

    char *psz_name;     /*< descriptive name of vlm_media_t item */

    int  i_input;       /*< number of input options */
    char **ppsz_input;  /*< array of input options */

    int  i_option;      /*< number of output options */
    char **ppsz_option; /*< array of output options */

    char *psz_output;   /*< */

    struct
    {
        bool b_loop;    /*< this vlc_media_t broadcast item should loop */
    } broadcast;        /*< Broadcast specific information */

} vlm_media_t;

/** VLM media instance */
typedef struct
{
    char *psz_name;         /*< vlm media instance descriptive name */

    int64_t     i_time;     /*< vlm media instance vlm media current time */
    int64_t     i_length;   /*< vlm media instance vlm media item length */
    double      d_position; /*< vlm media instance position in stream */
    bool        b_paused;   /*< vlm media instance is paused */
    float       f_rate;     // normal is 1.0f
} vlm_media_instance_t;

#if 0
typedef struct
{

} vlm_schedule_t
#endif

/** VLM events
 * You can catch vlm event by adding a callback on the variable "intf-event"
 * of the VLM object.
 * This variable is an address that will hold a vlm_event_t* value.
 */
enum vlm_event_type_e
{
    /* */
    VLM_EVENT_MEDIA_ADDED   = 0x100,
    VLM_EVENT_MEDIA_REMOVED,
    VLM_EVENT_MEDIA_CHANGED,

    /* */
    VLM_EVENT_MEDIA_INSTANCE_STARTED    = 0x200,
    VLM_EVENT_MEDIA_INSTANCE_STOPPED,
    VLM_EVENT_MEDIA_INSTANCE_STATE,
};

typedef enum vlm_state_e
{
    VLM_INIT_S = 0,
    VLM_OPENING_S,
    VLM_PLAYING_S,
    VLM_PAUSE_S,
    VLM_END_S,
    VLM_ERROR_S,
} vlm_state_e;

typedef struct
{
    int            i_type;            /* a vlm_event_type_e value */
    int64_t        id;                /* Media ID */
    const char    *psz_name;          /* Media name */
    const char    *psz_instance_name; /* Instance name or NULL */
    vlm_state_e    input_state;       /* Input instance event type */
} vlm_event_t;

/** VLM control query */
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
    /* Change properties of an existing media (all fields but id) */
    VLM_CHANGE_MEDIA,                   /* arg1=vlm_media_t*                            res=can fail */
    /* Get 1 media by it's ID */
    VLM_GET_MEDIA,                      /* arg1=int64_t id arg2=vlm_media_t **  */
    /* Get media ID from its name */
    VLM_GET_MEDIA_ID,                   /* arg1=const char *psz_name arg2=int64_t*  */

    /* Media instance control */
    /* Get all media instances */
    VLM_GET_MEDIA_INSTANCES,            /* arg1=int64_t id arg2=vlm_media_instance_t *** arg3=int *pi_instance */
    /* Delete all media instances */
    VLM_CLEAR_MEDIA_INSTANCES,          /* arg1=int64_t id */
    /* Control broadcast instance */
    VLM_START_MEDIA_BROADCAST_INSTANCE, /* arg1=int64_t id, arg2=const char *psz_instance_name, int i_input_index  res=can fail */
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
    char *psz_name;         /*< message name */
    char *psz_value;        /*< message value */

    int           i_child;  /*< number of child messages */
    vlm_message_t **child;  /*< array of vlm_message_t */
};


#ifdef __cplusplus
extern "C" {
#endif

VLC_API vlm_t * vlm_New( libvlc_int_t *, const char *path );
VLC_API void vlm_Delete( vlm_t * );
VLC_API int vlm_ExecuteCommand( vlm_t *, const char *, vlm_message_t ** );
VLC_API int vlm_Control( vlm_t *p_vlm, int i_query, ... );

VLC_API vlm_message_t * vlm_MessageSimpleNew( const char * );
VLC_API vlm_message_t * vlm_MessageNew( const char *, const char *, ... ) VLC_FORMAT( 2, 3 );
VLC_API vlm_message_t * vlm_MessageAdd( vlm_message_t *, vlm_message_t * );
VLC_API void vlm_MessageDelete( vlm_message_t * );

/* media helpers */

/**
 * Initialize a vlm_media_t instance
 * \param p_media vlm_media_t instance to initialize
 */
static inline void vlm_media_Init( vlm_media_t *p_media )
{
    memset( p_media, 0, sizeof(vlm_media_t) );
    p_media->id = 0;    // invalid id
    p_media->psz_name = NULL;
    TAB_INIT( p_media->i_input, p_media->ppsz_input );
    TAB_INIT( p_media->i_option, p_media->ppsz_option );
    p_media->psz_output = NULL;

    p_media->broadcast.b_loop = false;
}

/**
 * Copy a vlm_media_t instance into another vlm_media_t instance
 * \param p_dst vlm_media_t instance to copy to
 * \param p_src vlm_media_t instance to copy from
 */
static inline void
#ifndef __cplusplus
vlm_media_Copy( vlm_media_t *restrict p_dst, const vlm_media_t *restrict p_src )
#else
vlm_media_Copy( vlm_media_t *p_dst, const vlm_media_t *p_src )
#endif
{
    int i;

    memset( p_dst, 0, sizeof(vlm_media_t) );
    p_dst->id = p_src->id;
    p_dst->b_enabled = p_src->b_enabled;
    if( p_src->psz_name )
        p_dst->psz_name = strdup( p_src->psz_name );

    for( i = 0; i < p_src->i_input; i++ )
        TAB_APPEND_CAST( (char**), p_dst->i_input, p_dst->ppsz_input, strdup(p_src->ppsz_input[i]) );
    for( i = 0; i < p_src->i_option; i++ )
        TAB_APPEND_CAST( (char**), p_dst->i_option, p_dst->ppsz_option, strdup(p_src->ppsz_option[i]) );

    if( p_src->psz_output )
        p_dst->psz_output = strdup( p_src->psz_output );

    p_dst->broadcast.b_loop = p_src->broadcast.b_loop;
}

/**
 * Cleanup and release memory associated with this vlm_media_t instance.
 * You still need to release p_media itself with vlm_media_Delete().
 * \param p_media vlm_media_t to cleanup
 */
static inline void vlm_media_Clean( vlm_media_t *p_media )
{
    int i;
    free( p_media->psz_name );

    for( i = 0; i < p_media->i_input; i++ )
        free( p_media->ppsz_input[i]);
    TAB_CLEAN(p_media->i_input, p_media->ppsz_input );

    for( i = 0; i < p_media->i_option; i++ )
        free( p_media->ppsz_option[i]);
    TAB_CLEAN(p_media->i_option, p_media->ppsz_option );

    free( p_media->psz_output );
}

/**
 * Allocate a new vlm_media_t instance
 * \return vlm_media_t instance
 */
static inline vlm_media_t *vlm_media_New(void)
{
    vlm_media_t *p_media = (vlm_media_t *)malloc( sizeof(vlm_media_t) );
    if( p_media )
        vlm_media_Init( p_media );
    return p_media;
}

/**
 * Delete a vlm_media_t instance
 * \param p_media vlm_media_t instance to delete
 */
static inline void vlm_media_Delete( vlm_media_t *p_media )
{
    vlm_media_Clean( p_media );
    free( p_media );
}

/**
 * Copy a vlm_media_t instance
 * \param p_src vlm_media_t instance to copy
 * \return vlm_media_t duplicate of p_src
 */
static inline vlm_media_t *vlm_media_Duplicate( vlm_media_t *p_src )
{
    vlm_media_t *p_dst = vlm_media_New();
    if( p_dst )
        vlm_media_Copy( p_dst, p_src );
    return p_dst;
}

/* media instance helpers */
/**
 * Initialize vlm_media_instance_t
 * \param p_instance vlm_media_instance_t to initialize
 */
static inline void vlm_media_instance_Init( vlm_media_instance_t *p_instance )
{
    memset( p_instance, 0, sizeof(vlm_media_instance_t) );
    p_instance->psz_name = NULL;
    p_instance->i_time = 0;
    p_instance->i_length = 0;
    p_instance->d_position = 0.0;
    p_instance->b_paused = false;
    p_instance->f_rate = 1.0f;
}

/**
 * Cleanup vlm_media_instance_t
 * \param p_instance vlm_media_instance_t to cleanup
 */
static inline void vlm_media_instance_Clean( vlm_media_instance_t *p_instance )
{
    free( p_instance->psz_name );
}

/**
 * Allocate a new vlm_media_instance_t
 * \return a new vlm_media_instance_t
 */
static inline vlm_media_instance_t *vlm_media_instance_New(void)
{
    vlm_media_instance_t *p_instance = (vlm_media_instance_t *) malloc( sizeof(vlm_media_instance_t) );
    if( p_instance )
        vlm_media_instance_Init( p_instance );
    return p_instance;
}

/**
 * Delete a vlm_media_instance_t
 * \param p_instance vlm_media_instance_t to delete
 */
static inline void vlm_media_instance_Delete( vlm_media_instance_t *p_instance )
{
    vlm_media_instance_Clean( p_instance );
    free( p_instance );
}

#ifdef __cplusplus
}
#endif

/**@}*/

#endif
