/*****************************************************************************
 * ninput.h
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ninput.h,v 1.6 2003/08/22 20:29:58 fenrir Exp $
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


/* Stream */
VLC_EXPORT( stream_t *,     stream_OpenInput,       ( input_thread_t * ) );
VLC_EXPORT( void,           stream_Release,         ( stream_t * ) );
VLC_EXPORT( int,            stream_vaControl,       ( stream_t *, int i_query, va_list ) );
VLC_EXPORT( int,            stream_Control,         ( stream_t *, int i_query, ... ) );
VLC_EXPORT( int,            stream_Read,            ( stream_t *, void *p_read, int i_read ) );
VLC_EXPORT( int,            stream_Peek,            ( stream_t *, uint8_t **pp_peek, int i_peek ) );
VLC_EXPORT( data_packet_t *,stream_DataPacket,      ( stream_t *, int i_size, vlc_bool_t b_force ) );
VLC_EXPORT( pes_packet_t *, stream_PesPacket,       ( stream_t *, int i_size ) );
/**
 * @}
 */

/**
 * \defgroup demux Demux
 * XXX: don't look at it yet.
 * @{
 */
#define DEMUX_POSITION_MAX  10000
enum demux_query_e
{
    DEMUX_GET_POSITION,         /* arg1= int64_t *      res=    */
    DEMUX_SET_POSITION,         /* arg1= int64_t        res=can fail    */

    DEMUX_GET_TIME,             /* arg1= int64_t *      res=    */
    DEMUX_SET_TIME,             /* arg1= int64_t        res=can fail    */

    DEMUX_GET_LENGTH            /* arg1= int64_t *      res=can fail    */
};



/* Demux */
VLC_EXPORT( int,            demux_vaControl,        ( input_thread_t *, int i_query, va_list  ) );
VLC_EXPORT( int,            demux_Control,          ( input_thread_t *, int i_query, ...  ) );

/**
 * @}
 */
#endif

