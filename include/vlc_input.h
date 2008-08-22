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

#include <string.h>                                     /* strcasestr() */

struct vlc_meta_t;

/*****************************************************************************
 * input_item_t: Describes an input and is used to spawn input_thread_t objects
 *****************************************************************************/
struct info_t
{
    char *psz_name;            /**< Name of this info */
    char *psz_value;           /**< Value of the info */
};

struct info_category_t
{
    char   *psz_name;      /**< Name of this category */
    int    i_infos;        /**< Number of infos in the category */
    struct info_t **pp_infos;     /**< Pointer to an array of infos */
};

struct input_item_t
{
    VLC_GC_MEMBERS
    int        i_id;                 /**< Identifier of the item */

    char       *psz_name;            /**< text describing this item */
    char       *psz_uri;             /**< mrl of this item */
    bool       b_fixed_name;        /**< Can the interface change the name ?*/

    int        i_options;            /**< Number of input options */
    char       **ppsz_options;       /**< Array of input options */
    uint8_t    *optflagv;            /**< Some flags of input options */
    unsigned   optflagc;

    mtime_t    i_duration;           /**< Duration in milliseconds*/

    uint8_t    i_type;               /**< Type (file, disc, ...) */
    bool b_prefers_tree;             /**< Do we prefer being displayed as tree*/

    int        i_categories;         /**< Number of info categories */
    info_category_t **pp_categories; /**< Pointer to the first info category */

    int         i_es;                /**< Number of es format descriptions */
    es_format_t **es;                /**< Es formats */

    input_stats_t *p_stats;          /**< Statistics */
    int           i_nb_played;       /**< Number of times played */

    bool          b_error_when_reading;       /**< Error When Reading */

    vlc_meta_t *p_meta;

    vlc_event_manager_t event_manager;

    vlc_mutex_t lock;                 /**< Lock for the item */
};

#define ITEM_TYPE_UNKNOWN       0
#define ITEM_TYPE_FILE          1
#define ITEM_TYPE_DIRECTORY     2
#define ITEM_TYPE_DISC          3
#define ITEM_TYPE_CDDA          4
#define ITEM_TYPE_CARD          5
#define ITEM_TYPE_NET           6
#define ITEM_TYPE_PLAYLIST      7
#define ITEM_TYPE_NODE          8
#define ITEM_TYPE_NUMBER        9

VLC_EXPORT( void, input_item_CopyOptions, ( input_item_t *p_parent, input_item_t *p_child ) );
VLC_EXPORT( void, input_item_SetName, ( input_item_t *p_item, const char *psz_name ) );

/* This won't hold the item, but can tell to interested third parties
 * Like the playlist, that there is a new sub item. With this design
 * It is not the input item's responsability to keep all the ref of
 * the input item children. */
VLC_EXPORT( void, input_item_AddSubItem, ( input_item_t *p_parent, input_item_t *p_child ) );


/* Flags handled past input_item_AddOpt() */
#define VLC_INPUT_OPTION_TRUSTED 0x2

/* Flags handled within input_item_AddOpt() */
#define VLC_INPUT_OPTION_UNIQUE  0x100

VLC_EXPORT( int, input_item_AddOpt, ( input_item_t *, const char *str, unsigned flags ) );
VLC_EXPORT( int, input_item_AddOption, (input_item_t *item, const char *str) );
VLC_EXPORT( int ,input_item_AddOption, (input_item_t *item, const char *str) );
VLC_EXPORT( bool,input_item_HasErrorWhenReading, (input_item_t *item) );
VLC_EXPORT( void, input_item_SetMeta, ( input_item_t *p_i, vlc_meta_type_t meta_type, const char *psz_val ));

VLC_EXPORT( bool,input_item_HasErrorWhenReading, (input_item_t *item) );

VLC_EXPORT( bool,input_item_MetaMatch, ( input_item_t *p_i, vlc_meta_type_t meta_type, const char *psz ) );
VLC_EXPORT( char *, input_item_GetMeta, ( input_item_t *p_i, vlc_meta_type_t meta_type ) );
VLC_EXPORT( char *, input_item_GetName, ( input_item_t * p_i ) );
VLC_EXPORT( char *, input_item_GetURI, ( input_item_t * p_i ) );
VLC_EXPORT( void,   input_item_SetURI, ( input_item_t * p_i, char * psz_uri ));
VLC_EXPORT(mtime_t, input_item_GetDuration, ( input_item_t * p_i ) );
VLC_EXPORT( void,   input_item_SetDuration, ( input_item_t * p_i, mtime_t i_duration ));
VLC_EXPORT( bool,   input_item_IsPreparsed, ( input_item_t *p_i ));
VLC_EXPORT( bool,   input_item_IsArtFetched, ( input_item_t *p_i ));
VLC_EXPORT( const vlc_meta_t *, input_item_GetMetaObject, ( input_item_t *p_i ));
VLC_EXPORT( void,   input_item_MetaMerge, ( input_item_t *p_i, const vlc_meta_t * p_new_meta ));


