/*****************************************************************************
 * stream.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
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

    while( i < i_data && p_data[i] != '\n' &&  p_data[i] != '\r' )
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

        return p_line;
    }
}



/* TODO: one day we should create a special module stream
 * when we would have a access wrapper, and stream filter
 * (like caching, progessive, gunzip, ... )
 */

/* private stream_sys_t for input_Stream* */
typedef struct
{
    input_thread_t *p_input;
} input_stream_sys_t;

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
    input_stream_sys_t *p_sys;
    if( s )
    {
        s->pf_block  = NULL;
        s->pf_read   = IStreamRead;
        s->pf_peek   = IStreamPeek;
        s->pf_control= IStreamControl;

        s->p_sys = malloc( sizeof( input_stream_sys_t ) );
        p_sys = (input_stream_sys_t*)s->p_sys;
        p_sys->p_input = p_input;
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
    input_stream_sys_t * p_sys = (input_stream_sys_t*)s->p_sys;
    input_thread_t *p_input = p_sys->p_input;

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
            int64_t i_skip;
            i64 = (int64_t) va_arg( args, int64_t );

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

        case STREAM_CONTROL_ACCESS:
        {
            int i_int = (int) va_arg( args, int );
            if( i_int != ACCESS_SET_PRIVATE_ID_STATE )
            {
                msg_Err( s, "Hey, what are you thinking ?"
                            "DON'T USE STREAM_CONTROL_ACCESS !!!" );
                return VLC_EGENERIC;
            }
            if( p_input->pf_access_control )
            {
                return p_input->pf_access_control( p_input, i_int, args );
            }
            return VLC_EGENERIC;
        }

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
    input_stream_sys_t * p_sys = (input_stream_sys_t*)s->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    uint8_t *p = (uint8_t*)p_data;

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
        ssize_t i_count = p_input->p_last_data - p_input->p_current_data;

        if( i_count <= 0 )
        {
            /* Go to the next buffer */
            i_count = input_FillBuffer( p_input );

            if( i_count < 0 ) return -1;
            else if( i_count == 0 )
            {
                /* We reached the EOF */
                break;
            }
        }

        i_count = __MIN( i_data, i_count );
        memcpy( p, p_input->p_current_data, i_count );
        p_input->p_current_data += i_count;
        p += i_count;
        i_data -= i_count;
        i_read += i_count;

        /* Update stream position */
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.p_selected_area->i_tell += i_count;
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }

    return i_read;
}

/****************************************************************************
 * IStreamPeek:
 ****************************************************************************/
static int IStreamPeek( stream_t *s, uint8_t **pp_peek, int i_peek )
{
    input_stream_sys_t * p_sys = (input_stream_sys_t*)s->p_sys;
    return input_Peek( p_sys->p_input, pp_peek, i_peek );
}

/****************************************************************************
 * stream_Demux*: create a demuxer for an outpout stream (allow demuxer chain)
 ****************************************************************************/
typedef struct
{
    /* Data buffer */
    vlc_mutex_t lock;
    int         i_buffer;
    int         i_buffer_size;
    uint8_t     *p_buffer;

    int64_t     i_pos;

    /* Demuxer */
    char        *psz_name;
    es_out_t    *out;
    demux_t     *p_demux;
} d_stream_sys_t;

static int DStreamRead   ( stream_t *, void *p_read, int i_read );
static int DStreamPeek   ( stream_t *, uint8_t **pp_peek, int i_peek );
static int DStreamControl( stream_t *, int i_query, va_list );
static int DStreamThread ( stream_t * );


stream_t *__stream_DemuxNew( vlc_object_t *p_obj, char *psz_demux, es_out_t *out )
{
    /* We create a stream reader, and launch a thread */
    stream_t       *s;
    d_stream_sys_t *p_sys;

    if( psz_demux == NULL || *psz_demux == '\0' )
    {
        return NULL;
    }

    s = vlc_object_create( p_obj, sizeof( stream_t ) );
    s->pf_block  = NULL;
    s->pf_read   = DStreamRead;
    s->pf_peek   = DStreamPeek;
    s->pf_control= DStreamControl;

    s->p_sys = malloc( sizeof( d_stream_sys_t) );
    p_sys = (d_stream_sys_t*)s->p_sys;

    vlc_mutex_init( s, &p_sys->lock );
    p_sys->i_buffer = 0;
    p_sys->i_buffer_size = 1000000;
    p_sys->p_buffer = malloc( p_sys->i_buffer_size );
    p_sys->i_pos = 0;
    p_sys->psz_name = strdup( psz_demux );
    p_sys->out = out;
    p_sys->p_demux = NULL;

    if( vlc_thread_create( s, "stream out", DStreamThread, VLC_THREAD_PRIORITY_INPUT, VLC_FALSE ) )
    {
        vlc_mutex_destroy( &p_sys->lock );
        vlc_object_destroy( s );
        free( p_sys );
        return NULL;
    }

    return s;
}

void     stream_DemuxSend( stream_t *s, block_t *p_block )
{
    d_stream_sys_t *p_sys = (d_stream_sys_t*)s->p_sys;

    if( p_block->i_buffer > 0 )
    {
        vlc_mutex_lock( &p_sys->lock );
        /* Realloc if needed */
        if( p_sys->i_buffer + p_block->i_buffer > p_sys->i_buffer_size )
        {
            if( p_sys->i_buffer_size > 5000000 )
            {
                vlc_mutex_unlock( &p_sys->lock );
                msg_Err( s, "stream_DemuxSend: buffer size > 5000000" );
                block_Release( p_block );
                return;
            }
            /* I know, it's more than needed but that's perfect */
            p_sys->i_buffer_size += p_block->i_buffer;
            /* FIXME won't work with PEEK -> segfault */
            p_sys->p_buffer = realloc( p_sys->p_buffer, p_sys->i_buffer_size );
            msg_Dbg( s, "stream_DemuxSend: realloc to %d", p_sys->i_buffer_size );
        }

        /* copy data */
        memcpy( &p_sys->p_buffer[p_sys->i_buffer], p_block->p_buffer, p_block->i_buffer );
        p_sys->i_buffer += p_block->i_buffer;

        vlc_mutex_unlock( &p_sys->lock );
    }

    block_Release( p_block );
}

