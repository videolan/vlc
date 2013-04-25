/*****************************************************************************
 * stream.c
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

#include <dirent.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_strings.h>
#include <vlc_memory.h>

#include <libvlc.h>

#include "access.h"
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

typedef struct
{
    char     *psz_path;
    uint64_t  i_size;

} access_entry_t;

typedef enum
{
    STREAM_METHOD_BLOCK,
    STREAM_METHOD_STREAM
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

    /* Peek temporary buffer */
    unsigned int i_peek;
    uint8_t *p_peek;

    /* Stat for both method */
    struct
    {
        bool b_fastseek;  /* From access */

        /* Stat about reading data */
        uint64_t i_read_count;
        uint64_t i_bytes;
        uint64_t i_read_time;

        /* Stat about seek */
        unsigned i_seek_count;
        uint64_t i_seek_time;

    } stat;

    /* Streams list */
    int            i_list;
    access_entry_t **list;
    int            i_list_index;
    access_t       *p_list_access;
};

/* Method 1: */
static int  AStreamReadBlock( stream_t *s, void *p_read, unsigned int i_read );
static int  AStreamPeekBlock( stream_t *s, const uint8_t **p_peek, unsigned int i_read );
static int  AStreamSeekBlock( stream_t *s, uint64_t i_pos );
static void AStreamPrebufferBlock( stream_t *s );
static block_t *AReadBlock( stream_t *s, bool *pb_eof );

/* Method 2 */
static int  AStreamReadStream( stream_t *s, void *p_read, unsigned int i_read );
static int  AStreamPeekStream( stream_t *s, const uint8_t **pp_peek, unsigned int i_read );
static int  AStreamSeekStream( stream_t *s, uint64_t i_pos );
static void AStreamPrebufferStream( stream_t *s );
static int  AReadStream( stream_t *s, void *p_read, unsigned int i_read );

/* Common */
static int AStreamControl( stream_t *s, int i_query, va_list );
static void AStreamDestroy( stream_t *s );
static int  ASeek( stream_t *s, uint64_t i_pos );

/****************************************************************************
 * stream_CommonNew: create an empty stream structure
 ****************************************************************************/
stream_t *stream_CommonNew( vlc_object_t *p_obj )
{
    stream_t *s = (stream_t *)vlc_custom_create( p_obj, sizeof(*s), "stream" );

    if( !s )
        return NULL;

    s->p_text = malloc( sizeof(*s->p_text) );
    if( !s->p_text )
    {
        vlc_object_release( s );
        return NULL;
    }

    /* UTF16 and UTF32 text file conversion */
    s->p_text->conv = (vlc_iconv_t)(-1);
    s->p_text->i_char_width = 1;
    s->p_text->b_little_endian = false;

    return s;
}

void stream_CommonDelete( stream_t *s )
{
    if( s->p_text )
    {
        if( s->p_text->conv != (vlc_iconv_t)(-1) )
            vlc_iconv_close( s->p_text->conv );
        free( s->p_text );
    }
    free( s->psz_access );
    free( s->psz_path );
    vlc_object_release( s );
}

#undef stream_UrlNew
/****************************************************************************
 * stream_UrlNew: create a stream from a access
 ****************************************************************************/
stream_t *stream_UrlNew( vlc_object_t *p_parent, const char *psz_url )
{
    const char *psz_access, *psz_demux, *psz_path, *psz_anchor;
    access_t *p_access;

    if( !psz_url )
        return NULL;

    char psz_dup[strlen( psz_url ) + 1];
    strcpy( psz_dup, psz_url );
    input_SplitMRL( &psz_access, &psz_demux, &psz_path, &psz_anchor, psz_dup );

    /* Now try a real access */
    p_access = access_New( p_parent, NULL, psz_access, psz_demux, psz_path );
    if( p_access == NULL )
    {
        msg_Err( p_parent, "no suitable access module for `%s'", psz_url );
        return NULL;
    }

    return stream_AccessNew( p_access, NULL );
}

