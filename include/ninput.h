/*****************************************************************************
 * ninput.h
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ninput.h,v 1.2 2003/08/02 16:43:59 fenrir Exp $
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

/*
 * Stream
 *
 */
enum streamQuery_e
{
    /* capabilities */
    STREAM_CAN_SEEK,            /* arg1= vlc_bool_t *   res=cannot fail*/
    STREAM_CAN_FASTSEEK,        /* arg1= vlc_bool_t *   res=cannot fail*/

    /* */
    STREAM_SET_POSITION,        /* arg1= int64_t        res=can fail  */
    STREAM_GET_POSITION,        /* arg1= int64_t *      res=cannot fail*/

    STREAM_GET_SIZE,            /* arg1= int64_t *      res=cannot fail (0 if no sense)*/
};

/*
 * Demux
 *
 */
#define DEMUX_POSITION_MAX  10000
enum demuxQuery_e
{
    DEMUX_GET_POSITION,         /* arg1= int64_t *      res=    */
    DEMUX_SET_POSITION,         /* arg1= int64_t        res=can fail    */

    DEMUX_GET_TIME,             /* arg1= int64_t *      res=    */
    DEMUX_SET_TIME,             /* arg1= int64_t        res=can fail    */

    DEMUX_GET_LENGTH            /* arg1= int64_t *      res=can fail    */
};


VLC_EXPORT( stream_t *,     stream_OpenInput,       ( input_thread_t * ) );
VLC_EXPORT( void,           stream_Release,         ( stream_t * ) );
VLC_EXPORT( int,            stream_vaControl,       ( stream_t *, int, va_list ) );
VLC_EXPORT( int,            stream_Control,         ( stream_t *, int, ... ) );
VLC_EXPORT( int,            stream_Read,            ( stream_t *, void *, int ) );
VLC_EXPORT( int,            stream_Peek,            ( stream_t *, uint8_t **, int ) );

VLC_EXPORT( pes_packet_t *, stream_PesPacket,       ( stream_t *, int ) );

VLC_EXPORT( int,            demux_vaControl,        ( input_thread_t *, int, va_list  ) );
VLC_EXPORT( int,            demux_Control,          ( input_thread_t *, int, ...  ) );

#endif

