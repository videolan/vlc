/*****************************************************************************
 * vlc_input.h:
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: input_ext-intf.h 7954 2004-06-07 22:19:12Z fenrir $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/* __ is need because conflict with <vlc/input.h> */
#ifndef _VLC__INPUT_H
#define _VLC__INPUT_H 1

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
    char       *psz_name;            /**< text describing this item */
    char       *psz_uri;             /**< mrl of this item */

    int        i_options;            /**< Number of input options */
    char       **ppsz_options;       /**< Array of input options */

    mtime_t    i_duration;           /**< A hint about the duration of this
                                      * item, in milliseconds*/

    int        i_id;                 /**< Identifier of the item */
    uint8_t    i_type;               /**< Type (file, disc, ...) */

    int        i_categories;         /**< Number of info categories */
    info_category_t **pp_categories; /**< Pointer to the first info category */

    int         i_es;                /**< Number of es format descriptions */
    es_format_t **es;                /**< Pointer to an array of es formats */

    vlc_bool_t  b_fixed_name;        /**< Can the interface change the name ?*/

    vlc_mutex_t lock;                /**< Item cannot be changed without this lock */
};

#define ITEM_TYPE_UNKNOWN       0
#define ITEM_TYPE_AFILE         1
#define ITEM_TYPE_VFILE         2
#define ITEM_TYPE_DIRECTORY     3
#define ITEM_TYPE_DISC          4
#define ITEM_TYPE_CDDA          5
#define ITEM_TYPE_CARD          6
#define ITEM_TYPE_NET           7
#define ITEM_TYPE_PLAYLIST      8
#define ITEM_TYPE_NODE          9

static inline void vlc_input_item_Init( vlc_object_t *p_o, input_item_t *p_i )
{
    memset( p_i, 0, sizeof(input_item_t) );
    p_i->i_options  = 0;
    p_i->i_es = 0;
    p_i->i_categories = 0 ;
    p_i->psz_name = 0;
    p_i->psz_uri = 0;
    p_i->ppsz_options = 0;
    p_i->pp_categories = 0;
    p_i->es = 0;
    p_i->i_type = ITEM_TYPE_UNKNOWN;
    p_i->b_fixed_name = VLC_TRUE;
    vlc_mutex_init( p_o, &p_i->lock );
}

static inline void vlc_input_item_CopyOptions( input_item_t *p_parent,
                                               input_item_t *p_child )
{
    int i;
    for( i = 0 ; i< p_parent->i_options; i++ )
    {
        char *psz_option= strdup( p_parent->ppsz_options[i] );
        p_child->i_options++;
        p_child->ppsz_options = (char **)realloc( p_child->ppsz_options,
                                                  p_child->i_options *
                                                  sizeof( char * ) );
        p_child->ppsz_options[p_child->i_options-1] = psz_option;
    }
}

static inline void vlc_input_item_Clean( input_item_t *p_i )
{
    if( p_i->psz_name ) free( p_i->psz_name );
    if( p_i->psz_uri ) free( p_i->psz_uri );
    p_i->psz_name = 0;
    p_i->psz_uri = 0;

    while( p_i->i_options )
    {
        p_i->i_options--;
        if( p_i->ppsz_options[p_i->i_options] )
            free( p_i->ppsz_options[p_i->i_options] );
        if( !p_i->i_options ) free( p_i->ppsz_options );
    }

    while( p_i->i_es )
    {
        p_i->i_es--;
        es_format_Clean( p_i->es[p_i->i_es] );
        if( !p_i->i_es ) free( p_i->es );
    }

    while( p_i->i_categories )
    {
        info_category_t *p_category =
            p_i->pp_categories[--(p_i->i_categories)];

        while( p_category->i_infos )
        {
            p_category->i_infos--;

            if( p_category->pp_infos[p_category->i_infos]->psz_name )
                free( p_category->pp_infos[p_category->i_infos]->psz_name);
            if( p_category->pp_infos[p_category->i_infos]->psz_value )
                free( p_category->pp_infos[p_category->i_infos]->psz_value );
            free( p_category->pp_infos[p_category->i_infos] );

            if( !p_category->i_infos ) free( p_category->pp_infos );
        }

        if( p_category->psz_name ) free( p_category->psz_name );
        free( p_category );

        if( !p_i->i_categories ) free( p_i->pp_categories );
    }

    vlc_mutex_destroy( &p_i->lock );
}

VLC_EXPORT( char *, vlc_input_item_GetInfo, ( input_item_t *p_i, const char *psz_cat,const char *psz_name ) );
VLC_EXPORT(int, vlc_input_item_AddInfo, ( input_item_t *p_i, const char *psz_cat, const char *psz_name, const char *psz_format, ... ) );

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
    if( point->psz_name ) free( point->psz_name );
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

    vlc_bool_t  b_menu;      /* Is it a menu or a normal entry */

    int64_t     i_length;   /* Length(microsecond) if known, else 0 */
    int64_t     i_size;     /* Size (bytes) if known, else 0 */

    /* Title seekpoint */
    int         i_seekpoint;
    seekpoint_t **seekpoint;

} input_title_t;

