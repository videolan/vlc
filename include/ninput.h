/*****************************************************************************
 * ninput.h
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _NINPUT_H
#define _NINPUT_H 1

#include "vlc_es.h"

enum es_out_mode_e
{
    ES_OUT_MODE_NONE,   /* don't select anything */
    ES_OUT_MODE_ALL,    /* eg for stream output */
    ES_OUT_MODE_AUTO    /* best audio/video or for input follow audio-channel, spu-channel */
};

enum es_out_query_e
{
    /* activate apply of mode */
    ES_OUT_SET_ACTIVE,  /* arg1= vlc_bool_t                     */
    /* see if mode is currently aplied or not */
    ES_OUT_GET_ACTIVE,  /* arg1= vlc_bool_t*                    */

    /* set/get mode */
    ES_OUT_SET_MODE,    /* arg1= int                            */
    ES_OUT_GET_MODE,    /* arg2= int*                           */

    /* set es selected for the es category(audio/video/spu) */
    ES_OUT_SET_ES,      /* arg1= es_out_id_t*                   */

    /* force selection/unselection of the ES (bypass current mode)*/
    ES_OUT_SET_ES_STATE,/* arg1= es_out_id_t* arg2=vlc_bool_t   */
    ES_OUT_GET_ES_STATE,/* arg1= es_out_id_t* arg2=vlc_bool_t*  */

    /* XXX XXX XXX Don't use them YET !!! */
    ES_OUT_SET_PCR,             /* arg1=int64_t i_pcr(microsecond!) (using default group 0)*/
    ES_OUT_SET_GROUP_PCR,       /* arg1= int i_group, arg2=int64_t i_pcr(microsecond!)*/
    ES_OUT_RESET_PCR    /* no arg */
};

struct es_out_t
{
    es_out_id_t *(*pf_add)    ( es_out_t *, es_format_t * );
    int          (*pf_send)   ( es_out_t *, es_out_id_t *, block_t * );
    void         (*pf_del)    ( es_out_t *, es_out_id_t * );
    int          (*pf_control)( es_out_t *, int i_query, va_list );

    es_out_sys_t    *p_sys;
};

static inline es_out_id_t * es_out_Add( es_out_t *out, es_format_t *fmt )
{
    return out->pf_add( out, fmt );
}
static inline void es_out_Del( es_out_t *out, es_out_id_t *id )
{
    out->pf_del( out, id );
}
static inline int es_out_Send( es_out_t *out, es_out_id_t *id,
                               block_t *p_block )
{
    return out->pf_send( out, id, p_block );
}

static inline int es_out_vaControl( es_out_t *out, int i_query, va_list args )
{
    return out->pf_control( out, i_query, args );
}
static inline int es_out_Control( es_out_t *out, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = es_out_vaControl( out, i_query, args );
    va_end( args );
    return i_result;
}

/**
 * \defgroup stream Stream
 *
 *  This will allow you to easily handle read/seek in demuxer modules.
 * @{
 */

/**
 * Possible commands to send to stream_Control() and stream_vaControl()
 */
enum stream_query_e
{
    /* capabilities */
    STREAM_CAN_SEEK,            /**< arg1= vlc_bool_t *   res=cannot fail*/
    STREAM_CAN_FASTSEEK,        /**< arg1= vlc_bool_t *   res=cannot fail*/

    /* */
    STREAM_SET_POSITION,        /**< arg1= int64_t        res=can fail  */
    STREAM_GET_POSITION,        /**< arg1= int64_t *      res=cannot fail*/

    STREAM_GET_SIZE,            /**< arg1= int64_t *      res=cannot fail (0 if no sense)*/

    STREAM_GET_MTU              /**< arg1= int *          res=cannot fail (0 if no sense)*/
};

/**
 * stream_t definition
 */
struct stream_t
{
    VLC_COMMON_MEMBERS