#define input_item_SetTitle( item, b )       input_item_SetMeta( item, vlc_meta_Title, b )
#define input_item_SetArtist( item, b )      input_item_SetMeta( item, vlc_meta_Artist, b )
#define input_item_SetGenre( item, b )       input_item_SetMeta( item, vlc_meta_Genre, b )
#define input_item_SetCopyright( item, b )   input_item_SetMeta( item, vlc_meta_Copyright, b )
#define input_item_SetAlbum( item, b )       input_item_SetMeta( item, vlc_meta_Album, b )
#define input_item_SetTrackNum( item, b )    input_item_SetMeta( item, vlc_meta_TrackNumber, b )
#define input_item_SetDescription( item, b ) input_item_SetMeta( item, vlc_meta_Description, b )
#define input_item_SetRating( item, b )      input_item_SetMeta( item, vlc_meta_Rating, b )
#define input_item_SetDate( item, b )        input_item_SetMeta( item, vlc_meta_Date, b )
#define input_item_SetSetting( item, b )     input_item_SetMeta( item, vlc_meta_Setting, b )
#define input_item_SetURL( item, b )         input_item_SetMeta( item, vlc_meta_URL, b )
#define input_item_SetLanguage( item, b )    input_item_SetMeta( item, vlc_meta_Language, b )
#define input_item_SetNowPlaying( item, b )  input_item_SetMeta( item, vlc_meta_NowPlaying, b )
#define input_item_SetPublisher( item, b )   input_item_SetMeta( item, vlc_meta_Publisher, b )
#define input_item_SetEncodedBy( item, b )   input_item_SetMeta( item, vlc_meta_EncodedBy, b )
#define input_item_SetArtURL( item, b )      input_item_SetMeta( item, vlc_meta_ArtworkURL, b )
#define input_item_SetTrackID( item, b )     input_item_SetMeta( item, vlc_meta_TrackID, b )

#define input_item_GetTitle( item )          input_item_GetMeta( item, vlc_meta_Title )
#define input_item_GetArtist( item )         input_item_GetMeta( item, vlc_meta_Artist )
#define input_item_GetGenre( item )          input_item_GetMeta( item, vlc_meta_Genre )
#define input_item_GetCopyright( item )      input_item_GetMeta( item, vlc_meta_Copyright )
#define input_item_GetAlbum( item )          input_item_GetMeta( item, vlc_meta_Album )
#define input_item_GetTrackNum( item )       input_item_GetMeta( item, vlc_meta_TrackNumber )
#define input_item_GetDescription( item )    input_item_GetMeta( item, vlc_meta_Description )
#define input_item_GetRating( item )         input_item_GetMeta( item, vlc_meta_Rating )
#define input_item_GetDate( item )           input_item_GetMeta( item, vlc_meta_Date )
#define input_item_GetGetting( item )        input_item_GetMeta( item, vlc_meta_Getting )
#define input_item_GetURL( item )            input_item_GetMeta( item, vlc_meta_URL )
#define input_item_GetLanguage( item )       input_item_GetMeta( item, vlc_meta_Language )
#define input_item_GetNowPlaying( item )     input_item_GetMeta( item, vlc_meta_NowPlaying )
#define input_item_GetPublisher( item )      input_item_GetMeta( item, vlc_meta_Publisher )
#define input_item_GetEncodedBy( item )      input_item_GetMeta( item, vlc_meta_EncodedBy )
#define input_item_GetArtURL( item )         input_item_GetMeta( item, vlc_meta_ArtworkURL )
#define input_item_GetTrackID( item )        input_item_GetMeta( item, vlc_meta_TrackID )
#define input_item_GetSetting( item )        input_item_GetMeta( item, vlc_meta_Setting )

VLC_EXPORT( char *, input_item_GetInfo, ( input_item_t *p_i, const char *psz_cat,const char *psz_name ) );
VLC_EXPORT(int, input_item_AddInfo, ( input_item_t *p_i, const char *psz_cat, const char *psz_name, const char *psz_format, ... ) LIBVLC_FORMAT( 4, 5 ) );

#define input_item_New( a,b,c ) input_item_NewExt( a, b, c, 0, NULL, -1 )
#define input_item_NewExt(a,b,c,d,e,f) __input_item_NewExt( VLC_OBJECT(a),b,c,d,e,f)
VLC_EXPORT( input_item_t *, __input_item_NewExt, (vlc_object_t *, const char *, const char*, int, const char *const *, mtime_t i_duration )  );
VLC_EXPORT( input_item_t *, input_item_NewWithType, ( vlc_object_t *, const char *, const char *e, int, const char *const *, mtime_t i_duration, int ) );