void     stream_DemuxDelete( stream_t *s )
{
    d_stream_sys_t *p_sys = (d_stream_sys_t*)s->p_sys;

    s->b_die = VLC_TRUE;

    vlc_mutex_lock( &p_sys->lock );
    if( p_sys->p_demux )
    {
        p_sys->p_demux->b_die = VLC_TRUE;
    }
    vlc_mutex_unlock( &p_sys->lock );

    vlc_thread_join( s );

    if( p_sys->p_demux )
    {
        demux2_Delete( p_sys->p_demux );
    }
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys->psz_name );
    free( p_sys->p_buffer );
    free( p_sys );
    vlc_object_destroy( s );
}


static int      DStreamRead   ( stream_t *s, void *p_read, int i_read )
{
    d_stream_sys_t *p_sys = (d_stream_sys_t*)s->p_sys;
    int           i_copy;

    //msg_Dbg( s, "DStreamRead: wanted %d bytes", i_read );
    for( ;; )
    {
        vlc_mutex_lock( &p_sys->lock );
        //msg_Dbg( s, "DStreamRead: buffer %d", p_sys->i_buffer );
        if( p_sys->i_buffer >= i_read || s->b_die )
        {
            break;
        }
        vlc_mutex_unlock( &p_sys->lock );
        msleep( 10000 );
    }

    //msg_Dbg( s, "DStreamRead: read %d buffer %d", i_read, p_sys->i_buffer );

    i_copy = __MIN( i_read, p_sys->i_buffer );
    if( i_copy > 0 )
    {
        if( p_read )
        {
            memcpy( p_read, p_sys->p_buffer, i_copy );
        }
        p_sys->i_buffer -= i_copy;
        p_sys->i_pos += i_copy;

        if( p_sys->i_buffer > 0 )
        {
            memmove( p_sys->p_buffer, &p_sys->p_buffer[i_copy], p_sys->i_buffer );
        }

    }
    vlc_mutex_unlock( &p_sys->lock );

    return i_copy;
}
static int      DStreamPeek   ( stream_t *s, uint8_t **pp_peek, int i_peek )
{
    d_stream_sys_t *p_sys = (d_stream_sys_t*)s->p_sys;
    int           i_copy;

    //msg_Dbg( s, "DStreamPeek: wanted %d bytes", i_peek );
    for( ;; )
    {
        vlc_mutex_lock( &p_sys->lock );
        //msg_Dbg( s, "DStreamPeek: buffer %d", p_sys->i_buffer );
        if( p_sys->i_buffer >= i_peek || s->b_die )
        {
            break;
        }
        vlc_mutex_unlock( &p_sys->lock );
        msleep( 10000 );
    }
    *pp_peek = p_sys->p_buffer;
    i_copy = __MIN( i_peek, p_sys->i_buffer );

    vlc_mutex_unlock( &p_sys->lock );

    return i_copy;
}

static int      DStreamControl( stream_t *s, int i_query, va_list args )
{
    d_stream_sys_t *p_sys = (d_stream_sys_t*)s->p_sys;
    int64_t    *p_i64;
    vlc_bool_t *p_b;
    int        *p_int;
    switch( i_query )
    {
        case STREAM_GET_SIZE:
            p_i64 = (int64_t*) va_arg( args, int64_t * );
            *p_i64 = 0;
            return VLC_SUCCESS;

        case STREAM_CAN_SEEK:
            p_b = (vlc_bool_t*) va_arg( args, vlc_bool_t * );
            *p_b = VLC_FALSE;
            return VLC_SUCCESS;

        case STREAM_CAN_FASTSEEK:
            p_b = (vlc_bool_t*) va_arg( args, vlc_bool_t * );
            *p_b = VLC_FALSE;
            return VLC_SUCCESS;

        case STREAM_GET_POSITION:
            p_i64 = (int64_t*) va_arg( args, int64_t * );
            *p_i64 = p_sys->i_pos;
            return VLC_SUCCESS;

        case STREAM_SET_POSITION:
            return VLC_EGENERIC;

        case STREAM_GET_MTU:
            p_int = (int*) va_arg( args, int * );
            *p_int = 0;
            return VLC_SUCCESS;

        default:
            msg_Err( s, "invalid DStreamControl query=0x%x", i_query );
            return VLC_EGENERIC;
    }
}

static int      DStreamThread ( stream_t *s )
{
    d_stream_sys_t *p_sys = (d_stream_sys_t*)s->p_sys;
    demux_t      *p_demux;

    /* Create the demuxer */

    if( ( p_demux = demux2_New( s, "", p_sys->psz_name, "", s, p_sys->out ) ) == NULL )
    {
        return VLC_EGENERIC;
    }

    vlc_mutex_lock( &p_sys->lock );
    p_sys->p_demux = p_demux;
    vlc_mutex_unlock( &p_sys->lock );

    /* Main loop */
    while( !s->b_die && !p_demux->b_die )
    {
        if( p_demux->pf_demux( p_demux ) <= 0 )
        {
            break;
        }
    }
    p_demux->b_die = VLC_TRUE;
    return VLC_SUCCESS;
}





