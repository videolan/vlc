/*****************************************************************************
 * stream_demux.c
 *****************************************************************************
 * Copyright (C) 1999-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <limits.h>

#include "demux.h"
#include <libvlc.h>
#include <vlc_codec.h>
#include <vlc_atomic.h>

/****************************************************************************
 * stream_Demux*: create a demuxer for an outpout stream (allow demuxer chain)
 ****************************************************************************/
struct stream_sys_t
{
    /* Data buffer */
    block_fifo_t *p_fifo;
    block_t      *p_block;

    uint64_t    i_pos;

    /* Demuxer */
    char        *psz_name;
    es_out_t    *out;

    atomic_bool  active;
    vlc_thread_t thread;
    vlc_mutex_t  lock;
    struct
    {
        double  position;
        int64_t length;
        int64_t time;
    } stats;
};

static int  DStreamRead   ( stream_t *, void *p_read, unsigned int i_read );
static int  DStreamPeek   ( stream_t *, const uint8_t **pp_peek, unsigned int i_peek );
static int  DStreamControl( stream_t *, int i_query, va_list );
static void DStreamDelete ( stream_t * );
static void* DStreamThread ( void * );


stream_t *stream_DemuxNew( demux_t *p_demux, const char *psz_demux, es_out_t *out )
{
    vlc_object_t *p_obj = VLC_OBJECT(p_demux);
    /* We create a stream reader, and launch a thread */
    stream_t     *s;
    stream_sys_t *p_sys;

    s = stream_CommonNew( p_obj );
    if( s == NULL )
        return NULL;
    s->p_input = p_demux->p_input;
    s->psz_path  = strdup(""); /* N/A */
    s->pf_read   = DStreamRead;
    s->pf_peek   = DStreamPeek;
    s->pf_control= DStreamControl;
    s->pf_destroy= DStreamDelete;

    s->p_sys = p_sys = malloc( sizeof( *p_sys) );
    if( !s->psz_path || !s->p_sys )
    {
        stream_CommonDelete( s );
        return NULL;
    }

    p_sys->i_pos = 0;
    p_sys->out = out;
    p_sys->p_block = NULL;
    p_sys->psz_name = strdup( psz_demux );
    p_sys->stats.position = 0.;
    p_sys->stats.length = 0;
    p_sys->stats.time = 0;

    /* decoder fifo */
    if( ( p_sys->p_fifo = block_FifoNew() ) == NULL )
    {
        stream_CommonDelete( s );
        free( p_sys->psz_name );
        free( p_sys );
        return NULL;
    }

    atomic_init( &p_sys->active, true );
    vlc_mutex_init( &p_sys->lock );

    if( vlc_clone( &p_sys->thread, DStreamThread, s, VLC_THREAD_PRIORITY_INPUT ) )
    {
        vlc_mutex_destroy( &p_sys->lock );
        block_FifoRelease( p_sys->p_fifo );
        stream_CommonDelete( s );
        free( p_sys->psz_name );
        free( p_sys );
        return NULL;
    }

    return s;
}

void stream_DemuxSend( stream_t *s, block_t *p_block )
{
    stream_sys_t *p_sys = s->p_sys;
    block_FifoPut( p_sys->p_fifo, p_block );
}

