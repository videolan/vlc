/*****************************************************************************
 * vlc_input.h: Core input structures
 *****************************************************************************
 * Copyright (C) 1999-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

/* __ is need because conflict with <vlc/input.h> */
#ifndef VLC__INPUT_H
#define VLC__INPUT_H 1

/**
 * \file
 * This file defines functions, structures and enums for input objects in vlc
 */

#include <vlc_es.h>
#include <vlc_meta.h>
#include <vlc_epg.h>
#include <vlc_events.h>
#include <vlc_input_item.h>

#include <string.h>

/*****************************************************************************
 * Meta data helpers
 *****************************************************************************/
static inline void vlc_audio_replay_gain_MergeFromMeta( audio_replay_gain_t *p_dst,
                                                        const vlc_meta_t *p_meta )
{
    char * psz_value;

    if( !p_meta )
        return;

    if( (psz_value = (char *)vlc_dictionary_value_for_key( &p_meta->extra_tags, "REPLAYGAIN_TRACK_GAIN" )) ||
        (psz_value = (char *)vlc_dictionary_value_for_key( &p_meta->extra_tags, "RG_RADIO" )) )
    {
        p_dst->pb_gain[AUDIO_REPLAY_GAIN_TRACK] = true;
        p_dst->pf_gain[AUDIO_REPLAY_GAIN_TRACK] = atof( psz_value );
    }
    else if( (psz_value = (char *)vlc_dictionary_value_for_key( &p_meta->extra_tags, "REPLAYGAIN_TRACK_PEAK" )) ||
             (psz_value = (char *)vlc_dictionary_value_for_key( &p_meta->extra_tags, "RG_PEAK" )) )
    {
        p_dst->pb_peak[AUDIO_REPLAY_GAIN_TRACK] = true;
        p_dst->pf_peak[AUDIO_REPLAY_GAIN_TRACK] = atof( psz_value );
    }
    else if( (psz_value = (char *)vlc_dictionary_value_for_key( &p_meta->extra_tags, "REPLAYGAIN_ALBUM_GAIN" )) ||
             (psz_value = (char *)vlc_dictionary_value_for_key( &p_meta->extra_tags, "RG_AUDIOPHILE" )) )
    {
        p_dst->pb_gain[AUDIO_REPLAY_GAIN_ALBUM] = true;
        p_dst->pf_gain[AUDIO_REPLAY_GAIN_ALBUM] = atof( psz_value );
    }
    else if( (psz_value = (char *)vlc_dictionary_value_for_key( &p_meta->extra_tags, "REPLAYGAIN_ALBUM_PEAK" )) )
    {
        p_dst->pb_peak[AUDIO_REPLAY_GAIN_ALBUM] = true;
        p_dst->pf_peak[AUDIO_REPLAY_GAIN_ALBUM] = atof( psz_value );
    }
}

/*****************************************************************************
 * Seek point: (generalisation of chapters)
 *****************************************************************************/
struct seekpoint_t
{
    int64_t i_byte_offset;
    int64_t i_time_offset;
    char    *psz_name;
    int     i_level;
};

static inline seekpoint_t *vlc_seekpoint_New( void )
{
    seekpoint_t *point = (seekpoint_t*)malloc( sizeof( seekpoint_t ) );
    point->i_byte_offset =
    point->i_time_offset = -1;
    point->i_level = 0;
    point->psz_name = NULL;
    return point;
}

static inline void vlc_seekpoint_Delete( seekpoint_t *point )
{
    if( !point ) return;
    free( point->psz_name );
    free( point );
}

static inline seekpoint_t *vlc_seekpoint_Duplicate( seekpoint_t *src )
{
    seekpoint_t *point = vlc_seekpoint_New();
    if( src->psz_name ) point->psz_name = strdup( src->psz_name );
    point->i_time_offset = src->i_time_offset;
    point->i_byte_offset = src->i_byte_offset;
    return point;
}

/*****************************************************************************
 * Title:
 *****************************************************************************/
typedef struct
{
    char        *psz_name;

    bool        b_menu;      /* Is it a menu or a normal entry */

    int64_t     i_length;   /* Length(microsecond) if known, else 0 */
    int64_t     i_size;     /* Size (bytes) if known, else 0 */

    /* Title seekpoint */
    int         i_seekpoint;
    seekpoint_t **seekpoint;

} input_title_t;

