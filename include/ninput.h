/*****************************************************************************
 * ninput.h
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ninput.h,v 1.16 2003/11/19 00:14:39 fenrir Exp $
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

#include "audio_output.h"
#include "vlc_video.h"

enum es_extra_type_e
{
    ES_EXTRA_TYPE_UNKNOWN,
    ES_EXTRA_TYPE_WAVEFORMATEX,
    ES_EXTRA_TYPE_BITMAPINFOHEADER,
    ES_EXTRA_TYPE_SUBHEADER
};

typedef struct subs_format_t
{
    char *psz_encoding;

} subs_format_t;

typedef struct
{
    int             i_cat;
    vlc_fourcc_t    i_codec;

    int             i_group;    /* -1 : standalone
                                   >= 0 then a "group" (program) is created
                                        for each value */
    int             i_priority; /*  -2 : mean not selectable by the users
                                    -1 : mean not selected by default even
                                        when no other stream
                                    >=0: priority */
    char            *psz_language;
    char            *psz_description;

    audio_format_t audio;
    video_format_t video;
    subs_format_t  subs;

    int     i_bitrate;

    int     i_extra_type;
    int     i_extra;
    void    *p_extra;

} es_format_t;

static inline void es_format_Init( es_format_t *fmt,
                                   int i_cat, vlc_fourcc_t i_codec )
{
    fmt->i_cat                  = i_cat;
    fmt->i_codec                = i_codec;
    fmt->i_group                = 0;
    fmt->i_priority             = 0;
    fmt->psz_language           = NULL;
    fmt->psz_description        = NULL;

    memset( &fmt->audio, 0, sizeof(audio_format_t) );
    memset( &fmt->video, 0, sizeof(video_format_t) );
    memset( &fmt->subs, 0, sizeof(subs_format_t) );

    fmt->i_bitrate              = 0;
    fmt->i_extra_type           = ES_EXTRA_TYPE_UNKNOWN;
    fmt->i_extra                = 0;
    fmt->p_extra                = NULL;
}

enum es_out_query_e
{
    ES_OUT_SET_SELECT,  /* arg1= es_out_id_t* arg2=vlc_bool_t   */
    ES_OUT_GET_SELECT   /* arg1= es_out_id_t* arg2=vlc_bool_t*  */
};

struct es_out_t
{
    es_out_id_t *(*pf_add)    ( es_out_t *, es_format_t * );
    int          (*pf_send)   ( es_out_t *, es_out_id_t *, pes_packet_t * );
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
static inline int es_out_Send( es_out_t *out, es_out_id_t *id, pes_packet_t *p_pes )
{
    return out->pf_send( out, id, p_pes );
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
};

/* Stream */
VLC_EXPORT( stream_t *,     stream_OpenInput,       ( input_thread_t * ) );
VLC_EXPORT( void,           stream_Release,         ( stream_t * ) );
VLC_EXPORT( int,            stream_vaControl,       ( stream_t *, int i_query, va_list ) );
VLC_EXPORT( int,            stream_Control,         ( stream_t *, int i_query, ... ) );
VLC_EXPORT( int,            stream_Read,            ( stream_t *, void *p_read, int i_read ) );
VLC_EXPORT( int,            stream_Peek,            ( stream_t *, uint8_t **pp_peek, int i_peek ) );
VLC_EXPORT( data_packet_t *,stream_DataPacket,      ( stream_t *, int i_size, vlc_bool_t b_force ) );
VLC_EXPORT( pes_packet_t *, stream_PesPacket,       ( stream_t *, int i_size ) );

static int64_t inline stream_Tell( stream_t *s )
{
    int64_t i_pos;
    stream_Control( s, STREAM_GET_POSITION, &i_pos );

    return i_pos;
}
static int64_t inline stream_Size( stream_t *s )
{
    int64_t i_pos;
    stream_Control( s, STREAM_GET_SIZE, &i_pos );

    return i_pos;
}
static int inline stream_Seek( stream_t *s, int64_t i_pos )
{
    return stream_Control( s, STREAM_SET_POSITION, i_pos );
}


/**
 * @}
 */

/**
 * \defgroup demux Demux
 * @{
 */
enum demux_query_e
{
    DEMUX_GET_POSITION,         /* arg1= double *       res=    */
    DEMUX_SET_POSITION,         /* arg1= double         res=can fail    */

    DEMUX_GET_TIME,             /* arg1= int64_t *      res=    */
    DEMUX_SET_TIME,             /* arg1= int64_t        res=can fail    */

    DEMUX_GET_LENGTH,           /* arg1= int64_t *      res=can fail    */

    DEMUX_GET_FPS               /* arg1= float *        res=can fail    */
};



/* Demux */
VLC_EXPORT( int,            demux_vaControl,        ( input_thread_t *, int i_query, va_list  ) );
VLC_EXPORT( int,            demux_Control,          ( input_thread_t *, int i_query, ...  ) );

VLC_EXPORT( int,            demux_vaControlDefault, ( input_thread_t *, int i_query, va_list  ) );

/* Subtitles */
VLC_EXPORT( char **,        subtitles_Detect,       ( input_thread_t *, char* path, char *fname ) );

/**
 * @}
 */

#endif

