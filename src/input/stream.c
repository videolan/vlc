/*****************************************************************************
 * stream.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: stream.c,v 1.12 2004/01/21 17:01:54 fenrir Exp $
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

#define MAX_LINE 1024
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
    int        *p_int;

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
        {
            i64 = (int64_t) va_arg( args, int64_t );
            int64_t i_skip;

            vlc_mutex_lock( &s->p_input->stream.stream_lock );
            if( i64 < 0 ||
                ( s->p_input->stream.p_selected_area->i_size > 0 &&
                  s->p_input->stream.p_selected_area->i_size < i64 ) )
            {
                vlc_mutex_unlock( &s->p_input->stream.stream_lock );
                msg_Warn( s, "seek out of bound" );
                return VLC_EGENERIC;
            }

            i_skip = i64 - s->p_input->stream.p_selected_area->i_tell;

            if( i_skip == 0 )
            {
                vlc_mutex_unlock( &s->p_input->stream.stream_lock );
                return VLC_SUCCESS;
            }

            if( i_skip > 0 && i_skip < s->p_input->p_last_data - s->p_input->p_current_data - 1 )
            {
                /* We can skip without reading/seeking */
                s->p_input->p_current_data += i_skip;
                s->p_input->stream.p_selected_area->i_tell = i64;
                vlc_mutex_unlock( &s->p_input->stream.stream_lock );
                return VLC_SUCCESS;
            }
            vlc_mutex_unlock( &s->p_input->stream.stream_lock );

            if( s->p_input->stream.b_seekable &&
                ( s->p_input->stream.i_method == INPUT_METHOD_FILE ||
                  i_skip < 0 || i_skip >= ( s->p_input->i_mtu > 0 ? s->p_input->i_mtu : 4096 ) ) )
            {
                input_AccessReinit( s->p_input );
                s->p_input->pf_seek( s->p_input, i64 );
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
        }

        case STREAM_GET_MTU:
            p_int = (int*) va_arg( args, int * );
            *p_int = s->p_input->i_mtu;
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
 * \return The real numbers of valid bytes, if it's less
 * or equal to 0, *pp_peek is invalid.
 * \note pp_peek is a pointer to internal buffer and it will be invalid as
 * soons as other stream_* functions are called.
 * \note Due to input limitation, it could be less than i_peek without meaning
 * the end of the stream (but only when you have i_peek >=
 * p_input->i_bufsize)
 */
int stream_Peek( stream_t *s, uint8_t **pp_peek, int i_peek )
{
    return input_Peek( s->p_input, pp_peek, i_peek );
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
 * Read from the stream untill first newline.
 * \param s Stream handle to read from
 * \return A null-terminated string. This must be freed,
 */
char *stream_ReadLine( stream_t *s )
{
    uint8_t *p_data;
    char    *p_line;
    int      i_data;
    int      i = 0;
    i_data = stream_Peek( s, &p_data, MAX_LINE );
    msg_Dbg( s->p_input, "i_data %d", i_data );
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
            msg_Err( s->p_input, "Out of memory" );
            return NULL;
        }
        i = stream_Read( s, p_line, i + 1 );
        p_line[ i - 1 ] = '\0';
        msg_Dbg( s->p_input, "found %d chars long line", i );
        return p_line;
    }
}