static inline input_title_t *vlc_input_title_New(void)
{
    input_title_t *t = (input_title_t*)malloc( sizeof( input_title_t ) );

    t->psz_name = NULL;
    t->b_menu = false;
    t->i_length = 0;
    t->i_size   = 0;
    t->i_seekpoint = 0;
    t->seekpoint = NULL;

    return t;
}

static inline void vlc_input_title_Delete( input_title_t *t )
{
    int i;
    if( t == NULL )
        return;

    free( t->psz_name );
    for( i = 0; i < t->i_seekpoint; i++ )
    {
        free( t->seekpoint[i]->psz_name );
        free( t->seekpoint[i] );
    }
    free( t->seekpoint );
    free( t );
}

static inline input_title_t *vlc_input_title_Duplicate( input_title_t *t )
{
    input_title_t *dup = vlc_input_title_New( );
    int i;

    if( t->psz_name ) dup->psz_name = strdup( t->psz_name );
    dup->b_menu      = t->b_menu;
    dup->i_length    = t->i_length;
    dup->i_size      = t->i_size;
    dup->i_seekpoint = t->i_seekpoint;
    if( t->i_seekpoint > 0 )
    {
        dup->seekpoint = (seekpoint_t**)calloc( t->i_seekpoint,
                                                sizeof(seekpoint_t*) );

        for( i = 0; i < t->i_seekpoint; i++ )
        {
            dup->seekpoint[i] = vlc_seekpoint_Duplicate( t->seekpoint[i] );
        }
    }

    return dup;
}

/*****************************************************************************
 * Attachments
 *****************************************************************************/
struct input_attachment_t
{
    char *psz_name;
    char *psz_mime;
    char *psz_description;

    int  i_data;
    void *p_data;
};

static inline input_attachment_t *vlc_input_attachment_New( const char *psz_name,
                                                            const char *psz_mime,
                                                            const char *psz_description,
                                                            const void *p_data,
                                                            int i_data )
{
    input_attachment_t *a =
        (input_attachment_t*)malloc( sizeof(input_attachment_t) );
    if( !a )
        return NULL;
    a->psz_name = strdup( psz_name ? psz_name : "" );
    a->psz_mime = strdup( psz_mime ? psz_mime : "" );
    a->psz_description = strdup( psz_description ? psz_description : "" );
    a->i_data = i_data;
    a->p_data = NULL;
    if( i_data > 0 )
    {
        a->p_data = malloc( i_data );
        if( a->p_data && p_data )
            memcpy( a->p_data, p_data, i_data );
    }
    return a;
}
static inline input_attachment_t *vlc_input_attachment_Duplicate( const input_attachment_t *a )
{
    return vlc_input_attachment_New( a->psz_name, a->psz_mime, a->psz_description,
                                     a->p_data, a->i_data );
}
static inline void vlc_input_attachment_Delete( input_attachment_t *a )
{
    if( !a )
        return;
    free( a->psz_name );
    free( a->psz_mime );
    free( a->psz_description );
    free( a->p_data );
    free( a );
}

/*****************************************************************************
 * input defines/constants.
 *****************************************************************************/

/* i_update field of access_t/demux_t */
#define INPUT_UPDATE_NONE       0x0000
#define INPUT_UPDATE_SIZE       0x0001
#define INPUT_UPDATE_TITLE      0x0010
#define INPUT_UPDATE_SEEKPOINT  0x0020
#define INPUT_UPDATE_META       0x0040
#define INPUT_UPDATE_SIGNAL     0x0080

/**
 * This defines private core storage for an input.
 */
typedef struct input_thread_private_t input_thread_private_t;

/**
 * This defines an opaque input resource handler.
 */
typedef struct input_resource_t input_resource_t;

/**
 * Main structure representing an input thread. This structure is mostly
 * private. The only public fields are READ-ONLY. You must use the helpers
 * to modify them
 */
struct input_thread_t
{
    VLC_COMMON_MEMBERS;

    bool b_eof;
    bool b_preparsing;
    bool b_dead;

    /* All other data is input_thread is PRIVATE. You can't access it
     * outside of src/input */
    input_thread_private_t *p;
};

/**
 * Record prefix string.
 * TODO make it configurable.
 */
