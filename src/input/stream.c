/*****************************************************************************
 * stream.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: stream.c,v 1.14 2004/01/26 20:48:10 fenrir Exp $
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

#include <stdlib.h>
#include <vlc/vlc.h>
#include <vlc/input.h>

#include "ninput.h"

/****************************************************************************
 * stream_ReadLine:
 ****************************************************************************/
/**
 * Read from the stream untill first newline.
 * \param s Stream handle to read from
 * \return A null-terminated string. This must be freed,
 */
/* FIXME don't use stupid MAX_LINE -> do the same than net_ReadLine */
#define MAX_LINE 1024
char *stream_ReadLine( stream_t *s )
{
    uint8_t *p_data;
    char    *p_line;
    int      i_data;
    int      i = 0;
    i_data = stream_Peek( s, &p_data, MAX_LINE );
    msg_Dbg( s, "i_data %d", i_data );
    while( i < i_data && p_data[i] != '\n' )
    {
        i++;
    }
    if( i_data <= 0 )
    {
        return NULL;
    }
    else
    {
        p_line = malloc( i + 1 );
        if( p_line == NULL )
        {
            msg_Err( s, "out of memory" );
            return NULL;
        }
        i = stream_Read( s, p_line, i + 1 );
        p_line[ i - 1 ] = '\0';
        msg_Dbg( s, "found %d chars long line", i );
        return p_line;
    }
}



/* TODO: one day we should create a special module stream
 * when we would have a access wrapper, and stream filter
 * (like caching, progessive, gunzip, ... )
 */

/* private stream_sys_t for input_Stream* */
struct stream_sys_t
{
    input_thread_t *p_input;
};

/* private pf_* functions declarations */
static int      IStreamRead   ( stream_t *, void *p_read, int i_read );
static int      IStreamPeek   ( stream_t *, uint8_t **pp_peek, int i_peek );
static int      IStreamControl( stream_t *, int i_query, va_list );

/****************************************************************************
 * input_StreamNew: create a wrapper for p_input access
 ****************************************************************************/
stream_t *input_StreamNew( input_thread_t *p_input )
{
    stream_t *s = vlc_object_create( p_input, sizeof( stream_t ) );
    if( s )
    {
        s->pf_block  = NULL;
        s->pf_read   = IStreamRead;
        s->pf_peek   = IStreamPeek;
        s->pf_control= IStreamControl;

        s->p_sys = malloc( sizeof( stream_sys_t ) );
        s->p_sys->p_input = p_input;
    }
    return s;
}

/****************************************************************************
 * input_StreamDelete:
 ****************************************************************************/
void input_StreamDelete( stream_t *s )
{
    free( s->p_sys );
    vlc_object_destroy( s );
}


/****************************************************************************
 * IStreamControl:
 ****************************************************************************/
