/*****************************************************************************
 * stream_access.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>

#include <libvlc.h>
#include "stream.h"
#include "input_internal.h"

// #define STREAM_DEBUG 1

/* TODO:
 *  - tune the 2 methods (block/stream)
 *  - compute cost for seek
 *  - improve stream mode seeking with closest segments
 *  - ...
 */

/* Two methods:
 *  - using pf_block
 *      One linked list of data read
 *  - using pf_read
 *      More complex scheme using mutliple track to avoid seeking
 *  - using directly the access (only indirection for peeking).
 *      This method is known to introduce much less latency.
 *      It should probably defaulted (instead of the stream method (2)).
 */

/* How many tracks we have, currently only used for stream mode */
#ifdef OPTIMIZE_MEMORY
#   define STREAM_CACHE_TRACK 1
    /* Max size of our cache 128Ko per track */
#   define STREAM_CACHE_SIZE  (STREAM_CACHE_TRACK*1024*128)
#else
#   define STREAM_CACHE_TRACK 3
    /* Max size of our cache 4Mo per track */
#   define STREAM_CACHE_SIZE  (4*STREAM_CACHE_TRACK*1024*1024)
#endif

/* How many data we try to prebuffer
 * XXX it should be small to avoid useless latency but big enough for
 * efficient demux probing */
#define STREAM_CACHE_PREBUFFER_SIZE (128)

/* Method1: Simple, for pf_block.
 *  We get blocks and put them in the linked list.
 *  We release blocks once the total size is bigger than CACHE_BLOCK_SIZE
 */

/* Method2: A bit more complex, for pf_read
 *  - We use ring buffers, only one if unseekable, all if seekable
 *  - Upon seek date current ring, then search if one ring match the pos,
 *      yes: switch to it, seek the access to match the end of the ring
 *      no: search the ring with i_end the closer to i_pos,
 *          if close enough, read data and use this ring
 *          else use the oldest ring, seek and use it.
 *
 *  TODO: - with access non seekable: use all space available for only one ring, but
 *          we have to support seekable/non-seekable switch on the fly.
 *        - compute a good value for i_read_size
 *        - ?
 */
#define STREAM_READ_ATONCE 1024
#define STREAM_CACHE_TRACK_SIZE (STREAM_CACHE_SIZE/STREAM_CACHE_TRACK)

typedef struct
{
    int64_t i_date;

    uint64_t i_start;
    uint64_t i_end;

    uint8_t *p_buffer;

} stream_track_t;

typedef enum
{
    STREAM_METHOD_BLOCK,
    STREAM_METHOD_STREAM,
    STREAM_METHOD_READDIR
} stream_read_method_t;

struct stream_sys_t
{
    access_t    *p_access;

    stream_read_method_t   method;    /* method to use */

    uint64_t     i_pos;      /* Current reading offset */

    /* Method 1: pf_block */
    struct
    {
        uint64_t i_start;        /* Offset of block for p_first */
        uint64_t i_offset;       /* Offset for data in p_current */
        block_t *p_current;     /* Current block */

        uint64_t i_size;         /* Total amount of data in the list */
        block_t *p_first;
        block_t **pp_last;

    } block;

    /* Method 2: for pf_read */
    struct
    {
        unsigned i_offset;   /* Buffer offset in the current track */
        int      i_tk;       /* Current track */
        stream_track_t tk[STREAM_CACHE_TRACK];

        /* Global buffer */
        uint8_t *p_buffer;

        /* */
        unsigned i_used; /* Used since last read */
        unsigned i_read_size;

    } stream;

    /* Stat for both method */
    struct
    {
        /* Stat about reading data */
        uint64_t i_read_count;
        uint64_t i_bytes;
        uint64_t i_read_time;
    } stat;
};

/* Method 1: */
static ssize_t AStreamReadBlock( stream_t *, void *, size_t );
static int  AStreamSeekBlock( stream_t *s, uint64_t i_pos );
static void AStreamPrebufferBlock( stream_t *s );
static block_t *AReadBlock( stream_t *s, bool *pb_eof );

/* Method 2 */
static ssize_t AStreamReadStream( stream_t *, void *, size_t );
static int  AStreamSeekStream( stream_t *s, uint64_t i_pos );
static void AStreamPrebufferStream( stream_t *s );
static ssize_t AReadStream( stream_t *s, void *p_read, size_t i_read );

/* ReadDir */
static input_item_t *AStreamReadDir( stream_t *s );

/* Common */
static ssize_t AStreamReadError( stream_t *s, void *p_read, size_t i_read )
{
    (void) s; (void) p_read; (void) i_read;
    return VLC_EGENERIC;
}
static input_item_t * AStreamReadDirError( stream_t *s )
{
    (void) s;
    return NULL;
}
static int AStreamControl( stream_t *s, int i_query, va_list );
static void AStreamDestroy( stream_t *s );