#define INPUT_RECORD_PREFIX "vlc-record-%Y-%m-%d-%Hh%Mm%Ss-$ N-$ p"

/*****************************************************************************
 * Input events and variables
 *****************************************************************************/

/**
 * \defgroup inputvariable Input variables
 *
 * The input provides multiples variable you can write to and/or read from.
 *
 * TODO complete the documentation.
 * The read only variables are:
 *  - "length"
 *  - "can-seek" (if you can seek, it doesn't say if 'bar display' has be shown
 *    or not, for that check position != 0.0)
 *  - "can-pause"
 *  - "can-rate"
 *  - "can-rewind"
 *  - "can-record" (if a stream can be recorded while playing)
 *  - "teletext-es" (list of id from the spu tracks (spu-es) that are teletext, the
 *                   variable value being the one currently selected, -1 if no teletext)
 *  - "signal-quality"
 *  - "signal-strength"
 *  - "program-scrambled" (if the current program is scrambled)
 *  - "cache" (level of data cached [0 .. 1])
 *
 * The read-write variables are:
 *  - state (\see input_state_e)
 *  - rate, rate-slower, rate-faster
 *  - position, position-offset
 *  - time, time-offset
 *  - title, next-title, prev-title
 *  - chapter, next-chapter, next-chapter-prev
 *  - program, audio-es, video-es, spu-es
 *  - audio-delay, spu-delay
 *  - bookmark (bookmark list)
 *  - record
 *  - frame-next
 *  - navigation (list of "title %2i")
 *  - "title %2i"
 *
 * The variable used for event is
 *  - intf-event (\see input_event_type_e)
 */

/**
 * Input state
 *
 * This enum is used by the variable "state"
 */
typedef enum input_state_e
{
    INIT_S = 0,
    OPENING_S,
    PLAYING_S,
    PAUSE_S,
    END_S,
    ERROR_S,
} input_state_e;

/**
 * Input rate.
 *
 * It is an integer used by the variable "rate" in the
 * range [INPUT_RATE_MIN, INPUT_RATE_MAX] the default value
 * being INPUT_RATE_DEFAULT.
 *
 * A value lower than INPUT_RATE_DEFAULT plays faster.
 * A value higher than INPUT_RATE_DEFAULT plays slower.
 */

/**
 * Default rate value
 */
#define INPUT_RATE_DEFAULT  1000
/**
 * Minimal rate value
 */
#define INPUT_RATE_MIN        32            /* Up to 32/1 */
/**
 * Maximal rate value
 */
#define INPUT_RATE_MAX     32000            /* Up to 1/32 */

/**
 * Input events
 *
 * You can catch input event by adding a callback on the variable "intf-event".
 * This variable is an integer that will hold a input_event_type_e value.
 */
typedef enum input_event_type_e
{
    /* "state" has changed */
    INPUT_EVENT_STATE,
    /* b_dead is true */
    INPUT_EVENT_DEAD,
    /* a *user* abort has been requested */
    INPUT_EVENT_ABORT,

    /* "rate" has changed */
    INPUT_EVENT_RATE,

    /* At least one of "position" or "time" or "length" has changed */
    INPUT_EVENT_TIMES,

    /* A title has been added or removed or selected.
     * It imply that chapter has changed (not chapter event is sent) */
    INPUT_EVENT_TITLE,
    /* A chapter has been added or removed or selected. */
    INPUT_EVENT_CHAPTER,

    /* A program ("program") has been added or removed or selected,
     * or "program-scrambled" has changed.*/
    INPUT_EVENT_PROGRAM,
    /* A ES has been added or removed or selected */
    INPUT_EVENT_ES,
    /* "teletext-es" has changed */
    INPUT_EVENT_TELETEXT,

    /* "record" has changed */
    INPUT_EVENT_RECORD,

    /* input_item_t media has changed */
    INPUT_EVENT_ITEM_META,
    /* input_item_t info has changed */
    INPUT_EVENT_ITEM_INFO,
    /* input_item_t name has changed */
    INPUT_EVENT_ITEM_NAME,

    /* Input statistics have been updated */
    INPUT_EVENT_STATISTICS,
    /* At least one of "signal-quality" or "signal-strength" has changed */
    INPUT_EVENT_SIGNAL,

    /* "audio-delay" has changed */
    INPUT_EVENT_AUDIO_DELAY,
    /* "spu-delay" has changed */
    INPUT_EVENT_SUBTITLE_DELAY,

    /* "bookmark" has changed */
    INPUT_EVENT_BOOKMARK,

    /* cache" has changed */
    INPUT_EVENT_CACHE,

    /* A aout_instance_t object has been created/deleted by *the input* */
    INPUT_EVENT_AOUT,
    /* A vout_thread_t object has been created/deleted by *the input* */
    INPUT_EVENT_VOUT,

} input_event_type_e;