static inline input_title_t *vlc_input_title_New( )
{
    input_title_t *t = (input_title_t*)malloc( sizeof( input_title_t ) );

    t->psz_name = NULL;
    t->b_menu = VLC_FALSE;
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

    if( t->psz_name ) free( t->psz_name );
    for( i = 0; i < t->i_seekpoint; i++ )
    {
        if( t->seekpoint[i]->psz_name ) free( t->seekpoint[i]->psz_name );
        free( t->seekpoint[i] );
    }
    if( t->seekpoint ) free( t->seekpoint );
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
 * input defines/constants.
 *****************************************************************************/

/* "state" value */
enum input_state_e
{
    INIT_S,
    PLAYING_S,
    PAUSE_S,
    END_S,
};

/* "rate" default, min/max */
#define INPUT_RATE_DEFAULT  1000
#define INPUT_RATE_MIN       125            /* Up to 8/1 */
#define INPUT_RATE_MAX      8000            /* Up to 1/8 */

/* input_source_t: gathers all information per input source */
typedef struct
{
    /* Input item description */
    input_item_t *p_item;

    /* Access/Stream/Demux plugins */
    access_t *p_access;
    stream_t *p_stream;
    demux_t  *p_demux;

    /* Title infos for that input */
    vlc_bool_t   b_title_demux; /* Titles/Seekpoints provided by demux */
    int          i_title;
    input_title_t **title;

    int i_title_offset;
    int i_seekpoint_offset;

    int i_title_start;
    int i_title_end;
    int i_seekpoint_start;
    int i_seekpoint_end;

    /* Properties */
    vlc_bool_t b_can_pace_control;
    vlc_bool_t b_can_pause;
    vlc_bool_t b_eof;   /* eof of demuxer */

    /* Clock average variation */
    int     i_cr_average;

} input_source_t;

/* i_update field of access_t/demux_t */
#define INPUT_UPDATE_NONE       0x0000
#define INPUT_UPDATE_SIZE       0x0001
#define INPUT_UPDATE_TITLE      0x0010
#define INPUT_UPDATE_SEEKPOINT  0x0020
#define INPUT_UPDATE_META       0x0040

/* Input control XXX: internal */
#define INPUT_CONTROL_FIFO_SIZE    100

/*****************************************************************************
 * input_thread_t
 *****************************************************************************
 * XXX: this strucrures is *PRIVATE* so nobody can touch it out of src/input.
 * I plan to move it to src/input/input_internal.h anyway
 *
 * XXX: look at src/input/input.c:input_CreateThread for accessible variables
 *      YOU CANNOT HAVE ACCESS TO THE CONTENT OF input_thread_t except
 *      p_input->input.p_item (and it's only temporary).
 * XXX: move the docs somewhere (better than src/input )
 *****************************************************************************/
struct input_thread_t
{
    VLC_COMMON_MEMBERS

     /* Global properties */
    vlc_bool_t  b_eof;
    vlc_bool_t  b_can_pace_control;
    vlc_bool_t  b_can_pause;

    /* Global state */
    int         i_state;
    int         i_rate;

    /* */
    int64_t     i_start;    /* :start-time,0 by default */
    int64_t     i_time;     /* Current time */
    int64_t     i_stop;     /* :stop-time, 0 if none */

    /* Title infos FIXME multi-input (not easy) ? */
    int          i_title;
    input_title_t **title;

    int i_title_offset;
    int i_seekpoint_offset;

    /* User bookmarks FIXME won't be easy with multiples input */
    int         i_bookmark;
    seekpoint_t **bookmark;

    /* Global meta datas FIXME move to input_item_t ? */
    vlc_meta_t  *p_meta;

    /* Output */
    es_out_t    *p_es_out;
    sout_instance_t *p_sout;            /* XXX Move it to es_out ? */
    vlc_bool_t      b_out_pace_control; /*     idem ? */

    /* Internal caching common for all inputs */
    int64_t i_pts_delay;

    /* Main input properties */
    input_source_t input;

    /* Slave demuxers (subs, and others) */
    int            i_slave;
    input_source_t **slave;

    /* Buffer of pending actions */
    vlc_mutex_t lock_control;
    int i_control;
    struct
    {
        /* XXX: val isn't duplicated so it won't works with string */
        int         i_type;
        vlc_value_t val;
    } control[INPUT_CONTROL_FIFO_SIZE];
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#define input_CreateThread(a,b) __input_CreateThread(VLC_OBJECT(a),b)
VLC_EXPORT( input_thread_t *, __input_CreateThread, ( vlc_object_t *, input_item_t * ) );
#define input_Preparse(a,b) __input_Preparse(VLC_OBJECT(a),b)
VLC_EXPORT( int, __input_Preparse, ( vlc_object_t *, input_item_t * ) );
VLC_EXPORT( void,             input_StopThread,     ( input_thread_t * ) );
VLC_EXPORT( void,             input_DestroyThread,  ( input_thread_t * ) );

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

    /* bookmarks */
    INPUT_GET_BOOKMARKS,   /* arg1= seekpoint_t *** arg2= int * res=can fail */
    INPUT_CLEAR_BOOKMARKS, /* res=can fail */
    INPUT_ADD_BOOKMARK,    /* arg1= seekpoint_t *  res=can fail   */
    INPUT_CHANGE_BOOKMARK, /* arg1= seekpoint_t * arg2= int * res=can fail   */
    INPUT_DEL_BOOKMARK,    /* arg1= seekpoint_t *  res=can fail   */
    INPUT_SET_BOOKMARK,    /* arg1= int  res=can fail    */

    /* On the fly input slave */
    INPUT_ADD_SLAVE,       /* arg1= char * */
};

VLC_EXPORT( int, input_vaControl,( input_thread_t *, int i_query, va_list  ) );
VLC_EXPORT( int, input_Control,  ( input_thread_t *, int i_query, ...  ) );

VLC_EXPORT( decoder_t *, input_DecoderNew, ( input_thread_t *, es_format_t *, vlc_bool_t b_force_decoder ) );
VLC_EXPORT( void, input_DecoderDelete, ( decoder_t * ) );
VLC_EXPORT( void, input_DecoderDecode,( decoder_t *, block_t * ) );

#endif