stream_t *stream_AccessNew( access_t *p_access )
{
    stream_t *s = stream_CommonNew( VLC_OBJECT(p_access) );
    stream_sys_t *p_sys;

    if( !s )
        return NULL;

    s->p_input = p_access->p_input;
    if( asprintf( &s->psz_url, "%s://%s", p_access->psz_access,
                  p_access->psz_location ) == -1 )
        s->psz_url = NULL;
    s->p_sys = p_sys = malloc( sizeof( *p_sys ) );
    if( unlikely(s->psz_url == NULL || s->p_sys == NULL) )
    {
        free( s->p_sys );
        stream_CommonDelete( s );
        return NULL;
    }

    s->pf_read    = AStreamReadError;    /* Replaced later */
    s->pf_readdir = AStreamReadDirError; /* Replaced later */
    s->pf_control = AStreamControl;
    s->pf_destroy = AStreamDestroy;

    /* Common field */
    p_sys->p_access = p_access;
    assert( p_access->pf_block || p_access->pf_read || p_access->pf_readdir );
    if( p_access->pf_block )
        p_sys->method = STREAM_METHOD_BLOCK;
    else if( p_access->pf_read )
        p_sys->method = STREAM_METHOD_STREAM;
    else
        p_sys->method = STREAM_METHOD_READDIR;

    p_sys->i_pos = p_access->info.i_pos;

    /* Stats */
    p_sys->stat.i_bytes = 0;
    p_sys->stat.i_read_time = 0;
    p_sys->stat.i_read_count = 0;

    if( p_sys->method == STREAM_METHOD_BLOCK )
    {
        msg_Dbg( s, "Using block method for AStream*" );
        s->pf_read = AStreamReadBlock;

        /* Init all fields of p_sys->block */
        p_sys->block.i_start = p_sys->i_pos;
        p_sys->block.i_offset = 0;
        p_sys->block.p_current = NULL;
        p_sys->block.i_size = 0;
        p_sys->block.p_first = NULL;
        p_sys->block.pp_last = &p_sys->block.p_first;

        /* Do the prebuffering */
        AStreamPrebufferBlock( s );

        if( p_sys->block.i_size <= 0 )
        {
            msg_Err( s, "cannot pre fill buffer" );
            goto error;
        }
    }
    else if ( p_sys->method == STREAM_METHOD_STREAM )
    {
        int i;

        msg_Dbg( s, "Using stream method for AStream*" );

        s->pf_read = AStreamReadStream;

        /* Allocate/Setup our tracks */
        p_sys->stream.i_offset = 0;
        p_sys->stream.i_tk     = 0;
        p_sys->stream.p_buffer = malloc( STREAM_CACHE_SIZE );
        if( p_sys->stream.p_buffer == NULL )
            goto error;
        p_sys->stream.i_used   = 0;
        p_sys->stream.i_read_size = STREAM_READ_ATONCE;
#if STREAM_READ_ATONCE < 256
#   error "Invalid STREAM_READ_ATONCE value"
#endif

        for( i = 0; i < STREAM_CACHE_TRACK; i++ )
        {
            p_sys->stream.tk[i].i_date  = 0;
            p_sys->stream.tk[i].i_start = p_sys->i_pos;
            p_sys->stream.tk[i].i_end   = p_sys->i_pos;
            p_sys->stream.tk[i].p_buffer=
                &p_sys->stream.p_buffer[i * STREAM_CACHE_TRACK_SIZE];
        }

        /* Do the prebuffering */
        AStreamPrebufferStream( s );

        if( p_sys->stream.tk[p_sys->stream.i_tk].i_end <= 0 )
        {
            msg_Err( s, "cannot pre fill buffer" );
            goto error;
        }
    }
    else
    {
        msg_Dbg( s, "Using readdir method for AStream*" );

        assert( p_sys->method == STREAM_METHOD_READDIR );
        s->pf_readdir = AStreamReadDir;
    }

    return s;

error:
    if( p_sys->method == STREAM_METHOD_BLOCK )
    {
        /* Nothing yet */
    }
    else if( p_sys->method == STREAM_METHOD_STREAM )
    {
        free( p_sys->stream.p_buffer );
    }
    free( s->p_sys );
    stream_CommonDelete( s );
    vlc_access_Delete( p_access );
    return NULL;
}

/****************************************************************************
 * AStreamDestroy:
 ****************************************************************************/