/**
 * Input queries
 */
enum input_query_e
{
    /* input variable "position" */
    INPUT_GET_POSITION,         /* arg1= double *       res=    */
    INPUT_SET_POSITION,         /* arg1= double         res=can fail    */

    /* input variable "length" */
    INPUT_GET_LENGTH,           /* arg1= int64_t *      res=can fail    */

    /* input variable "time" */
    INPUT_GET_TIME,             /* arg1= int64_t *      res=    */
    INPUT_SET_TIME,             /* arg1= int64_t        res=can fail    */

    /* input variable "rate" (1 is DEFAULT_RATE) */
    INPUT_GET_RATE,             /* arg1= int *          res=    */
    INPUT_SET_RATE,             /* arg1= int            res=can fail    */

    /* input variable "state" */
    INPUT_GET_STATE,            /* arg1= int *          res=    */
    INPUT_SET_STATE,            /* arg1= int            res=can fail    */

    /* input variable "audio-delay" and "sub-delay" */
    INPUT_GET_AUDIO_DELAY,      /* arg1 = int* res=can fail */
    INPUT_SET_AUDIO_DELAY,      /* arg1 = int  res=can fail */
    INPUT_GET_SPU_DELAY,        /* arg1 = int* res=can fail */
    INPUT_SET_SPU_DELAY,        /* arg1 = int  res=can fail */

    /* Meta datas */
    INPUT_ADD_INFO,   /* arg1= char* arg2= char* arg3=...     res=can fail */
    INPUT_GET_INFO,   /* arg1= char* arg2= char* arg3= char** res=can fail */
    INPUT_DEL_INFO,   /* arg1= char* arg2= char*              res=can fail */
    INPUT_SET_NAME,   /* arg1= char* res=can fail    */

    /* Input config options */
    INPUT_ADD_OPTION,      /* arg1= char * arg2= char *  res=can fail*/

    /* Input properties */
    INPUT_GET_VIDEO_FPS,         /* arg1= double *        res=can fail */

    /* bookmarks */
    INPUT_GET_BOOKMARK,    /* arg1= seekpoint_t *               res=can fail */
    INPUT_GET_BOOKMARKS,   /* arg1= seekpoint_t *** arg2= int * res=can fail */
    INPUT_CLEAR_BOOKMARKS, /* res=can fail */
    INPUT_ADD_BOOKMARK,    /* arg1= seekpoint_t *  res=can fail   */
    INPUT_CHANGE_BOOKMARK, /* arg1= seekpoint_t * arg2= int * res=can fail   */
    INPUT_DEL_BOOKMARK,    /* arg1= seekpoint_t *  res=can fail   */
    INPUT_SET_BOOKMARK,    /* arg1= int  res=can fail    */

    /* Attachments */
    INPUT_GET_ATTACHMENTS, /* arg1=input_attachment_t***, arg2=int*  res=can fail */
    INPUT_GET_ATTACHMENT,  /* arg1=input_attachment_t**, arg2=char*  res=can fail */

    /* On the fly input slave */
    INPUT_ADD_SLAVE,       /* arg1= const char * */
    INPUT_ADD_SUBTITLE,    /* arg1= const char *, arg2=bool b_check_extension */

    /* On the fly record while playing */
    INPUT_SET_RECORD_STATE, /* arg1=bool    res=can fail */
    INPUT_GET_RECORD_STATE, /* arg1=bool*   res=can fail */

    /* ES */
    INPUT_RESTART_ES,       /* arg1=int (-AUDIO/VIDEO/SPU_ES for the whole category) */

    /* Input ressources
     * XXX You must call vlc_object_release as soon as possible */
    INPUT_GET_AOUT,         /* arg1=aout_instance_t **              res=can fail */
    INPUT_GET_VOUTS,        /* arg1=vout_thread_t ***, int *        res=can fail */
};

