/*****************************************************************************
 * ninput.h
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ninput.h,v 1.3 2003/08/02 19:16:04 fenrir Exp $
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
 * Stream (stream_t)
 * -----------------
 *  This will allow you to easily handle read/seek in demuxer modules.
 *
 * - stream_OpenInput
 *      create a "stream_t *" from an "input_thread_t *".
 * - stream_Release
 *      destroy a previously "stream_t *" instances.
 * - stream_Read
 *      Try to read "i_read" bytes into a buffer pointed by "p_read".
 *      If "p_read" is NULL then data are skipped instead of read.
 *      The return value is the real numbers of bytes read/skip. If
 *      this value is less than i_read that means that it's the end
 *      of the stream.
 * - stream_Peek
 *      Store in pp_peek a pointer to the next "i_peek" bytes in the
 *      stream
 *      The return value is the real numbers of valid bytes, if it's
 *      less or equal to 0, *pp_peek is invalid.
 *      XXX: it's a pointer to internal buffer and it will be invalid
 *      as soons as other stream_* functions are called.
 *      be 0 (then *pp_peek isn't valid).
 *      XXX: due to input limitation, it could be less than i_peek without
 *      meaning the end of the stream (but only when you have
 *      i_peek >= p_input->i_bufsize)
 * - stream_PesPacket
 *      Read "i_size" bytes and store them in a pes_packet_t.
 *      Only fields p_first, p_last, i_nb_data, and i_pes_size are set.
 *      (Of course, you need to fill i_dts, i_pts, ... )
 *      If only less than "i_size" bytes are available NULL is returned.
 * - stream_vaControl, stream_Control
 *      Use to control the "stream_t *". Look at stream_query_e for possible
 *      "i_query" value and format arguments.
 *      Return VLC_SUCCESS if ... succeed ;) and VLC_EGENERIC if failed or
 *      unimplemented
 */

enum stream_query_e
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
 * XXX: don't look at it yet.
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


/* Stream */
VLC_EXPORT( stream_t *,     stream_OpenInput,       ( input_thread_t * ) );
VLC_EXPORT( void,           stream_Release,         ( stream_t * ) );
VLC_EXPORT( int,            stream_vaControl,       ( stream_t *, int i_query, va_list ) );
VLC_EXPORT( int,            stream_Control,         ( stream_t *, int i_query, ... ) );
VLC_EXPORT( int,            stream_Read,            ( stream_t *, void *p_read, int i_read ) );
VLC_EXPORT( int,            stream_Peek,            ( stream_t *, uint8_t **pp_peek, int i_peek ) );
VLC_EXPORT( pes_packet_t *, stream_PesPacket,       ( stream_t *, int i_size ) );

/* Demux */
VLC_EXPORT( int,            demux_vaControl,        ( input_thread_t *, int i_query, va_list  ) );
VLC_EXPORT( int,            demux_Control,          ( input_thread_t *, int i_query, ...  ) );

#endif