static void AStreamDestroy( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;

    if( p_sys->method == STREAM_METHOD_BLOCK )
        block_ChainRelease( p_sys->block.p_first );
    else if( p_sys->method == STREAM_METHOD_STREAM )
        free( p_sys->stream.p_buffer );

    stream_CommonDelete( s );
    vlc_access_Delete( p_sys->p_access );
    free( p_sys );
}

/****************************************************************************
 * AStreamControlReset:
 ****************************************************************************/
static void AStreamControlReset( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;

    p_sys->i_pos = p_sys->p_access->info.i_pos;

    if( p_sys->method == STREAM_METHOD_BLOCK )
    {
        block_ChainRelease( p_sys->block.p_first );

        /* Init all fields of p_sys->block */
        p_sys->block.i_start = p_sys->i_pos;
        p_sys->block.i_offset = 0;
        p_sys->block.p_current = NULL;
        p_sys->block.i_size = 0;
        p_sys->block.p_first = NULL;
        p_sys->block.pp_last = &p_sys->block.p_first;

        /* Do the prebuffering */
        AStreamPrebufferBlock( s );
    }
    else
    {
        int i;

        assert( p_sys->method == STREAM_METHOD_STREAM );

        /* Setup our tracks */
        p_sys->stream.i_offset = 0;
        p_sys->stream.i_tk     = 0;
        p_sys->stream.i_used   = 0;

        for( i = 0; i < STREAM_CACHE_TRACK; i++ )
        {
            p_sys->stream.tk[i].i_date  = 0;
            p_sys->stream.tk[i].i_start = p_sys->i_pos;
            p_sys->stream.tk[i].i_end   = p_sys->i_pos;
        }

        /* Do the prebuffering */
        AStreamPrebufferStream( s );
    }
}

