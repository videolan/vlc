/*****************************************************************************
 * stream.c
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: stream.c,v 1.8 2004/01/03 00:23:04 gbazin Exp $
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

/**
 * Handle to a stream.
 */
struct stream_t
{
    VLC_COMMON_MEMBERS

    /** pointer to the input thread */
    input_thread_t *p_input;

};

/**
 * Create a "stream_t *" from an "input_thread_t *".
 */
stream_t *stream_OpenInput( input_thread_t *p_input )
{
    stream_t *s;

    s = vlc_object_create( p_input, sizeof( stream_t ) );
    if( s )
    {
        s->p_input = p_input;
    }

    return s;
}

/**
 * Destroy a previously created "stream_t *" instance.
 */
void stream_Release( stream_t *s )
{
    vlc_object_destroy( s );
}

/**
 * Similar to #stream_Control(), but takes a va_list and not variable
 * arguments.
 */
int stream_vaControl( stream_t *s, int i_query, va_list args )
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
            *p_b = s->p_input->stream.b_seekable &&
                   s->p_input->stream.i_method == INPUT_METHOD_FILE;
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
                msg_Warn( s, "seek out of bound" );
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
                  i64 - s->p_input->stream.p_selected_area->i_tell > 4096 ) )
            {
                input_AccessReinit( s->p_input );
                s->p_input->pf_seek( s->p_input, i64 );
                return VLC_SUCCESS;
            }

            if( i64 - s->p_input->stream.p_selected_area->i_tell > 0 )
            {
                data_packet_t *p_data;
                int i_skip = i64 - s->p_input->stream.p_selected_area->i_tell;

                if( i_skip > 1000 )
                {
                    msg_Warn( s, "will skip %d bytes, slow", i_skip );
                }

                while (i_skip > 0 )
                {
                    int i_read;

                    i_read = input_SplitBuffer( s->p_input, &p_data,
                                 __MIN( (int)s->p_input->i_bufsize, i_skip ) );
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

/**
 * Use to control the "stream_t *". Look at #stream_query_e for
 * possible "i_query" value and format arguments.  Return VLC_SUCCESS
 * if ... succeed ;) and VLC_EGENERIC if failed or unimplemented
 */
int stream_Control( stream_t *s, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = stream_vaControl( s, i_query, args );
    va_end( args );

    return i_result;
}

/**
 * Try to read "i_read" bytes into a buffer pointed by "p_read".  If
 * "p_read" is NULL then data are skipped instead of read.  The return
 * value is the real numbers of bytes read/skip. If this value is less
 * than i_read that means that it's the end of the stream.
 */
int stream_Read( stream_t *s, void *p_data, int i_data )
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

        i_count = input_SplitBuffer( s->p_input, &p_packet,
                      __MIN( i_data, (int)s->p_input->i_bufsize ) );

        if( i_count <= 0 )
        {
            if( i_count == 0 )
                input_DeletePacket( s->p_input->p_method_data, p_packet );

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

/**
 * Store in pp_peek a pointer to the next "i_peek" bytes in the stream
 * The return value is the real numbers of valid bytes, if it's less
 * or equal to 0, *pp_peek is invalid.  XXX: it's a pointer to
 * internal buffer and it will be invalid as soons as other stream_*
 * functions are called.  be 0 (then *pp_peek isn't valid).  XXX: due
 * to input limitation, it could be less than i_peek without meaning
 * the end of the stream (but only when you have i_peek >=
 * p_input->i_bufsize)
 */
int stream_Peek( stream_t *s, uint8_t **pp_peek, int i_data )
{
    return input_Peek( s->p_input, pp_peek, i_data );
}

/**
 * Read "i_size" bytes and store them in a block_t. If less than "i_size"
 * bytes are available then return what is left and if nothing is availble,
 * return NULL.
 */
block_t *stream_Block( stream_t *s, int i_size )
{
    block_t *p_block;

    if( i_size <= 0 ) return NULL;
    if( !(p_block = block_New( s->p_input, i_size ) ) ) return NULL;

    p_block->i_buffer = stream_Read( s, p_block->p_buffer, i_size );
    if( !p_block->i_buffer )
    {
        block_Release( p_block );
        p_block = NULL;
    }

    return p_block;
}

/**
 * Read "i_size" bytes and store them in a pes_packet_t.  Only fields
 * p_first, p_last, i_nb_data, and i_pes_size are set.  (Of course,
 * you need to fill i_dts, i_pts, ... ) If only less than "i_size"
 * bytes are available NULL is returned.
 */
pes_packet_t *stream_PesPacket( stream_t *s, int i_data )
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
            p_pes->p_last = input_NewPacket( s->p_input->p_method_data, 0 );
        p_pes->i_nb_data = 1;
        return p_pes;
    }

    while( i_data > 0 )
    {
        int i_read;

        i_read = input_SplitBuffer( s->p_input, &p_packet,
                     __MIN( i_data, (int)s->p_input->i_bufsize ) );
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
        i_data -= i_read;
    }

    return p_pes;
}

/**
 * Read i_size into a data_packet_t. If b_force is not set, fewer bytes can
 * be returned. You should always set b_force, unless you know what you are
 * doing.
 */
data_packet_t *stream_DataPacket( stream_t *s, int i_size, vlc_bool_t b_force )
{
    data_packet_t *p_pk;
    int           i_read;

    if( i_size <= 0 )
    {
        p_pk = input_NewPacket( s->p_input->p_method_data, 0 );
        if( p_pk )
        {
            p_pk->p_payload_end = p_pk->p_payload_start;
        }
        return p_pk;
    }

    i_read = input_SplitBuffer( s->p_input, &p_pk, i_size );
    if( i_read <= 0 )
    {
        return NULL;
    }

    /* Should be really rare, near 0 */
    if( i_read < i_size && b_force )
    {
        data_packet_t *p_old = p_pk;
        int           i_missing = i_size - i_read;

        p_pk = input_NewPacket( s->p_input->p_method_data, i_size );
        if( p_pk == NULL )
        {
            input_DeletePacket( s->p_input->p_method_data, p_old );
            return NULL;
        }
        p_pk->p_payload_end = p_pk->p_payload_start + i_size;
        memcpy( p_pk->p_payload_start, p_old->p_payload_start, i_read );
        input_DeletePacket( s->p_input->p_method_data, p_old );

        if( stream_Read( s, &p_pk->p_payload_start[i_read], i_missing )
            < i_missing )
        {
            input_DeletePacket( s->p_input->p_method_data, p_pk );
            return NULL;
        }
    }

    return p_pk;
}