    block_t *(*pf_block)  ( stream_t *, int i_size );
    int      (*pf_read)   ( stream_t *, void *p_read, int i_read );
    int      (*pf_peek)   ( stream_t *, uint8_t **pp_peek, int i_peek );
    int      (*pf_control)( stream_t *, int i_query, va_list );

    stream_sys_t *p_sys;
};

/**
 * Try to read "i_read" bytes into a buffer pointed by "p_read".  If
 * "p_read" is NULL then data are skipped instead of read.  The return
 * value is the real numbers of bytes read/skip. If this value is less
 * than i_read that means that it's the end of the stream.
 */
static inline int stream_Read( stream_t *s, void *p_read, int i_read )
{
    return s->pf_read( s, p_read, i_read );
}

/**
 * Store in pp_peek a pointer to the next "i_peek" bytes in the stream
 * \return The real numbers of valid bytes, if it's less
 * or equal to 0, *pp_peek is invalid.
 * \note pp_peek is a pointer to internal buffer and it will be invalid as
 * soons as other stream_* functions are called.
 * \note Due to input limitation, it could be less than i_peek without meaning
 * the end of the stream (but only when you have i_peek >=
 * p_input->i_bufsize)
 */
static inline int stream_Peek( stream_t *s, uint8_t **pp_peek, int i_peek )
{
    return s->pf_peek( s, pp_peek, i_peek );
}

/**
 * Use to control the "stream_t *". Look at #stream_query_e for
 * possible "i_query" value and format arguments.  Return VLC_SUCCESS
 * if ... succeed ;) and VLC_EGENERIC if failed or unimplemented
 */
static inline int stream_vaControl( stream_t *s, int i_query, va_list args )
{
    return s->pf_control( s, i_query, args );
}
static inline int stream_Control( stream_t *s, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = s->pf_control( s, i_query, args );
    va_end( args );
    return i_result;
}
static inline int64_t stream_Tell( stream_t *s )
{
    int64_t i_pos;
    stream_Control( s, STREAM_GET_POSITION, &i_pos );
    return i_pos;
}
static inline int64_t stream_Size( stream_t *s )
{
    int64_t i_pos;
    stream_Control( s, STREAM_GET_SIZE, &i_pos );
    return i_pos;
}
static inline int stream_MTU( stream_t *s )
{
    int i_mtu;
    stream_Control( s, STREAM_GET_MTU, &i_mtu );
    return i_mtu;
}
static inline int stream_Seek( stream_t *s, int64_t i_pos )
{
    return stream_Control( s, STREAM_SET_POSITION, i_pos );
}

/**
 * Read "i_size" bytes and store them in a block_t. If less than "i_size"
 * bytes are available then return what is left and if nothing is availble,
 * return NULL.
 */
static inline block_t *stream_Block( stream_t *s, int i_size )
{
    if( i_size <= 0 ) return NULL;

    if( s->pf_block )
    {
        return s->pf_block( s, i_size );
    }
    else
    {
        /* emulate block read */
        block_t *p_bk = block_New( s, i_size );
        if( p_bk )
        {
            p_bk->i_buffer = stream_Read( s, p_bk->p_buffer, i_size );
            if( p_bk->i_buffer > 0 )
            {
                return p_bk;
            }
        }
        if( p_bk ) block_Release( p_bk );
        return NULL;
    }
}

VLC_EXPORT( char *, stream_ReadLine, ( stream_t * ) );

/**
 * @}
 */

/**
 * \defgroup demux Demux
 * @{
 */

struct demux_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t    *p_module;

    /* eg informative but needed (we can have access+demux) */
    char        *psz_access;
    char        *psz_demux;
    char        *psz_path;

    /* input stream */
    stream_t    *s;     /* NULL in case of a access+demux in one */

    /* es output */
    es_out_t    *out;   /* ou p_es_out */

    /* set by demuxer */
    int (*pf_demux)  ( demux_t * );   /* demux one frame only */
    int (*pf_control)( demux_t *, int i_query, va_list args);
    demux_sys_t *p_sys;
};