stream_t *stream_AccessNew( access_t *p_access, char **ppsz_list )
{
    stream_t *s = stream_CommonNew( VLC_OBJECT(p_access) );
    stream_sys_t *p_sys;

    if( !s )
        return NULL;

    s->p_input = p_access->p_input;
    s->psz_access = strdup( p_access->psz_access );
    s->psz_path = strdup( p_access->psz_location );
    s->p_sys = p_sys = malloc( sizeof( *p_sys ) );
    if( !s->psz_access || !s->psz_path || !s->p_sys )
    {
        stream_CommonDelete( s );
        return NULL;
    }

    s->pf_read   = NULL;    /* Set up later */
    s->pf_peek   = NULL;
    s->pf_control = AStreamControl;
    s->pf_destroy = AStreamDestroy;

    /* Common field */
    p_sys->p_access = p_access;
    if( p_access->pf_block )
        p_sys->method = STREAM_METHOD_BLOCK;
    else
        p_sys->method = STREAM_METHOD_STREAM;

    p_sys->i_pos = p_access->info.i_pos;

    /* Stats */
    access_Control( p_access, ACCESS_CAN_FASTSEEK, &p_sys->stat.b_fastseek );
    p_sys->stat.i_bytes = 0;
    p_sys->stat.i_read_time = 0;
    p_sys->stat.i_read_count = 0;
    p_sys->stat.i_seek_count = 0;
    p_sys->stat.i_seek_time = 0;

    TAB_INIT( p_sys->i_list, p_sys->list );
    p_sys->i_list_index = 0;
    p_sys->p_list_access = NULL;

    /* Get the additional list of inputs if any (for concatenation) */
    if( ppsz_list && ppsz_list[0] )
    {
        access_entry_t *p_entry = malloc( sizeof(*p_entry) );
        if( !p_entry )
            goto error;

        p_entry->i_size = p_access->info.i_size;
        p_entry->psz_path = strdup( p_access->psz_location );
        if( !p_entry->psz_path )
        {
            free( p_entry );
            goto error;
        }
        p_sys->p_list_access = p_access;
        TAB_APPEND( p_sys->i_list, p_sys->list, p_entry );
        msg_Dbg( p_access, "adding file `%s', (%"PRId64" bytes)",
                 p_entry->psz_path, p_access->info.i_size );

        for( int i = 0; ppsz_list[i] != NULL; i++ )
        {
            char *psz_name = strdup( ppsz_list[i] );

            if( !psz_name )
                break;

            access_t *p_tmp = access_New( p_access, p_access->p_input,
                                          p_access->psz_access, "", psz_name );
            if( !p_tmp )
                continue;

            msg_Dbg( p_access, "adding file `%s', (%"PRId64" bytes)",
                     psz_name, p_tmp->info.i_size );

            p_entry = malloc( sizeof(*p_entry) );
            if( p_entry )
            {
                p_entry->i_size = p_tmp->info.i_size;
                p_entry->psz_path = psz_name;
                TAB_APPEND( p_sys->i_list, p_sys->list, p_entry );
            }
            access_Delete( p_tmp );
        }
    }

    /* Peek */
    p_sys->i_peek = 0;
    p_sys->p_peek = NULL;

    if( p_sys->method == STREAM_METHOD_BLOCK )
    {
        msg_Dbg( s, "Using block method for AStream*" );
        s->pf_read = AStreamReadBlock;
        s->pf_peek = AStreamPeekBlock;

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
    else
    {
        int i;

        assert( p_sys->method == STREAM_METHOD_STREAM );

        msg_Dbg( s, "Using stream method for AStream*" );

        s->pf_read = AStreamReadStream;
        s->pf_peek = AStreamPeekStream;

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

    return s;

error:
    if( p_sys->method == STREAM_METHOD_BLOCK )
    {
        /* Nothing yet */
    }
    else
    {
        free( p_sys->stream.p_buffer );
    }
    while( p_sys->i_list > 0 )
        free( p_sys->list[--(p_sys->i_list)] );
    free( p_sys->list );
    free( s->p_sys );
    stream_CommonDelete( s );
    access_Delete( p_access );
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
    else
        free( p_sys->stream.p_buffer );

    free( p_sys->p_peek );

    if( p_sys->p_list_access && p_sys->p_list_access != p_sys->p_access )
        access_Delete( p_sys->p_list_access );

    while( p_sys->i_list-- )
    {
        free( p_sys->list[p_sys->i_list]->psz_path );
        free( p_sys->list[p_sys->i_list] );
    }
    free( p_sys->list );

    stream_CommonDelete( s );
    access_Delete( p_sys->p_access );
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

/****************************************************************************
 * AStreamControlUpdate:
 ****************************************************************************/
static void AStreamControlUpdate( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;

    p_sys->i_pos = p_sys->p_access->info.i_pos;

    if( p_sys->i_list )
    {
        int i;
        for( i = 0; i < p_sys->i_list_index; i++ )
        {
            p_sys->i_pos += p_sys->list[i]->i_size;
        }
    }
}

/****************************************************************************
 * AStreamControl:
 ****************************************************************************/
static int AStreamControl( stream_t *s, int i_query, va_list args )
{
    stream_sys_t *p_sys = s->p_sys;
    access_t     *p_access = p_sys->p_access;

    uint64_t *pi_64, i_64;

    switch( i_query )
    {
        case STREAM_GET_SIZE:
            pi_64 = va_arg( args, uint64_t * );
            if( s->p_sys->i_list )
            {
                int i;
                *pi_64 = 0;
                for( i = 0; i < s->p_sys->i_list; i++ )
                    *pi_64 += s->p_sys->list[i]->i_size;
                break;
            }
            *pi_64 = p_access->info.i_size;
            break;

        case STREAM_CAN_SEEK:
            return access_vaControl( p_access, ACCESS_CAN_SEEK, args );
        case STREAM_CAN_FASTSEEK:
            return access_vaControl( p_access, ACCESS_CAN_FASTSEEK, args );
        case STREAM_CAN_PAUSE:
            return access_vaControl( p_access, ACCESS_CAN_PAUSE, args );
        case STREAM_CAN_CONTROL_PACE:
            return access_vaControl( p_access, ACCESS_CAN_CONTROL_PACE, args );

        case STREAM_GET_POSITION:
            pi_64 = va_arg( args, uint64_t * );
            *pi_64 = p_sys->i_pos;
            break;

        case STREAM_SET_POSITION:
            i_64 = va_arg( args, uint64_t );
            switch( p_sys->method )
            {
            case STREAM_METHOD_BLOCK:
                return AStreamSeekBlock( s, i_64 );
            case STREAM_METHOD_STREAM:
                return AStreamSeekStream( s, i_64 );
            default:
                assert(0);
                return VLC_EGENERIC;
            }

        case STREAM_CONTROL_ACCESS:
        {
            int i_int = (int) va_arg( args, int );
            if( i_int != ACCESS_SET_PRIVATE_ID_STATE &&
                i_int != ACCESS_SET_PRIVATE_ID_CA &&
                i_int != ACCESS_GET_PRIVATE_ID_STATE )
            {
                msg_Err( s, "Hey, what are you thinking ?"
                            "DON'T USE STREAM_CONTROL_ACCESS !!!" );
                return VLC_EGENERIC;
            }
            return access_vaControl( p_access, i_int, args );
        }

        case STREAM_UPDATE_SIZE:
            AStreamControlUpdate( s );
            return VLC_SUCCESS;

        case STREAM_GET_TITLE_INFO:
            return access_vaControl( p_access, ACCESS_GET_TITLE_INFO, args );
        case STREAM_GET_META:
            return access_vaControl( p_access, ACCESS_GET_META, args );
        case STREAM_GET_CONTENT_TYPE:
            return access_vaControl( p_access, ACCESS_GET_CONTENT_TYPE, args );
        case STREAM_GET_SIGNAL:
            return access_vaControl( p_access, ACCESS_GET_SIGNAL, args );

        case STREAM_SET_PAUSE_STATE:
            return access_vaControl( p_access, ACCESS_SET_PAUSE_STATE, args );
        case STREAM_SET_TITLE:
        {
            int ret = access_vaControl( p_access, ACCESS_SET_TITLE, args );
            if( ret == VLC_SUCCESS )
                AStreamControlReset( s );
            return ret;
        }
        case STREAM_SET_SEEKPOINT:
        {
            int ret = access_vaControl( p_access, ACCESS_SET_SEEKPOINT, args );
            if( ret == VLC_SUCCESS )
                AStreamControlReset( s );
            return ret;
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

        if( !vlc_object_alive(s) || p_sys->block.i_size > STREAM_CACHE_PREBUFFER_SIZE )
        {
            int64_t i_byterate;

            /* Update stat */
            p_sys->stat.i_bytes = p_sys->block.i_size;
            p_sys->stat.i_read_time = i_date - i_start;
            i_byterate = ( INT64_C(1000000) * p_sys->stat.i_bytes ) /
                         (p_sys->stat.i_read_time + 1);

            msg_Dbg( s, "prebuffering done %"PRId64" bytes in %"PRId64"s - "
                     "%"PRId64" KiB/s",
                     p_sys->stat.i_bytes,
                     p_sys->stat.i_read_time / INT64_C(1000000),
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

static int AStreamReadBlock( stream_t *s, void *p_read, unsigned int i_read )
{
    stream_sys_t *p_sys = s->p_sys;

    uint8_t *p_data = p_read;
    unsigned int i_data = 0;

    /* It means EOF */
    if( p_sys->block.p_current == NULL )
        return 0;

    if( p_data == NULL )
    {
        /* seek within this stream if possible, else use plain old read and discard */
        stream_sys_t *p_sys = s->p_sys;
        access_t     *p_access = p_sys->p_access;
        bool   b_aseek;
        access_Control( p_access, ACCESS_CAN_SEEK, &b_aseek );
        if( b_aseek )
            return AStreamSeekBlock( s, p_sys->i_pos + i_read ) ? 0 : i_read;
    }

    while( i_data < i_read )
    {
        int i_current =
            p_sys->block.p_current->i_buffer - p_sys->block.i_offset;
        unsigned int i_copy = VLC_CLIP( (unsigned int)i_current, 0, i_read - i_data);

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
            if( p_sys->block.p_current )
            {
                p_sys->block.i_offset = 0;
                p_sys->block.p_current = p_sys->block.p_current->p_next;
            }
            /*Get a new block if needed */
            if( !p_sys->block.p_current && AStreamRefillBlock( s ) )
            {
                break;
            }
        }
    }

    p_sys->i_pos += i_data;
    return i_data;
}

static int AStreamPeekBlock( stream_t *s, const uint8_t **pp_peek, unsigned int i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    uint8_t *p_data;
    unsigned int i_data = 0;
    block_t *b;
    unsigned int i_offset;

    if( p_sys->block.p_current == NULL ) return 0; /* EOF */

    /* We can directly give a pointer over our buffer */
    if( i_read <= p_sys->block.p_current->i_buffer - p_sys->block.i_offset )
    {
        *pp_peek = &p_sys->block.p_current->p_buffer[p_sys->block.i_offset];
        return i_read;
    }

    /* We need to create a local copy */
    if( p_sys->i_peek < i_read )
    {
        p_sys->p_peek = realloc_or_free( p_sys->p_peek, i_read );
        if( !p_sys->p_peek )
        {
            p_sys->i_peek = 0;
            return 0;
        }
        p_sys->i_peek = i_read;
    }

    /* Fill enough data */
    while( p_sys->block.i_size - (p_sys->i_pos - p_sys->block.i_start)
           < i_read )
    {
        block_t **pp_last = p_sys->block.pp_last;

        if( AStreamRefillBlock( s ) ) break;

        /* Our buffer are probably filled enough, don't try anymore */
        if( pp_last == p_sys->block.pp_last ) break;
    }

    /* Copy what we have */
    b = p_sys->block.p_current;
    i_offset = p_sys->block.i_offset;
    p_data = p_sys->p_peek;

    while( b && i_data < i_read )
    {
        unsigned int i_current = __MAX(b->i_buffer - i_offset,0);
        int i_copy = __MIN( i_current, i_read - i_data );

        memcpy( p_data, &b->p_buffer[i_offset], i_copy );
        i_data += i_copy;
        p_data += i_copy;
        i_offset += i_copy;

        if( i_offset >= b->i_buffer )
        {
            i_offset = 0;
            b = b->p_next;
        }
    }

    *pp_peek = p_sys->p_peek;
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
        int64_t i_start, i_end;
        /* Do the access seek */
        i_start = mdate();
        if( ASeek( s, i_pos ) ) return VLC_EGENERIC;
        i_end = mdate();

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

        /* Update stat */
        p_sys->stat.i_seek_time += i_end - i_start;
        p_sys->stat.i_seek_count++;
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
    block_t      *b;

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
    for( ;; )
    {
        bool b_eof;

        if( !vlc_object_alive(s) )
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
static int AStreamReadNoSeekStream( stream_t *s, void *p_read, unsigned int i_read );

static int AStreamReadStream( stream_t *s, void *p_read, unsigned int i_read )
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

static int AStreamPeekStream( stream_t *s, const uint8_t **pp_peek, unsigned int i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    stream_track_t *tk = &p_sys->stream.tk[p_sys->stream.i_tk];
    uint64_t i_off;

    if( tk->i_start >= tk->i_end ) return 0; /* EOF */

#ifdef STREAM_DEBUG
    msg_Dbg( s, "AStreamPeekStream: %d pos=%"PRId64" tk=%d "
             "start=%"PRId64" offset=%d end=%"PRId64,
             i_read, p_sys->i_pos, p_sys->stream.i_tk,
             tk->i_start, p_sys->stream.i_offset, tk->i_end );
#endif

    /* Avoid problem, but that should *never* happen */
    if( i_read > STREAM_CACHE_TRACK_SIZE / 2 )
        i_read = STREAM_CACHE_TRACK_SIZE / 2;

    while( tk->i_end < tk->i_start + p_sys->stream.i_offset + i_read )
    {
        if( p_sys->stream.i_used <= 1 )
        {
            /* Be sure we will read something */
            p_sys->stream.i_used += tk->i_start + p_sys->stream.i_offset + i_read - tk->i_end;
        }
        if( AStreamRefillStream( s ) ) break;
    }

    if( tk->i_end < tk->i_start + p_sys->stream.i_offset + i_read )
    {
        i_read = tk->i_end - tk->i_start - p_sys->stream.i_offset;
    }


    /* Now, direct pointer or a copy ? */
    i_off = (tk->i_start + p_sys->stream.i_offset) % STREAM_CACHE_TRACK_SIZE;
    if( i_off + i_read <= STREAM_CACHE_TRACK_SIZE )
    {
        *pp_peek = &tk->p_buffer[i_off];
        return i_read;
    }

    if( p_sys->i_peek < i_read )
    {
        p_sys->p_peek = realloc_or_free( p_sys->p_peek, i_read );
        if( !p_sys->p_peek )
        {
            p_sys->i_peek = 0;
            return 0;
        }
        p_sys->i_peek = i_read;
    }

    memcpy( p_sys->p_peek, &tk->p_buffer[i_off],
            STREAM_CACHE_TRACK_SIZE - i_off );
    memcpy( &p_sys->p_peek[STREAM_CACHE_TRACK_SIZE - i_off],
            &tk->p_buffer[0], i_read - (STREAM_CACHE_TRACK_SIZE - i_off) );

    *pp_peek = p_sys->p_peek;
    return i_read;
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
            if( ASeek( s, tk->i_end ) )
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
        if( ASeek( s, i_pos ) )
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

static int AStreamReadNoSeekStream( stream_t *s, void *p_read, unsigned int i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    stream_track_t *tk = &p_sys->stream.tk[p_sys->stream.i_tk];

    uint8_t *p_data = (uint8_t *)p_read;
    unsigned int i_data = 0;

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
        unsigned int i_current =
            __MIN( tk->i_end - tk->i_start - p_sys->stream.i_offset,
                   STREAM_CACHE_TRACK_SIZE - i_off );
        int i_copy = __MIN( i_current, i_read - i_data );

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
            const unsigned i_read_requested = VLC_CLIP( i_read - i_data,
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

        if( !vlc_object_alive(s) )
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

        if( !vlc_object_alive(s) || i_buffered >= STREAM_CACHE_PREBUFFER_SIZE )
        {
            int64_t i_byterate;

            /* Update stat */
            p_sys->stat.i_bytes = i_buffered;
            p_sys->stat.i_read_time = i_date - i_start;
            i_byterate = ( INT64_C(1000000) * p_sys->stat.i_bytes ) /
                         (p_sys->stat.i_read_time+1);

            msg_Dbg( s, "pre-buffering done %"PRId64" bytes in %"PRId64"s - "
                     "%"PRId64" KiB/s",
                     p_sys->stat.i_bytes,
                     p_sys->stat.i_read_time / INT64_C(1000000),
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
 * stream_ReadLine:
 ****************************************************************************/
/**
 * Read from the stream untill first newline.
 * \param s Stream handle to read from
 * \return A pointer to the allocated output string. You need to free this when you are done.
 */
#define STREAM_PROBE_LINE 2048
#define STREAM_LINE_MAX (2048*100)
char *stream_ReadLine( stream_t *s )
{
    char *p_line = NULL;
    int i_line = 0, i_read = 0;

    for( ;; )
    {
        char *psz_eol;
        const uint8_t *p_data;
        int i_data;
        int64_t i_pos;

        /* Probe new data */
        i_data = stream_Peek( s, &p_data, STREAM_PROBE_LINE );
        if( i_data <= 0 ) break; /* No more data */

        /* BOM detection */
        i_pos = stream_Tell( s );
        if( i_pos == 0 && i_data >= 2 )
        {
            const char *psz_encoding = NULL;

            if( !memcmp( p_data, "\xFF\xFE", 2 ) )
            {
                psz_encoding = "UTF-16LE";
                s->p_text->b_little_endian = true;
            }
            else if( !memcmp( p_data, "\xFE\xFF", 2 ) )
            {
                psz_encoding = "UTF-16BE";
            }

            /* Open the converter if we need it */
            if( psz_encoding != NULL )
            {
                msg_Dbg( s, "UTF-16 BOM detected" );
                s->p_text->i_char_width = 2;
                s->p_text->conv = vlc_iconv_open( "UTF-8", psz_encoding );
                if( s->p_text->conv == (vlc_iconv_t)-1 )
                    msg_Err( s, "iconv_open failed" );
            }
        }

        if( i_data % s->p_text->i_char_width )
        {
            /* keep i_char_width boundary */
            i_data = i_data - ( i_data % s->p_text->i_char_width );
            msg_Warn( s, "the read is not i_char_width compatible");
        }

        if( i_data == 0 )
            break;

        /* Check if there is an EOL */
        if( s->p_text->i_char_width == 1 )
        {
            /* UTF-8: 0A <LF> */
            psz_eol = memchr( p_data, '\n', i_data );
            if( psz_eol == NULL )
                /* UTF-8: 0D <CR> */
                psz_eol = memchr( p_data, '\r', i_data );
        }
        else
        {
            const uint8_t *p_last = p_data + i_data - s->p_text->i_char_width;
            uint16_t eol = s->p_text->b_little_endian ? 0x0A00 : 0x00A0;

            assert( s->p_text->i_char_width == 2 );
            psz_eol = NULL;
            /* UTF-16: 000A <LF> */
            for( const uint8_t *p = p_data; p <= p_last; p += 2 )
            {
                if( U16_AT( p ) == eol )
                {
                     psz_eol = (char *)p + 1;
                     break;
                }
            }

            if( psz_eol == NULL )
            {   /* UTF-16: 000D <CR> */
                eol = s->p_text->b_little_endian ? 0x0D00 : 0x00D0;
                for( const uint8_t *p = p_data; p <= p_last; p += 2 )
                {
                    if( U16_AT( p ) == eol )
                    {
                        psz_eol = (char *)p + 1;
                        break;
                    }
                }
            }
        }

        if( psz_eol )
        {
            i_data = (psz_eol - (char *)p_data) + 1;
            p_line = realloc_or_free( p_line,
                     i_line + i_data + s->p_text->i_char_width ); /* add \0 */
            if( !p_line )
                goto error;
            i_data = stream_Read( s, &p_line[i_line], i_data );
            if( i_data <= 0 ) break; /* Hmmm */
            i_line += i_data - s->p_text->i_char_width; /* skip \n */;
            i_read += i_data;

            /* We have our line */
            break;
        }

        /* Read data (+1 for easy \0 append) */
        p_line = realloc_or_free( p_line,
                       i_line + STREAM_PROBE_LINE + s->p_text->i_char_width );
        if( !p_line )
            goto error;
        i_data = stream_Read( s, &p_line[i_line], STREAM_PROBE_LINE );
        if( i_data <= 0 ) break; /* Hmmm */
        i_line += i_data;
        i_read += i_data;

        if( i_read >= STREAM_LINE_MAX )
            goto error; /* line too long */
    }

    if( i_read > 0 )
    {
        int j;
        for( j = 0; j < s->p_text->i_char_width; j++ )
        {
            p_line[i_line + j] = '\0';
        }
        i_line += s->p_text->i_char_width; /* the added \0 */
        if( s->p_text->i_char_width > 1 )
        {
            int i_new_line = 0;
            size_t i_in = 0, i_out = 0;
            const char * p_in = NULL;
            char * p_out = NULL;
            char * psz_new_line = NULL;

            /* iconv */
            /* UTF-8 needs at most 150% of the buffer as many as UTF-16 */
            i_new_line = i_line * 3 / 2;
            psz_new_line = malloc( i_new_line );
            if( psz_new_line == NULL )
                goto error;
            i_in = (size_t)i_line;
            i_out = (size_t)i_new_line;
            p_in = p_line;
            p_out = psz_new_line;

            if( vlc_iconv( s->p_text->conv, &p_in, &i_in, &p_out, &i_out ) == (size_t)-1 )
            {
                msg_Err( s, "iconv failed" );
                msg_Dbg( s, "original: %d, in %d, out %d", i_line, (int)i_in, (int)i_out );
            }
            free( p_line );
            p_line = psz_new_line;
            i_line = (size_t)i_new_line - i_out; /* does not include \0 */
        }

        /* Remove trailing LF/CR */
        while( i_line >= 2 && ( p_line[i_line-2] == '\r' ||
            p_line[i_line-2] == '\n') ) i_line--;

        /* Make sure the \0 is there */
        p_line[i_line-1] = '\0';

        return p_line;
    }

error:
    /* We failed to read any data, probably EOF */
    free( p_line );

    /* */
    if( s->p_text->conv != (vlc_iconv_t)(-1) )
        vlc_iconv_close( s->p_text->conv );
    s->p_text->conv = (vlc_iconv_t)(-1);
    return NULL;
}

/****************************************************************************
 * Access reading/seeking wrappers to handle concatenated streams.
 ****************************************************************************/
static int AReadStream( stream_t *s, void *p_read, unsigned int i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    access_t *p_access = p_sys->p_access;
    input_thread_t *p_input = s->p_input;
    int i_read_orig = i_read;

    if( !p_sys->i_list )
    {
        i_read = p_access->pf_read( p_access, p_read, i_read );
        if( p_input )
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

    i_read = p_sys->p_list_access->pf_read( p_sys->p_list_access, p_read,
                                            i_read );

    /* If we reached an EOF then switch to the next stream in the list */
    if( i_read == 0 && p_sys->i_list_index + 1 < p_sys->i_list )
    {
        char *psz_name = p_sys->list[++p_sys->i_list_index]->psz_path;
        access_t *p_list_access;

        msg_Dbg( s, "opening input `%s'", psz_name );

        p_list_access = access_New( s, s->p_input, p_access->psz_access, "", psz_name );

        if( !p_list_access ) return 0;

        if( p_sys->p_list_access != p_access )
            access_Delete( p_sys->p_list_access );

        p_sys->p_list_access = p_list_access;

        /* We have to read some data */
        return AReadStream( s, p_read, i_read_orig );
    }

    /* Update read bytes in input */
    if( p_input )
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
    block_t *p_block;
    bool b_eof;

    if( !p_sys->i_list )
    {
        p_block = p_access->pf_block( p_access );
        if( pb_eof ) *pb_eof = p_access->info.b_eof;
        if( p_input && p_block && libvlc_stats (p_access) )
        {
            uint64_t total;

            vlc_mutex_lock( &p_input->p->counters.counters_lock );
            stats_Update( p_input->p->counters.p_read_bytes,
                          p_block->i_buffer, &total );
            stats_Update( p_input->p->counters.p_input_bitrate,
                          total, NULL );
            stats_Update( p_input->p->counters.p_read_packets, 1, NULL );
            vlc_mutex_unlock( &p_input->p->counters.counters_lock );
        }
        return p_block;
    }

    p_block = p_sys->p_list_access->pf_block( p_sys->p_list_access );
    b_eof = p_sys->p_list_access->info.b_eof;
    if( pb_eof ) *pb_eof = b_eof;

    /* If we reached an EOF then switch to the next stream in the list */
    if( !p_block && b_eof && p_sys->i_list_index + 1 < p_sys->i_list )
    {
        char *psz_name = p_sys->list[++p_sys->i_list_index]->psz_path;
        access_t *p_list_access;

        msg_Dbg( s, "opening input `%s'", psz_name );

        p_list_access = access_New( s, s->p_input, p_access->psz_access, "", psz_name );

        if( !p_list_access ) return 0;

        if( p_sys->p_list_access != p_access )
            access_Delete( p_sys->p_list_access );

        p_sys->p_list_access = p_list_access;

        /* We have to read some data */
        return AReadBlock( s, pb_eof );
    }
    if( p_block )
    {
        if( p_input )
        {
            uint64_t total;

            vlc_mutex_lock( &p_input->p->counters.counters_lock );
            stats_Update( p_input->p->counters.p_read_bytes,
                          p_block->i_buffer, &total );
            stats_Update( p_input->p->counters.p_input_bitrate, total, NULL );
            stats_Update( p_input->p->counters.p_read_packets, 1 , NULL);
            vlc_mutex_unlock( &p_input->p->counters.counters_lock );
        }
    }
    return p_block;
}

static int ASeek( stream_t *s, uint64_t i_pos )
{
    stream_sys_t *p_sys = s->p_sys;
    access_t *p_access = p_sys->p_access;

    /* Check which stream we need to access */
    if( p_sys->i_list )
    {
        int i;
        char *psz_name;
        int64_t i_size = 0;
        access_t *p_list_access = 0;

        for( i = 0; i < p_sys->i_list - 1; i++ )
        {
            if( i_pos < p_sys->list[i]->i_size + i_size ) break;
            i_size += p_sys->list[i]->i_size;
        }
        psz_name = p_sys->list[i]->psz_path;

        if( i != p_sys->i_list_index )
            msg_Dbg( s, "opening input `%s'", psz_name );

        if( i != p_sys->i_list_index && i != 0 )
        {
            p_list_access =
                access_New( s, s->p_input, p_access->psz_access, "", psz_name );
        }
        else if( i != p_sys->i_list_index )
        {
            p_list_access = p_access;
        }

        if( p_list_access )
        {
            if( p_sys->p_list_access != p_access )
                access_Delete( p_sys->p_list_access );

            p_sys->p_list_access = p_list_access;
        }

        p_sys->i_list_index = i;
        return p_sys->p_list_access->pf_seek( p_sys->p_list_access,
                                              i_pos - i_size );
    }

    return p_access->pf_seek( p_access, i_pos );
}


/**
 * Try to read "i_read" bytes into a buffer pointed by "p_read".  If
 * "p_read" is NULL then data are skipped instead of read.
 * \return The real number of bytes read/skip. If this value is less
 * than i_read that means that it's the end of the stream.
 * \note stream_Read increments the stream position, and when p_read is NULL,
 * this is its only task.
 */
int stream_Read( stream_t *s, void *p_read, int i_read )
{
    return s->pf_read( s, p_read, i_read );
}

/**
 * Store in pp_peek a pointer to the next "i_peek" bytes in the stream
 * \return The real number of valid bytes. If it's less
 * or equal to 0, *pp_peek is invalid.
 * \note pp_peek is a pointer to internal buffer and it will be invalid as
 * soons as other stream_* functions are called.
 * \note Contrary to stream_Read, stream_Peek doesn't modify the stream
 * position, and doesn't necessarily involve copying of data. It's mainly
 * used by the modules to quickly probe the (head of the) stream.
 * \note Due to input limitation, the return value could be less than i_peek
 * without meaning the end of the stream (but only when you have i_peek >=
 * p_input->i_bufsize)
 */
int stream_Peek( stream_t *s, const uint8_t **pp_peek, int i_peek )
{
    return s->pf_peek( s, pp_peek, i_peek );
}

/**
 * Use to control the "stream_t *". Look at #stream_query_e for
 * possible "i_query" value and format arguments.  Return VLC_SUCCESS
 * if ... succeed ;) and VLC_EGENERIC if failed or unimplemented
 */
int stream_vaControl( stream_t *s, int i_query, va_list args )
{
    return s->pf_control( s, i_query, args );
}

/**
 * Destroy a stream
 */
void stream_Delete( stream_t *s )
{
    s->pf_destroy( s );
}

int stream_Control( stream_t *s, int i_query, ... )
{
    va_list args;
    int     i_result;

    if( s == NULL )
        return VLC_EGENERIC;

    va_start( args, i_query );
    i_result = s->pf_control( s, i_query, args );
    va_end( args );
    return i_result;
}

/**
 * Read "i_size" bytes and store them in a block_t.
 * It always read i_size bytes unless you are at the end of the stream
 * where it return what is available.
 */
block_t *stream_Block( stream_t *s, int i_size )
{
    if( i_size <= 0 ) return NULL;

    /* emulate block read */
    block_t *p_bk = block_Alloc( i_size );
    if( p_bk )
    {
        int i_read = stream_Read( s, p_bk->p_buffer, i_size );
        if( i_read > 0 )
        {
            p_bk->i_buffer = i_read;
            return p_bk;
        }
        block_Release( p_bk );
    }
    return NULL;
}

/**
 * Read the remaining of the data if there is less than i_max_size bytes, otherwise
 * return NULL.
 *
 * The stream position is unknown after the call.
 */
block_t *stream_BlockRemaining( stream_t *s, int i_max_size )
{
    int     i_allocate = __MIN(1000000, i_max_size);
    int64_t i_size = stream_Size( s );
    if( i_size > 0 )
    {
        int64_t i_position = stream_Tell( s );
        if( i_position + i_max_size < i_size )
        {
            msg_Err( s, "Remaining stream size is greater than %d bytes",
                     i_max_size );
            return NULL;
        }
        i_allocate = i_size - i_position;
    }
    if( i_allocate <= 0 )
        return NULL;

    block_t *p_block = block_Alloc( i_allocate );
    int i_index = 0;
    while( p_block )
    {
        int i_read = stream_Read( s, &p_block->p_buffer[i_index],
                                     p_block->i_buffer - i_index);
        if( i_read <= 0 )
            break;
        i_index += i_read;
        i_max_size -= i_read;
        if( i_max_size <= 0 )
            break;
        p_block = block_Realloc( p_block, 0, p_block->i_buffer +
                                             __MIN(1000000, i_max_size) );
    }
    if( p_block )
        p_block->i_buffer = i_index;
    return p_block;
}