#define static_control_match(foo) \
    static_assert((unsigned) STREAM_##foo == ACCESS_##foo, "Mismatch")

/****************************************************************************
 * AStreamControl:
 ****************************************************************************/
static int AStreamControl( stream_t *s, int i_query, va_list args )
{
    stream_sys_t *p_sys = s->p_sys;
    access_t     *p_access = p_sys->p_access;

    static_control_match(CAN_SEEK);
    static_control_match(CAN_FASTSEEK);
    static_control_match(CAN_PAUSE);
    static_control_match(CAN_CONTROL_PACE);
    static_control_match(GET_SIZE);
    static_control_match(GET_PTS_DELAY);
    static_control_match(GET_TITLE_INFO);
    static_control_match(GET_TITLE);
    static_control_match(GET_SEEKPOINT);
    static_control_match(GET_META);
    static_control_match(GET_CONTENT_TYPE);
    static_control_match(GET_SIGNAL);
    static_control_match(SET_PAUSE_STATE);
    static_control_match(SET_TITLE);
    static_control_match(SET_SEEKPOINT);
    static_control_match(SET_PRIVATE_ID_STATE);
    static_control_match(SET_PRIVATE_ID_CA);
    static_control_match(GET_PRIVATE_ID_STATE);

    switch( i_query )
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
        case STREAM_GET_SIZE:
        case STREAM_GET_PTS_DELAY:
        case STREAM_GET_TITLE_INFO:
        case STREAM_GET_TITLE:
        case STREAM_GET_SEEKPOINT:
        case STREAM_GET_META:
        case STREAM_GET_CONTENT_TYPE:
        case STREAM_GET_SIGNAL:
        case STREAM_SET_PAUSE_STATE:
        case STREAM_SET_PRIVATE_ID_STATE:
        case STREAM_SET_PRIVATE_ID_CA:
        case STREAM_GET_PRIVATE_ID_STATE:
            return access_vaControl( p_access, i_query, args );

        case STREAM_GET_POSITION:
            *va_arg( args, uint64_t * ) = p_sys->i_pos;
            break;

        case STREAM_SET_POSITION:
        {
            uint64_t offset = va_arg( args, uint64_t );
            switch( p_sys->method )
            {
            case STREAM_METHOD_BLOCK:
                return AStreamSeekBlock( s, offset );
            case STREAM_METHOD_STREAM:
                return AStreamSeekStream( s, offset );
            default:
                vlc_assert_unreachable();
                return VLC_EGENERIC;
            }
        }

        case STREAM_SET_TITLE:
        case STREAM_SET_SEEKPOINT:
        {
            int ret = access_vaControl( p_access, i_query, args );
            if( ret == VLC_SUCCESS )
                AStreamControlReset( s );
            return ret;
        }

        case STREAM_IS_DIRECTORY:
        {
            bool *pb_canreaddir = va_arg( args, bool * );
            bool *pb_dirsorted = va_arg( args, bool * );
            bool *pb_dircanloop = va_arg( args, bool * );
            *pb_canreaddir = p_sys->method == STREAM_METHOD_READDIR;
            if( pb_dirsorted )
                *pb_dirsorted = p_access->info.b_dir_sorted;
            if( pb_dircanloop )
                *pb_dircanloop = p_access->info.b_dir_can_loop;
            return VLC_SUCCESS;
        }

        case STREAM_SET_RECORD_STATE:
        default:
            msg_Err( s, "invalid stream_vaControl query=0x%x", i_query );
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/****************************************************************************
 * Method 1:
 ****************************************************************************/
static void AStreamPrebufferBlock( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;

    int64_t i_first = 0;
    int64_t i_start;

    msg_Dbg( s, "starting pre-buffering" );
    i_start = mdate();
    for( ;; )
    {
        const int64_t i_date = mdate();
        bool b_eof;
        block_t *b;

        if( vlc_killed() || p_sys->block.i_size > STREAM_CACHE_PREBUFFER_SIZE )
        {
            int64_t i_byterate;

            /* Update stat */
            p_sys->stat.i_bytes = p_sys->block.i_size;
            p_sys->stat.i_read_time = i_date - i_start;
            i_byterate = ( CLOCK_FREQ * p_sys->stat.i_bytes ) /
                         (p_sys->stat.i_read_time + 1);

            msg_Dbg( s, "prebuffering done %"PRId64" bytes in %"PRId64"s - "
                     "%"PRId64" KiB/s",
                     p_sys->stat.i_bytes,
                     p_sys->stat.i_read_time / CLOCK_FREQ,
                     i_byterate / 1024 );
            break;
        }

        /* Fetch a block */
        if( ( b = AReadBlock( s, &b_eof ) ) == NULL )
        {
            if( b_eof )
                break;
            continue;
        }

        while( b )
        {
            /* Append the block */
            p_sys->block.i_size += b->i_buffer;
            *p_sys->block.pp_last = b;
            p_sys->block.pp_last = &b->p_next;

            p_sys->stat.i_read_count++;
            b = b->p_next;
        }

        if( i_first == 0 )
        {
            i_first = mdate();
            msg_Dbg( s, "received first data after %d ms",
                     (int)((i_first-i_start)/1000) );
        }
    }

    p_sys->block.p_current = p_sys->block.p_first;
}

static int AStreamRefillBlock( stream_t *s );

static ssize_t AStreamReadBlock( stream_t *s, void *p_read, size_t i_read )
{
    stream_sys_t *p_sys = s->p_sys;

    uint8_t *p_data = p_read;
    size_t i_data = 0;

    /* It means EOF */
    if( p_sys->block.p_current == NULL )
        return 0;

    if( p_data == NULL )
    {
        /* seek within this stream if possible, else use plain old read and discard */
        access_t *p_access = p_sys->p_access;
        bool b_aseek;

        access_Control( p_access, ACCESS_CAN_SEEK, &b_aseek );
        if( b_aseek )
            return AStreamSeekBlock( s, p_sys->i_pos + i_read ) ? 0 : i_read;
    }

    while( i_data < i_read )
    {
        ssize_t i_current =
            p_sys->block.p_current->i_buffer - p_sys->block.i_offset;
        size_t i_copy = VLC_CLIP( (size_t)i_current, 0, i_read - i_data);

        /* Copy data */
        if( p_data )
        {
            memcpy( p_data,
                    &p_sys->block.p_current->p_buffer[p_sys->block.i_offset],
                    i_copy );
            p_data += i_copy;
        }
        i_data += i_copy;

        p_sys->block.i_offset += i_copy;
        if( p_sys->block.i_offset >= p_sys->block.p_current->i_buffer )
        {
            /* Current block is now empty, switch to next */
            p_sys->block.i_offset = 0;
            p_sys->block.p_current = p_sys->block.p_current->p_next;

            /*Get a new block if needed */
            if( !p_sys->block.p_current && AStreamRefillBlock( s ) )
                break;
            assert( p_sys->block.p_current );
        }
    }

    p_sys->i_pos += i_data;
    return i_data;
}

static int AStreamSeekBlock( stream_t *s, uint64_t i_pos )
{
    stream_sys_t *p_sys = s->p_sys;
    access_t   *p_access = p_sys->p_access;
    int64_t    i_offset = i_pos - p_sys->block.i_start;
    bool b_seek;

    /* We already have thoses data, just update p_current/i_offset */
    if( i_offset >= 0 && (uint64_t)i_offset < p_sys->block.i_size )
    {
        block_t *b = p_sys->block.p_first;
        int i_current = 0;

        while( i_current + b->i_buffer < (uint64_t)i_offset )
        {
            i_current += b->i_buffer;
            b = b->p_next;
        }

        p_sys->block.p_current = b;
        p_sys->block.i_offset = i_offset - i_current;

        p_sys->i_pos = i_pos;

        return VLC_SUCCESS;
    }

    /* We may need to seek or to read data */
    if( i_offset < 0 )
    {
        bool b_aseek;
        access_Control( p_access, ACCESS_CAN_SEEK, &b_aseek );

        if( !b_aseek )
        {
            msg_Err( s, "backward seeking impossible (access not seekable)" );
            return VLC_EGENERIC;
        }

        b_seek = true;
    }
    else
    {
        bool b_aseek, b_aseekfast;

        access_Control( p_access, ACCESS_CAN_SEEK, &b_aseek );
        access_Control( p_access, ACCESS_CAN_FASTSEEK, &b_aseekfast );

        if( !b_aseek )
        {
            b_seek = false;
            msg_Warn( s, "%"PRId64" bytes need to be skipped "
                      "(access non seekable)",
                      i_offset - p_sys->block.i_size );
        }
        else
        {
            int64_t i_skip = i_offset - p_sys->block.i_size;

            /* Avg bytes per packets */
            int i_avg = p_sys->stat.i_bytes / p_sys->stat.i_read_count;
            /* TODO compute a seek cost instead of fixed threshold */
            int i_th = b_aseekfast ? 1 : 5;

            if( i_skip <= i_th * i_avg &&
                i_skip < STREAM_CACHE_SIZE )
                b_seek = false;
            else
                b_seek = true;

            msg_Dbg( s, "b_seek=%d th*avg=%d skip=%"PRId64,
                     b_seek, i_th*i_avg, i_skip );
        }
    }

    if( b_seek )
    {
        /* Do the access seek */
        if( vlc_access_Seek( p_access, i_pos ) ) return VLC_EGENERIC;

        /* Release data */
        block_ChainRelease( p_sys->block.p_first );

        /* Reinit */
        p_sys->block.i_start = p_sys->i_pos = i_pos;
        p_sys->block.i_offset = 0;
        p_sys->block.p_current = NULL;
        p_sys->block.i_size = 0;
        p_sys->block.p_first = NULL;
        p_sys->block.pp_last = &p_sys->block.p_first;

        /* Refill a block */
        if( AStreamRefillBlock( s ) )
            return VLC_EGENERIC;

        return VLC_SUCCESS;
    }
    else
    {
        do
        {
            while( p_sys->block.p_current &&
                   p_sys->i_pos + p_sys->block.p_current->i_buffer - p_sys->block.i_offset <= i_pos )
            {
                p_sys->i_pos += p_sys->block.p_current->i_buffer - p_sys->block.i_offset;
                p_sys->block.p_current = p_sys->block.p_current->p_next;
                p_sys->block.i_offset = 0;
            }
            if( !p_sys->block.p_current && AStreamRefillBlock( s ) )
            {
                if( p_sys->i_pos != i_pos )
                    return VLC_EGENERIC;
            }
        }
        while( p_sys->block.i_start + p_sys->block.i_size < i_pos );

        p_sys->block.i_offset += i_pos - p_sys->i_pos;
        p_sys->i_pos = i_pos;

        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

static int AStreamRefillBlock( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;

    /* Release data */
    while( p_sys->block.i_size >= STREAM_CACHE_SIZE &&
           p_sys->block.p_first != p_sys->block.p_current )
    {
        block_t *b = p_sys->block.p_first;

        p_sys->block.i_start += b->i_buffer;
        p_sys->block.i_size  -= b->i_buffer;
        p_sys->block.p_first  = b->p_next;

        block_Release( b );
    }
    if( p_sys->block.i_size >= STREAM_CACHE_SIZE &&
        p_sys->block.p_current == p_sys->block.p_first &&
        p_sys->block.p_current->p_next )    /* At least 2 packets */
    {
        /* Enough data, don't read more */
        return VLC_SUCCESS;
    }

    /* Now read a new block */
    const int64_t i_start = mdate();
    block_t *b;

    for( ;; )
    {
        bool b_eof;

        if( vlc_killed() )
            return VLC_EGENERIC;

        /* Fetch a block */
        if( ( b = AReadBlock( s, &b_eof ) ) )
            break;
        if( b_eof )
            return VLC_EGENERIC;
    }

    p_sys->stat.i_read_time += mdate() - i_start;
    while( b )
    {
        /* Append the block */
        p_sys->block.i_size += b->i_buffer;
        *p_sys->block.pp_last = b;
        p_sys->block.pp_last = &b->p_next;

        /* Fix p_current */
        if( p_sys->block.p_current == NULL )
            p_sys->block.p_current = b;

        /* Update stat */
        p_sys->stat.i_bytes += b->i_buffer;
        p_sys->stat.i_read_count++;

        b = b->p_next;
    }
    return VLC_SUCCESS;
}


/****************************************************************************
 * Method 2:
 ****************************************************************************/
static int AStreamRefillStream( stream_t *s );
static ssize_t AStreamReadNoSeekStream( stream_t *, void *, size_t );

static ssize_t AStreamReadStream( stream_t *s, void *p_read, size_t i_read )
{
    stream_sys_t *p_sys = s->p_sys;

    if( !p_read )
    {
        const uint64_t i_pos_wanted = p_sys->i_pos + i_read;

        if( AStreamSeekStream( s, i_pos_wanted ) )
        {
            if( p_sys->i_pos != i_pos_wanted )
                return 0;
        }
        return i_read;
    }
    return AStreamReadNoSeekStream( s, p_read, i_read );
}

static int AStreamSeekStream( stream_t *s, uint64_t i_pos )
{
    stream_sys_t *p_sys = s->p_sys;

    stream_track_t *p_current = &p_sys->stream.tk[p_sys->stream.i_tk];
    access_t *p_access = p_sys->p_access;

    if( p_current->i_start >= p_current->i_end  && i_pos >= p_current->i_end )
        return 0; /* EOF */

#ifdef STREAM_DEBUG
    msg_Dbg( s, "AStreamSeekStream: to %"PRId64" pos=%"PRId64
             " tk=%d start=%"PRId64" offset=%d end=%"PRId64,
             i_pos, p_sys->i_pos, p_sys->stream.i_tk,
             p_current->i_start,
             p_sys->stream.i_offset,
             p_current->i_end );
#endif

    bool   b_aseek;
    access_Control( p_access, ACCESS_CAN_SEEK, &b_aseek );
    if( !b_aseek && i_pos < p_current->i_start )
    {
        msg_Warn( s, "AStreamSeekStream: can't seek" );
        return VLC_EGENERIC;
    }

    bool   b_afastseek;
    access_Control( p_access, ACCESS_CAN_FASTSEEK, &b_afastseek );

    /* FIXME compute seek cost (instead of static 'stupid' value) */
    uint64_t i_skip_threshold;
    if( b_aseek )
        i_skip_threshold = b_afastseek ? 128 : 3*p_sys->stream.i_read_size;
    else
        i_skip_threshold = INT64_MAX;

    /* Date the current track */
    p_current->i_date = mdate();

    /* Search a new track slot */
    stream_track_t *tk = NULL;
    int i_tk_idx = -1;

    /* Prefer the current track */
    if( p_current->i_start <= i_pos && i_pos <= p_current->i_end + i_skip_threshold )
    {
        tk = p_current;
        i_tk_idx = p_sys->stream.i_tk;
    }
    if( !tk )
    {
        /* Try to maximize already read data */
        for( int i = 0; i < STREAM_CACHE_TRACK; i++ )
        {
            stream_track_t *t = &p_sys->stream.tk[i];

            if( t->i_start > i_pos || i_pos > t->i_end )
                continue;

            if( !tk || tk->i_end < t->i_end )
            {
                tk = t;
                i_tk_idx = i;
            }
        }
    }
    if( !tk )
    {
        /* Use the oldest unused */
        for( int i = 0; i < STREAM_CACHE_TRACK; i++ )
        {
            stream_track_t *t = &p_sys->stream.tk[i];

            if( !tk || tk->i_date > t->i_date )
            {
                tk = t;
                i_tk_idx = i;
            }
        }
    }
    assert( i_tk_idx >= 0 && i_tk_idx < STREAM_CACHE_TRACK );

    if( tk != p_current )
        i_skip_threshold = 0;
    if( tk->i_start <= i_pos && i_pos <= tk->i_end + i_skip_threshold )
    {
#ifdef STREAM_DEBUG
        msg_Err( s, "AStreamSeekStream: reusing %d start=%"PRId64
                 " end=%"PRId64"(%s)",
                 i_tk_idx, tk->i_start, tk->i_end,
                 tk != p_current ? "seek" : i_pos > tk->i_end ? "skip" : "noseek" );
#endif
        if( tk != p_current )
        {
            assert( b_aseek );

            /* Seek at the end of the buffer
             * TODO it is stupid to seek now, it would be better to delay it
             */
            if( vlc_access_Seek( p_access, tk->i_end ) )
                return VLC_EGENERIC;
        }
        else if( i_pos > tk->i_end )
        {
            uint64_t i_skip = i_pos - tk->i_end;
            while( i_skip > 0 )
            {
                const int i_read_max = __MIN( 10 * STREAM_READ_ATONCE, i_skip );
                if( AStreamReadNoSeekStream( s, NULL, i_read_max ) != i_read_max )
                    return VLC_EGENERIC;
                i_skip -= i_read_max;
            }
        }
    }
    else
    {
#ifdef STREAM_DEBUG
        msg_Err( s, "AStreamSeekStream: hard seek" );
#endif
        /* Nothing good, seek and choose oldest segment */
        if( vlc_access_Seek( p_access, i_pos ) )
            return VLC_EGENERIC;

        tk->i_start = i_pos;
        tk->i_end   = i_pos;
    }
    p_sys->stream.i_offset = i_pos - tk->i_start;
    p_sys->stream.i_tk = i_tk_idx;
    p_sys->i_pos = i_pos;

    /* If there is not enough data left in the track, refill  */
    /* TODO How to get a correct value for
     *    - refilling threshold
     *    - how much to refill
     */
    if( tk->i_end < tk->i_start + p_sys->stream.i_offset + p_sys->stream.i_read_size )
    {
        if( p_sys->stream.i_used < STREAM_READ_ATONCE / 2 )
            p_sys->stream.i_used = STREAM_READ_ATONCE / 2;

        if( AStreamRefillStream( s ) && i_pos >= tk->i_end )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static ssize_t AStreamReadNoSeekStream( stream_t *s, void *p_read,
                                        size_t i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    stream_track_t *tk = &p_sys->stream.tk[p_sys->stream.i_tk];

    uint8_t *p_data = (uint8_t *)p_read;
    size_t i_data = 0;

    if( tk->i_start >= tk->i_end )
        return 0; /* EOF */

#ifdef STREAM_DEBUG
    msg_Dbg( s, "AStreamReadStream: %d pos=%"PRId64" tk=%d start=%"PRId64
             " offset=%d end=%"PRId64,
             i_read, p_sys->i_pos, p_sys->stream.i_tk,
             tk->i_start, p_sys->stream.i_offset, tk->i_end );
#endif

    while( i_data < i_read )
    {
        unsigned i_off = (tk->i_start + p_sys->stream.i_offset) % STREAM_CACHE_TRACK_SIZE;
        size_t i_current =
            __MIN( tk->i_end - tk->i_start - p_sys->stream.i_offset,
                   STREAM_CACHE_TRACK_SIZE - i_off );
        ssize_t i_copy = __MIN( i_current, i_read - i_data );

        if( i_copy <= 0 ) break; /* EOF */

        /* Copy data */
        /* msg_Dbg( s, "AStreamReadStream: copy %d", i_copy ); */
        if( p_data )
        {
            memcpy( p_data, &tk->p_buffer[i_off], i_copy );
            p_data += i_copy;
        }
        i_data += i_copy;
        p_sys->stream.i_offset += i_copy;

        /* Update pos now */
        p_sys->i_pos += i_copy;

        /* */
        p_sys->stream.i_used += i_copy;

        if( tk->i_end + i_data <= tk->i_start + p_sys->stream.i_offset + i_read )
        {
            const size_t i_read_requested = VLC_CLIP( i_read - i_data,
                                                    STREAM_READ_ATONCE / 2,
                                                    STREAM_READ_ATONCE * 10 );

            if( p_sys->stream.i_used < i_read_requested )
                p_sys->stream.i_used = i_read_requested;

            if( AStreamRefillStream( s ) )
            {
                /* EOF */
                if( tk->i_start >= tk->i_end ) break;
            }
        }
    }

    return i_data;
}


static int AStreamRefillStream( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;
    stream_track_t *tk = &p_sys->stream.tk[p_sys->stream.i_tk];

    /* We read but won't increase i_start after initial start + offset */
    int i_toread =
        __MIN( p_sys->stream.i_used, STREAM_CACHE_TRACK_SIZE -
               (tk->i_end - tk->i_start - p_sys->stream.i_offset) );
    bool b_read = false;
    int64_t i_start, i_stop;

    if( i_toread <= 0 ) return VLC_EGENERIC; /* EOF */

#ifdef STREAM_DEBUG
    msg_Dbg( s, "AStreamRefillStream: used=%d toread=%d",
                 p_sys->stream.i_used, i_toread );
#endif

    i_start = mdate();
    while( i_toread > 0 )
    {
        int i_off = tk->i_end % STREAM_CACHE_TRACK_SIZE;
        int i_read;

        if( vlc_killed() )
            return VLC_EGENERIC;

        i_read = __MIN( i_toread, STREAM_CACHE_TRACK_SIZE - i_off );
        i_read = AReadStream( s, &tk->p_buffer[i_off], i_read );

        /* msg_Dbg( s, "AStreamRefillStream: read=%d", i_read ); */
        if( i_read <  0 )
        {
            continue;
        }
        else if( i_read == 0 )
        {
            if( !b_read )
                return VLC_EGENERIC;
            return VLC_SUCCESS;
        }
        b_read = true;

        /* Update end */
        tk->i_end += i_read;

        /* Windows of STREAM_CACHE_TRACK_SIZE */
        if( tk->i_start + STREAM_CACHE_TRACK_SIZE < tk->i_end )
        {
            unsigned i_invalid = tk->i_end - tk->i_start - STREAM_CACHE_TRACK_SIZE;

            tk->i_start += i_invalid;
            p_sys->stream.i_offset -= i_invalid;
        }

        i_toread -= i_read;
        p_sys->stream.i_used -= i_read;

        p_sys->stat.i_bytes += i_read;
        p_sys->stat.i_read_count++;
    }
    i_stop = mdate();

    p_sys->stat.i_read_time += i_stop - i_start;

    return VLC_SUCCESS;
}

static void AStreamPrebufferStream( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;

    int64_t i_first = 0;
    int64_t i_start;

    msg_Dbg( s, "starting pre-buffering" );
    i_start = mdate();
    for( ;; )
    {
        stream_track_t *tk = &p_sys->stream.tk[p_sys->stream.i_tk];

        int64_t i_date = mdate();
        int i_read;
        int i_buffered = tk->i_end - tk->i_start;

        if( vlc_killed() || i_buffered >= STREAM_CACHE_PREBUFFER_SIZE )
        {
            int64_t i_byterate;

            /* Update stat */
            p_sys->stat.i_bytes = i_buffered;
            p_sys->stat.i_read_time = i_date - i_start;
            i_byterate = ( CLOCK_FREQ * p_sys->stat.i_bytes ) /
                         (p_sys->stat.i_read_time+1);

            msg_Dbg( s, "pre-buffering done %"PRId64" bytes in %"PRId64"s - "
                     "%"PRId64" KiB/s",
                     p_sys->stat.i_bytes,
                     p_sys->stat.i_read_time / CLOCK_FREQ,
                     i_byterate / 1024 );
            break;
        }

        /* */
        i_read = STREAM_CACHE_TRACK_SIZE - i_buffered;
        i_read = __MIN( (int)p_sys->stream.i_read_size, i_read );
        i_read = AReadStream( s, &tk->p_buffer[i_buffered], i_read );
        if( i_read <  0 )
            continue;
        else if( i_read == 0 )
            break;  /* EOF */

        if( i_first == 0 )
        {
            i_first = mdate();
            msg_Dbg( s, "received first data after %d ms",
                     (int)((i_first-i_start)/1000) );
        }

        tk->i_end += i_read;

        p_sys->stat.i_read_count++;
    }
}

/****************************************************************************
 * Access reading/seeking wrappers to handle concatenated streams.
 ****************************************************************************/
static ssize_t AReadStream( stream_t *s, void *p_read, size_t i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    input_thread_t *p_input = s->p_input;

    i_read = vlc_access_Read( p_sys->p_access, p_read, i_read );
    if( p_input != NULL )
    {
        uint64_t total;

        vlc_mutex_lock( &p_input->p->counters.counters_lock );
        stats_Update( p_input->p->counters.p_read_bytes, i_read, &total );
        stats_Update( p_input->p->counters.p_input_bitrate, total, NULL );
        stats_Update( p_input->p->counters.p_read_packets, 1, NULL );
        vlc_mutex_unlock( &p_input->p->counters.counters_lock );
    }
    return i_read;
}

static block_t *AReadBlock( stream_t *s, bool *pb_eof )
{
    stream_sys_t *p_sys = s->p_sys;
    access_t *p_access = p_sys->p_access;
    input_thread_t *p_input = s->p_input;

    block_t *p_block = p_access->pf_block( p_access );

    if( pb_eof != NULL )
        *pb_eof = p_access->info.b_eof;

    if( p_input != NULL && p_block != NULL && libvlc_stats (p_access) )
    {
        uint64_t total;

        vlc_mutex_lock( &p_input->p->counters.counters_lock );
        stats_Update( p_input->p->counters.p_read_bytes, p_block->i_buffer,
                      &total );
        stats_Update( p_input->p->counters.p_input_bitrate, total, NULL );
        stats_Update( p_input->p->counters.p_read_packets, 1, NULL );
        vlc_mutex_unlock( &p_input->p->counters.counters_lock );
    }
    return p_block;
}

static input_item_t *AStreamReadDir( stream_t *s )
{
    access_t *p_access = s->p_sys->p_access;

    return p_access->pf_readdir( p_access );
}