int stream_DemuxControlVa( stream_t *s, int query, va_list args )
{
    stream_sys_t *sys = s->p_sys;

    switch( query )
    {
        case DEMUX_GET_POSITION:
            vlc_mutex_lock( &sys->lock );
            *va_arg( args, double * ) = sys->stats.position;
            vlc_mutex_unlock( &sys->lock );
            break;
        case DEMUX_GET_LENGTH:
            vlc_mutex_lock( &sys->lock );
            *va_arg( args, int64_t * ) = sys->stats.length;
            vlc_mutex_unlock( &sys->lock );
            break;
        case DEMUX_GET_TIME:
            vlc_mutex_lock( &sys->lock );
            *va_arg( args, int64_t * ) = sys->stats.time;
            vlc_mutex_unlock( &sys->lock );
            break;
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void DStreamDelete( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;
    block_t *p_empty;

    atomic_store( &p_sys->active, false );
    p_empty = block_Alloc( 0 );
    block_FifoPut( p_sys->p_fifo, p_empty );
    vlc_join( p_sys->thread, NULL );
    vlc_mutex_destroy( &p_sys->lock );

    if( p_sys->p_block )
        block_Release( p_sys->p_block );

    block_FifoRelease( p_sys->p_fifo );
    free( p_sys->psz_name );
    free( p_sys );
    stream_CommonDelete( s );
}


static int DStreamRead( stream_t *s, void *p_read, unsigned int i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    uint8_t *p_out = p_read;
    int i_out = 0;

    //msg_Dbg( s, "DStreamRead: wanted %d bytes", i_read );

    while( atomic_load( &p_sys->active ) && !s->b_error && i_read )
    {
        block_t *p_block = p_sys->p_block;
        int i_copy;

        if( !p_block )
        {
            p_block = block_FifoGet( p_sys->p_fifo );
            if( !p_block ) s->b_error = 1;
            p_sys->p_block = p_block;
        }

        if( p_block && i_read )
        {
            i_copy = __MIN( i_read, p_block->i_buffer );
            if( p_out && i_copy ) memcpy( p_out, p_block->p_buffer, i_copy );
            i_read -= i_copy;
            p_out += i_copy;
            i_out += i_copy;
            p_block->i_buffer -= i_copy;
            p_block->p_buffer += i_copy;

            if( !p_block->i_buffer )
            {
                block_Release( p_block );
                p_sys->p_block = NULL;
            }
        }
    }

    p_sys->i_pos += i_out;
    return i_out;
}

static int DStreamPeek( stream_t *s, const uint8_t **pp_peek, unsigned int i_peek )
{
    stream_sys_t *p_sys = s->p_sys;
    block_t **pp_block = &p_sys->p_block;
    int i_out = 0;
    *pp_peek = 0;

    //msg_Dbg( s, "DStreamPeek: wanted %d bytes", i_peek );

    while( atomic_load( &p_sys->active ) && !s->b_error && i_peek )
    {
        int i_copy;

        if( !*pp_block )
        {
            *pp_block = block_FifoGet( p_sys->p_fifo );
            if( !*pp_block ) s->b_error = 1;
        }

        if( *pp_block && i_peek )
        {
            i_copy = __MIN( i_peek, (*pp_block)->i_buffer );
            i_peek -= i_copy;
            i_out += i_copy;

            if( i_peek ) pp_block = &(*pp_block)->p_next;
        }
    }

    if( p_sys->p_block )
    {
        p_sys->p_block = block_ChainGather( p_sys->p_block );
        *pp_peek = p_sys->p_block->p_buffer;
    }

    return i_out;
}

static int DStreamControl( stream_t *s, int i_query, va_list args )
{
    stream_sys_t *p_sys = s->p_sys;
    uint64_t    *p_i64;

    switch( i_query )
    {
        case STREAM_GET_SIZE:
            p_i64 = va_arg( args, uint64_t * );
            *p_i64 = 0;
            return VLC_SUCCESS;

        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg( args, bool * ) = false;
            return VLC_SUCCESS;

        case STREAM_GET_POSITION:
            p_i64 = va_arg( args, uint64_t * );
            *p_i64 = p_sys->i_pos;
            return VLC_SUCCESS;

        case STREAM_SET_POSITION:
        {
            uint64_t i64 = va_arg( args, uint64_t );
            if( i64 < p_sys->i_pos )
                return VLC_EGENERIC;

            uint64_t i_skip = i64 - p_sys->i_pos;
            while( i_skip > 0 )
            {
                int i_read = DStreamRead( s, NULL, __MIN(i_skip, INT_MAX) );
                if( i_read <= 0 )
                    return VLC_EGENERIC;
                i_skip -= i_read;
            }
            return VLC_SUCCESS;
        }

        case STREAM_CONTROL_ACCESS:
        case STREAM_GET_TITLE_INFO:
        case STREAM_GET_META:
        case STREAM_GET_CONTENT_TYPE:
        case STREAM_GET_SIGNAL:
        case STREAM_SET_PAUSE_STATE:
        case STREAM_SET_TITLE:
        case STREAM_SET_SEEKPOINT:
        case STREAM_SET_RECORD_STATE:
            return VLC_EGENERIC;

        default:
            msg_Err( s, "invalid DStreamControl query=0x%x", i_query );
            return VLC_EGENERIC;
    }
}

static void* DStreamThread( void *obj )
{
    stream_t *s = (stream_t *)obj;
    stream_sys_t *p_sys = s->p_sys;
    demux_t *p_demux;

    /* Create the demuxer */
    p_demux = demux_New( s, s->p_input, "", p_sys->psz_name, "", s, p_sys->out,
                         false );
    if( p_demux == NULL )
        return NULL;

    /* stream_Demux cannot apply DVB filters.
     * Get all programs and let the E/S output sort them out. */
    demux_Control( p_demux, DEMUX_SET_GROUP, -1, NULL );

    /* Main loop */
    mtime_t next_update = 0;
    while( atomic_load( &p_sys->active ) )
    {
        if( p_demux->info.i_update || mdate() >= next_update )
        {
            double newpos;
            int64_t newlen, newtime;

            if( demux_Control( p_demux, DEMUX_GET_POSITION, &newpos ) )
                newpos = 0.;
            if( demux_Control( p_demux, DEMUX_GET_LENGTH, &newlen ) )
                newlen = 0;
            if( demux_Control( p_demux, DEMUX_GET_TIME, &newtime ) )
                newtime = 0;

            vlc_mutex_lock( &p_sys->lock );
            p_sys->stats.position = newpos;
            p_sys->stats.length = newlen;
            p_sys->stats.time = newtime;
            vlc_mutex_unlock( &p_sys->lock );

            p_demux->info.i_update = 0;
            next_update = mdate() + (CLOCK_FREQ / 4);
        }

        if( demux_Demux( p_demux ) <= 0 )
            break;
    }

    demux_Delete( p_demux );

    return NULL;
}