#define input_item_GetById(a,b) __input_item_GetById( VLC_OBJECT(a),b )
VLC_EXPORT( input_item_t *, __input_item_GetById, (vlc_object_t *, int ) );

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

/* "state" value */
/* NOTE: you need to update ppsz_input_state in the RC interface
 * if you modify this list. */
typedef enum input_state_e
{
    INIT_S = 0,
    OPENING_S,
    BUFFERING_S,
    PLAYING_S,
    PAUSE_S,
    STOP_S,
    FORWARD_S,
    BACKWARD_S,
    END_S,
    ERROR_S,
} input_state_e;

/* "rate" default, min/max
 * A rate below 1000 plays the movie faster,
 * A rate above 1000 plays the movie slower.
 */
#define INPUT_RATE_DEFAULT  1000
#define INPUT_RATE_MIN       125            /* Up to 8/1 */
#define INPUT_RATE_MAX     32000            /* Up to 1/32 */

/* i_update field of access_t/demux_t */
#define INPUT_UPDATE_NONE       0x0000
#define INPUT_UPDATE_SIZE       0x0001
#define INPUT_UPDATE_TITLE      0x0010
#define INPUT_UPDATE_SEEKPOINT  0x0020
#define INPUT_UPDATE_META       0x0040

/* Input control XXX: internal */
#define INPUT_CONTROL_FIFO_SIZE    100

/** Get the input item for an input thread */
VLC_EXPORT(input_item_t*, input_GetItem, (input_thread_t*));

typedef struct input_thread_private_t input_thread_private_t;

/**
 * Main structure representing an input thread. This structure is mostly
 * private. The only public fields are READ-ONLY. You must use the helpers
 * to modify them
 */
struct input_thread_t
{
    VLC_COMMON_MEMBERS;

    bool  b_eof;
    bool b_preparsing;

    int i_state;
    bool b_can_pace_control;
    int64_t     i_time;     /* Current time */

    /* Internal caching common to all inputs */
    mtime_t i_pts_delay;

    /* All other data is input_thread is PRIVATE. You can't access it
     * outside of src/input */
    input_thread_private_t *p;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/* input_CreateThread
 * Release the returned input_thread_t using vlc_object_release() */
#define input_CreateThread(a,b) __input_CreateThread(VLC_OBJECT(a),b)
VLC_EXPORT( input_thread_t *, __input_CreateThread, ( vlc_object_t *, input_item_t * ) );

#define input_Preparse(a,b) __input_Preparse(VLC_OBJECT(a),b)
VLC_EXPORT( int, __input_Preparse, ( vlc_object_t *, input_item_t * ) );

#define input_Read(a,b,c) __input_Read(VLC_OBJECT(a),b, c)
VLC_EXPORT( int, __input_Read, ( vlc_object_t *, input_item_t *, bool ) );
VLC_EXPORT( void,             input_StopThread,     ( input_thread_t * ) );

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
    INPUT_GET_BYTE_POSITION,     /* arg1= int64_t *       res=    */
    INPUT_SET_BYTE_SIZE,         /* arg1= int64_t *       res=    */
    INPUT_GET_VIDEO_FPS,         /* arg1= double *        res=can fail */

    /* bookmarks */
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
    INPUT_ADD_SLAVE        /* arg1= char * */
};

VLC_EXPORT( int, input_vaControl,( input_thread_t *, int i_query, va_list  ) );
VLC_EXPORT( int, input_Control,  ( input_thread_t *, int i_query, ...  ) );

static inline input_state_e input_GetState( input_thread_t * p_input )
{
    input_state_e state = INIT_S;
    input_Control( p_input, INPUT_GET_STATE, &state );
    return state;
}
VLC_EXPORT( decoder_t *, input_DecoderNew, ( input_thread_t *, es_format_t *, bool b_force_decoder ) );
VLC_EXPORT( void, input_DecoderDelete, ( decoder_t * ) );
VLC_EXPORT( void, input_DecoderDecode,( decoder_t *, block_t * ) );

VLC_EXPORT( bool, input_AddSubtitles, ( input_thread_t *, char *, bool ) );

VLC_EXPORT( vlc_event_manager_t *, input_get_event_manager, ( input_thread_t * ) );

/**
 * This function allows to split a MRL into access, demux and path part.
 *
 *  You should not write into access and demux string as they may not point into
 * the provided buffer.
 *  The buffer provided by psz_dup will be modified.
 */
VLC_EXPORT( void, input_SplitMRL, ( const char **ppsz_access, const char **ppsz_demux, char **ppsz_path, char *psz_dup ) );

#endif
