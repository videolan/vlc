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

static ssize_t DStreamRead( stream_t *, void *p_read, size_t i_read );
static int  DStreamControl( stream_t *, int i_query, va_list );
static void DStreamDelete ( stream_t * );
static void* DStreamThread ( void * );


stream_t *stream_DemuxNew( demux_t *p_demux, const char *psz_demux, es_out_t *out )
{
    vlc_object_t *p_obj = VLC_OBJECT(p_demux);
    /* We create a stream reader, and launch a thread */
    stream_t     *s;
    stream_sys_t *p_sys;

    s = stream_CommonNew( p_obj, DStreamDelete );
    if( s == NULL )
        return NULL;
    s->p_input = p_demux->p_input;
    s->pf_read   = DStreamRead;
    s->pf_seek   = NULL;
    s->pf_control= DStreamControl;

    s->p_sys = p_sys = malloc( sizeof( *p_sys) );
    if( unlikely(p_sys == NULL) )
        goto error;

    p_sys->out = out;
    p_sys->p_block = NULL;
    p_sys->psz_name = strdup( psz_demux );
    p_sys->stats.position = 0.;
    p_sys->stats.length = 0;
    p_sys->stats.time = 0;

    /* decoder fifo */
    if( ( p_sys->p_fifo = block_FifoNew() ) == NULL )
    {
        free( p_sys->psz_name );
        goto error;
    }

    atomic_init( &p_sys->active, true );
    vlc_mutex_init( &p_sys->lock );

    if( vlc_clone( &p_sys->thread, DStreamThread, s, VLC_THREAD_PRIORITY_INPUT ) )
    {
        vlc_mutex_destroy( &p_sys->lock );
        block_FifoRelease( p_sys->p_fifo );
        free( p_sys->psz_name );
        goto error;
    }

    return s;
error:
    free( p_sys );
    stream_CommonDelete( s );
    return NULL;
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
}


static ssize_t DStreamRead( stream_t *s, void *buf, size_t len )
{
    stream_sys_t *sys = s->p_sys;

    if( !atomic_load( &sys->active ) )
        return -1;
    if( len == 0 )
        return 0;

    //msg_Dbg( s, "DStreamRead: wanted %d bytes", i_read );

    block_t *block = sys->p_block;
    if( block == NULL )
    {
        block = block_FifoGet( sys->p_fifo );
        sys->p_block = block;
    }

    size_t copy = __MIN( len, block->i_buffer );
    if( buf != NULL && copy > 0 )
        memcpy( buf, block->p_buffer, copy );

    block->p_buffer += copy;
    block->i_buffer -= copy;
    if( block->i_buffer == 0 )
    {
        block_Release( block );
        sys->p_block = NULL;
    }

    return copy;
}

static int DStreamControl( stream_t *s, int i_query, va_list args )
{
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

        case STREAM_GET_PTS_DELAY:
            *va_arg( args, int64_t * ) = DEFAULT_PTS_DELAY;
            return VLC_SUCCESS;

        case STREAM_GET_TITLE_INFO:
        case STREAM_GET_TITLE:
        case STREAM_GET_SEEKPOINT:
        case STREAM_GET_META:
        case STREAM_GET_CONTENT_TYPE:
        case STREAM_GET_SIGNAL:
        case STREAM_SET_PAUSE_STATE:
        case STREAM_SET_TITLE:
        case STREAM_SET_SEEKPOINT:
        case STREAM_SET_RECORD_STATE:
        case STREAM_SET_PRIVATE_ID_STATE:
        case STREAM_SET_PRIVATE_ID_CA:
        case STREAM_GET_PRIVATE_ID_STATE:
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
    p_demux = demux_NewAdvanced( s, s->p_input, "", p_sys->psz_name, "",
                                 s, p_sys->out, false );
    if( p_demux == NULL )
        return NULL;

    /* stream_Demux cannot apply DVB filters.
     * Get all programs and let the E/S output sort them out. */
    demux_Control( p_demux, DEMUX_SET_GROUP, -1, NULL );

    /* Main loop */
    mtime_t next_update = 0;
    while( atomic_load( &p_sys->active ) )
    {
        if( demux_TestAndClearFlags( p_demux, UINT_MAX )
         || mdate() >= next_update )
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

            next_update = mdate() + (CLOCK_FREQ / 4);
        }

        if( demux_Demux( p_demux ) <= 0 )
            break;
    }

    /* Explicit kludge: the stream is destroyed by the owner of the
     * streamDemux, not here. */
    p_demux->s = NULL;
    demux_Delete( p_demux );

    return NULL;
}