/** @}*/

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

#define input_Create(a,b,c,d) __input_Create(VLC_OBJECT(a),b,c,d)
VLC_EXPORT( input_thread_t *, __input_Create, ( vlc_object_t *p_parent, input_item_t *, const char *psz_log, input_resource_t * ) );

#define input_CreateAndStart(a,b,c) __input_CreateAndStart(VLC_OBJECT(a),b,c)
VLC_EXPORT( input_thread_t *, __input_CreateAndStart, ( vlc_object_t *p_parent, input_item_t *, const char *psz_log ) );

VLC_EXPORT( int,  input_Start, ( input_thread_t * ) );

VLC_EXPORT( void, input_Stop, ( input_thread_t *, bool b_abort ) );

#define input_Read(a,b,c) __input_Read(VLC_OBJECT(a),b, c)
VLC_EXPORT( int, __input_Read, ( vlc_object_t *, input_item_t *, bool ) );

VLC_EXPORT( int, input_vaControl,( input_thread_t *, int i_query, va_list  ) );

VLC_EXPORT( int, input_Control,  ( input_thread_t *, int i_query, ...  ) );

/**
 * Get the input item for an input thread
 *
 * You have to keep a reference to the input or to the input_item_t until
 * you do not need it anymore.
 */
VLC_EXPORT( input_item_t*, input_GetItem, ( input_thread_t * ) );

/**
 * It will return the current state of the input.
 * Provided for convenience.
 */
static inline input_state_e input_GetState( input_thread_t * p_input )
{
    input_state_e state = INIT_S;
    input_Control( p_input, INPUT_GET_STATE, &state );
    return state;
}
/**
 * It will add a new subtitle source to the input.
 * Provided for convenience.
 */
static inline int input_AddSubtitle( input_thread_t *p_input, const char *psz_url, bool b_check_extension )
{
    return input_Control( p_input, INPUT_ADD_SUBTITLE, psz_url, b_check_extension );
}

/**
 * Return one of the video output (if any). If possible, you should use
 * INPUT_GET_VOUTS directly and process _all_ video outputs instead.
 * @param p_input an input thread from which to get a video output
 * @return NULL on error, or a video output thread pointer (which needs to be
 * released with vlc_object_release()).
 */
static inline vout_thread_t *input_GetVout( input_thread_t *p_input )
{
     vout_thread_t **pp_vout, *p_vout;
     unsigned i_vout;

     if( input_Control( p_input, INPUT_GET_VOUTS, &pp_vout, &i_vout ) )
         return NULL;

     for( unsigned i = 1; i < i_vout; i++ )
         vlc_object_release( (vlc_object_t *)(pp_vout[i]) );

     p_vout = (i_vout >= 1) ? pp_vout[0] : NULL;
     free( pp_vout );
     return p_vout;
}

/**
 * Return the audio output (if any) associated with an input.
 * @param p_input an input thread
 * @return NULL on error, or the audio output (which needs to be
 * released with vlc_object_release()).
 */
static inline aout_instance_t *input_GetAout( input_thread_t *p_input )
{
     aout_instance_t *p_aout;
     return input_Control( p_input, INPUT_GET_AOUT, &p_aout ) ? NULL : p_aout;
}

/* */
typedef struct input_clock_t input_clock_t;
VLC_EXPORT( decoder_t *, input_DecoderNew, ( input_thread_t *, es_format_t *, input_clock_t *, sout_instance_t * ) );
VLC_EXPORT( void, input_DecoderDelete, ( decoder_t * ) );
VLC_EXPORT( void, input_DecoderDecode,( decoder_t *, block_t *, bool b_do_pace ) );

/**
 * This function allows to split a MRL into access, demux and path part.
 *
 *  You should not write into access and demux string as they may not point into
 * the provided buffer.
 *  The buffer provided by psz_dup will be modified.
 */
VLC_EXPORT( void, input_SplitMRL, ( const char **ppsz_access, const char **ppsz_demux, char **ppsz_path, char *psz_dup ) );

/**
 * This function creates a sane filename path.
 */
VLC_EXPORT( char *, input_CreateFilename, ( vlc_object_t *, const char *psz_path, const char *psz_prefix, const char *psz_extension ) );

#endif
