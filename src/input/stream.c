/*****************************************************************************
 * stream.c
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: stream.c,v 1.1 2003/08/01 00:00:12 fenrir Exp $
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
 * stream_* :
 * XXX for now it's just a wrapper
 *
 ****************************************************************************/

struct stream_t
{
    VLC_COMMON_MEMBERS

    input_thread_t *p_input;

};

stream_t    *stream_OpenInput( input_thread_t *p_input )
{
    stream_t *s;

    s = vlc_object_create( p_input, sizeof( stream_t ) );
    if( s )
    {
        s->p_input = p_input;
    }

    return s;
}

void        stream_Release( stream_t *s )
{
    vlc_object_destroy( s );
}

int         stream_vaControl( stream_t *s, int i_query, va_list args )
{
    vlc_bool_t *p_b;
    int64_t    *p_i64, i64;

    switch( i_query )
    {
        case STREAM_GET_SIZE:
            p_i64 = (int64_t*) va_arg( args, int64_t * );

            vlc_mutex_lock( &s->p_input->stream.stream_lock );
            *p_i64 = s->p_input->stream.p_selected_area->i_size;
            vlc_mutex_unlock( &s->p_input->stream.stream_lock );
            return VLC_SUCCESS;

        case STREAM_CAN_SEEK:
            p_b = (vlc_bool_t*) va_arg( args, vlc_bool_t * );

            vlc_mutex_lock( &s->p_input->stream.stream_lock );
            *p_b = s->p_input->stream.b_seekable;
            vlc_mutex_unlock( &s->p_input->stream.stream_lock );
            return VLC_SUCCESS;

        case STREAM_CAN_FASTSEEK:
            p_b = (vlc_bool_t*) va_arg( args, vlc_bool_t * );

            vlc_mutex_lock( &s->p_input->stream.stream_lock );
            *p_b = s->p_input->stream.b_seekable && s->p_input->stream.i_method == INPUT_METHOD_FILE;
            vlc_mutex_unlock( &s->p_input->stream.stream_lock );
            return VLC_SUCCESS;

        case STREAM_GET_POSITION:
            p_i64 = (int64_t*) va_arg( args, int64_t * );

            vlc_mutex_lock( &s->p_input->stream.stream_lock );
            *p_i64 = s->p_input->stream.p_selected_area->i_tell;
            vlc_mutex_unlock( &s->p_input->stream.stream_lock );
            return VLC_SUCCESS;

        case STREAM_SET_POSITION:
            i64 = (int64_t) va_arg( args, int64_t );

            vlc_mutex_lock( &s->p_input->stream.stream_lock );
            if( i64 < 0 ||
                ( s->p_input->stream.p_selected_area->i_size > 0 &&
                  s->p_input->stream.p_selected_area->i_size < i64 ) )
            {
                vlc_mutex_unlock( &s->p_input->stream.stream_lock );
                msg_Err( s, "seek out of bound" );
                return VLC_EGENERIC;
            }
            vlc_mutex_unlock( &s->p_input->stream.stream_lock );

            if( i64 == s->p_input->stream.p_selected_area->i_tell )
            {
                return VLC_SUCCESS;
            }

            if( s->p_input->stream.b_seekable &&
                ( s->p_input->stream.i_method == INPUT_METHOD_FILE ||
                  i64 - s->p_input->stream.p_selected_area->i_tell < 0 ||
                  i64 - s->p_input->stream.p_selected_area->i_tell > 1024 ) )
            {
                input_AccessReinit( s->p_input );
                s->p_input->pf_seek( s->p_input, i64 );
                return VLC_SUCCESS;
            }

            if( i64 - s->p_input->stream.p_selected_area->i_tell > 0 )
            {
                data_packet_t   *p_data;
                int             i_skip = i64 - s->p_input->stream.p_selected_area->i_tell;

                msg_Warn( s, "will skip %d bytes, slow", i_skip );

                while (i_skip > 0 )
                {
                    int i_read;

                    i_read = input_SplitBuffer( s->p_input, &p_data, __MIN( 4096, i_skip ) );
                    if( i_read < 0 )
                    {
                        return VLC_EGENERIC;
                    }
                    i_skip -= i_read;

                    input_DeletePacket( s->p_input->p_method_data, p_data );
                    if( i_read == 0 && i_skip > 0 )
                    {
                        return VLC_EGENERIC;
                    }
                }
            }

            return VLC_SUCCESS;

        default:
            msg_Err( s, "invalid stream_vaControl query=0x%x", i_query );
            return VLC_EGENERIC;
    }
}

int         stream_Control( stream_t *s, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = stream_vaControl( s, i_query, args );
    va_end( args );

    return i_result;
}

int         stream_Read( stream_t *s, void *p_data, int i_data )
{
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

    while( i_data > 0 && !s->p_input->b_die )
    {
        int i_count;

        i_count = input_SplitBuffer( s->p_input, &p_packet, __MIN( i_data, 4096 ) );
        if( i_count <= 0 )
        {
            return i_read;
        }

        if( p )
        {
            memcpy( p, p_packet->p_payload_start, i_count );
            p += i_count;
        }

        input_DeletePacket( s->p_input->p_method_data, p_packet );

        i_data -= i_count;
        i_read += i_count;
    }

    return i_read;
}

int         stream_Peek( stream_t *s, uint8_t **pp_peek, int i_data )
{
    return input_Peek( s->p_input, pp_peek, i_data );
}


pes_packet_t    *stream_PesPacket( stream_t *s, int i_data )
{
    pes_packet_t  *p_pes;
    data_packet_t *p_packet;


    if( !(p_pes = input_NewPES( s->p_input->p_method_data ) ) )
    {
        return NULL;
    }

    if( i_data <= 0 )
    {
        p_pes->p_first =
            p_pes->p_last  =
                input_NewPacket( s->p_input->p_method_data, 0 );
        p_pes->i_nb_data = 1;
        return p_pes;
    }

    while( i_data > 0 )
    {
        int i_read;

        i_read = input_SplitBuffer( s->p_input, &p_packet, __MIN( i_data, 4096 ) );
        if( i_read <= 0 )
        {
            /* should occur only with EOF and max allocation reached 
             * it safer to  return an error */
            /* free pes */
            input_DeletePES( s->p_input->p_method_data, p_pes );
            return NULL;
        }

        if( p_pes->p_first == NULL )
        {
            p_pes->p_first = p_packet;
        }
        else
        {
            p_pes->p_last->p_next = p_packet;
        }
        p_pes->p_last = p_packet;
        p_pes->i_nb_data++;
        p_pes->i_pes_size += i_read;
        i_data            -= i_read;
    }

    return p_pes;
}