enum demux_query_e
{
    DEMUX_GET_POSITION,         /* arg1= double *       res=    */
    DEMUX_SET_POSITION,         /* arg1= double         res=can fail    */

    DEMUX_GET_TIME,             /* arg1= int64_t *      res=    */
    DEMUX_SET_TIME,             /* arg1= int64_t        res=can fail    */

    DEMUX_GET_LENGTH,           /* arg1= int64_t *      res=can fail    */

    DEMUX_GET_FPS,              /* arg1= float *        res=can fail    */
    DEMUX_GET_META              /* arg1= vlc_meta_t **  res=can fail    */
};

struct seekpoint_t
{
    int64_t i_byte_offset;
    int64_t i_time_offset;
    char    *psz_name;
};

static inline seekpoint_t *vlc_seekpoint_New( void )
{
    seekpoint_t *point = (seekpoint_t*)malloc( sizeof( seekpoint_t ) );
    point->i_byte_offset = point->i_time_offset;
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

/* Demux */
VLC_EXPORT( int, demux_vaControl,        ( input_thread_t *, int i_query, va_list  ) );
VLC_EXPORT( int, demux_Control,          ( input_thread_t *, int i_query, ...  ) );

VLC_EXPORT( int, demux_vaControlDefault, ( input_thread_t *, int i_query, va_list  ) );


/* New demux arch: don't touch that */
/* stream_t *s could be null and then it mean a access+demux in one */
#define demux2_New( a, b, c, d ) __demux2_New(VLC_OBJECT(a), b, c, d)
VLC_EXPORT( demux_t *, __demux2_New,  ( vlc_object_t *p_obj, char *psz_mrl, stream_t *s, es_out_t *out ) );
VLC_EXPORT( void,      demux2_Delete, ( demux_t * ) );
VLC_EXPORT( int,       demux2_vaControlHelper, ( stream_t *, int64_t i_start, int64_t i_end, int i_bitrate, int i_align, int i_query, va_list args ) );

static inline int demux2_Demux( demux_t *p_demux )
{
    return p_demux->pf_demux( p_demux );
}
static inline int demux2_vaControl( demux_t *p_demux, int i_query, va_list args )
{
    return p_demux->pf_control( p_demux, i_query, args );
}
static inline int demux2_Control( demux_t *p_demux, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = demux2_vaControl( p_demux, i_query, args );
    va_end( args );
    return i_result;
}

/* Subtitles */
VLC_EXPORT( char **, subtitles_Detect, ( input_thread_t *, char* path, char *fname ) );

/**
 * @}
 */


/**
 * \defgroup input Input
 * @{
 */
enum input_query_e
{
    INPUT_GET_POSITION,         /* arg1= double *       res=    */
    INPUT_SET_POSITION,         /* arg1= double         res=can fail    */

    INPUT_GET_TIME,             /* arg1= int64_t *      res=    */
    INPUT_SET_TIME,             /* arg1= int64_t        res=can fail    */

    INPUT_GET_LENGTH,           /* arg1= int64_t *      res=can fail    */

    INPUT_GET_FPS,              /* arg1= float *        res=can fail    */
    INPUT_GET_META,             /* arg1= vlc_meta_t **  res=can fail    */

    INPUT_GET_BOOKMARKS,   /* arg1= seekpoint_t *** arg2= int * res=can fail */
    INPUT_CLEAR_BOOKMARKS, /* res=can fail */
    INPUT_ADD_BOOKMARK,    /* arg1= seekpoint_t *  res=can fail   */
    INPUT_DEL_BOOKMARK,    /* arg1= seekpoint_t *  res=can fail   */
    INPUT_SET_BOOKMARK,    /* arg1= int  res=can fail    */

    INPUT_GET_DIVISIONS
};

VLC_EXPORT( int, input_vaControl,( input_thread_t *, int i_query, va_list  ) );
VLC_EXPORT( int, input_Control,  ( input_thread_t *, int i_query, ...  ) );

/**
 * @}
 */

#endif