static int IStreamControl( stream_t *s, int i_query, va_list args )
{
    input_thread_t *p_input = s->p_sys->p_input;

    vlc_bool_t *p_b;
    int64_t    *p_i64, i64;
    int        *p_int;

    switch( i_query )
    {
        case STREAM_GET_SIZE:
            p_i64 = (int64_t*) va_arg( args, int64_t * );

            vlc_mutex_lock( &p_input->stream.stream_lock );
            *p_i64 = p_input->stream.p_selected_area->i_size;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            return VLC_SUCCESS;

        case STREAM_CAN_SEEK:
            p_b = (vlc_bool_t*) va_arg( args, vlc_bool_t * );

            vlc_mutex_lock( &p_input->stream.stream_lock );
            *p_b = p_input->stream.b_seekable;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            return VLC_SUCCESS;

        case STREAM_CAN_FASTSEEK:
            p_b = (vlc_bool_t*) va_arg( args, vlc_bool_t * );

            vlc_mutex_lock( &p_input->stream.stream_lock );
            *p_b = p_input->stream.b_seekable &&
                   p_input->stream.i_method == INPUT_METHOD_FILE;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            return VLC_SUCCESS;

        case STREAM_GET_POSITION:
            p_i64 = (int64_t*) va_arg( args, int64_t * );

            vlc_mutex_lock( &p_input->stream.stream_lock );
            *p_i64 = p_input->stream.p_selected_area->i_tell;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            return VLC_SUCCESS;

        case STREAM_SET_POSITION:
        {
            i64 = (int64_t) va_arg( args, int64_t );
            int64_t i_skip;

            vlc_mutex_lock( &p_input->stream.stream_lock );
            if( i64 < 0 ||
                ( p_input->stream.p_selected_area->i_size > 0 &&
                  p_input->stream.p_selected_area->i_size < i64 ) )
            {
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                msg_Warn( s, "seek out of bound" );
                return VLC_EGENERIC;
            }

            i_skip = i64 - p_input->stream.p_selected_area->i_tell;

            if( i_skip == 0 )
            {
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                return VLC_SUCCESS;
            }

            if( i_skip > 0 && i_skip < p_input->p_last_data -
                            p_input->p_current_data - 1 )
            {
                /* We can skip without reading/seeking */
                p_input->p_current_data += i_skip;
                p_input->stream.p_selected_area->i_tell = i64;
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                return VLC_SUCCESS;
            }
            vlc_mutex_unlock( &p_input->stream.stream_lock );

            if( p_input->stream.b_seekable &&
                ( p_input->stream.i_method == INPUT_METHOD_FILE ||
                  i_skip < 0 || i_skip >= ( p_input->i_mtu > 0 ?
                                            p_input->i_mtu : 4096 ) ) )
            {
                input_AccessReinit( p_input );
                p_input->pf_seek( p_input, i64 );
                return VLC_SUCCESS;
            }

            if( i_skip > 0 )
            {
                data_packet_t *p_data;

                if( i_skip > 1000 )
                {
                    msg_Warn( s, "will skip "I64Fd" bytes, slow", i_skip );
                }

                while( i_skip > 0 )
                {
                    int i_read;

                    i_read = input_SplitBuffer( p_input, &p_data,
                                 __MIN( (int)p_input->i_bufsize, i_skip ) );
                    if( i_read < 0 )
                    {
                        return VLC_EGENERIC;
                    }
                    i_skip -= i_read;

                    input_DeletePacket( p_input->p_method_data, p_data );
                    if( i_read == 0 && i_skip > 0 )
                    {
                        return VLC_EGENERIC;
                    }
                }
            }
            return VLC_SUCCESS;
        }

        case STREAM_GET_MTU:
            p_int = (int*) va_arg( args, int * );
            *p_int = p_input->i_mtu;
            return VLC_SUCCESS;

        default:
            msg_Err( s, "invalid stream_vaControl query=0x%x", i_query );
            return VLC_EGENERIC;
    }
}

/****************************************************************************
 * IStreamRead:
 ****************************************************************************/
static int IStreamRead( stream_t *s, void *p_data, int i_data )
{
    input_thread_t *p_input = s->p_sys->p_input;
    uint8_t       *p = (uint8_t*)p_data;
    data_packet_t *p_packet;

    int i_read = 0;

    if( p_data == NULL && i_data > 0 )
    {
        int64_t i_pos;

        stream_Control( s, STREAM_GET_POSITION, &i_pos );

        i_pos += i_data;
        if( stream_Control( s, STREAM_SET_POSITION, i_pos ) )
        {
            return 0;
        }
        return i_data;
    }

    while( i_data > 0 && !p_input->b_die )
    {
        int i_count;

        i_count = input_SplitBuffer( p_input, &p_packet,
                      __MIN( i_data, (int)p_input->i_bufsize ) );

        if( i_count <= 0 )
        {
            if( i_count == 0 )
                input_DeletePacket( p_input->p_method_data, p_packet );

            return i_read;
        }

        if( p )
        {
            memcpy( p, p_packet->p_payload_start, i_count );
            p += i_count;
        }

        input_DeletePacket( p_input->p_method_data, p_packet );

        i_data -= i_count;
        i_read += i_count;
    }

    return i_read;
}
/****************************************************************************
 * IStreamPeek:
 ****************************************************************************/
static int IStreamPeek( stream_t *s, uint8_t **pp_peek, int i_peek )
{
    return input_Peek( s->p_sys->p_input, pp_peek, i_peek );
}
