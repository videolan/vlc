/*****************************************************************************
 * cache_block.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
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
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <vlc_interrupt.h>
#include <vlc_block_helper.h>

/* TODO:
 *  - tune the 2 methods (block/stream)
 *  - compute cost for seek
 *  - improve stream mode seeking with closest segments
 *  - ...
 */

/* 
 * One linked list of data read
 */

/* How many tracks we have, currently only used for stream mode */
#ifdef OPTIMIZE_MEMORY
    /* Max size of our cache 128KiB per stream */
#   define STREAM_CACHE_SIZE  (1024*128)
#else
    /* Max size of our cache 48MiB per stream */
#   define STREAM_CACHE_SIZE  (4*12*1024*1024)
#endif

/* How many data we try to prebuffer
 * XXX it should be small to avoid useless latency but big enough for
 * efficient demux probing */
#define STREAM_CACHE_PREBUFFER_SIZE (128)

/* Method: Simple, for pf_block.
 *  We get blocks and put them in the linked list.
 *  We release blocks once the total size is bigger than STREAM_CACHE_SIZE
 */

typedef struct
{
    block_bytestream_t cache; /* bytestream chain for storing cache */

    struct
    {
        /* Stats for calculating speed */
        uint64_t read_bytes;
        vlc_tick_t read_time;
    } stat;
} stream_sys_t;

static int AStreamRefillBlock(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;
    size_t cache_size = sys->cache.i_total;

    /* Release data */
    if (cache_size >= STREAM_CACHE_SIZE)
    {
        block_BytestreamFlush( &sys->cache );
        cache_size = sys->cache.i_total;
    }
    if (cache_size >= STREAM_CACHE_SIZE &&
        sys->cache.p_chain != *sys->cache.pp_last)
    {
        /* Enough data, don't read more */
        return VLC_SUCCESS;
    }

    /* Now read a new block */
    const vlc_tick_t start = vlc_tick_now();
    block_t *b;

    for (;;)
    {
        if (vlc_killed())
            return VLC_EGENERIC;

        /* Fetch a block */
        if ((b = vlc_stream_ReadBlock(s->s)))
            break;
        if (vlc_stream_Eof(s->s))
            return VLC_EGENERIC;
    }
    sys->stat.read_time += vlc_tick_now() - start;
    size_t added_bytes;
    block_ChainProperties( b, NULL, &added_bytes, NULL );
    sys->stat.read_bytes += added_bytes;

    block_BytestreamPush( &sys->cache, b );
    return VLC_SUCCESS;
}

static void AStreamPrebufferBlock(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;
    vlc_tick_t start = vlc_tick_now();
    bool first = true;

    msg_Dbg(s, "starting pre-buffering");
    for (;;)
    {
        const vlc_tick_t now = vlc_tick_now();
        size_t cache_size = block_BytestreamRemaining( &sys->cache );

        if (vlc_killed() || cache_size > STREAM_CACHE_PREBUFFER_SIZE)
        {
            int64_t byterate;

            /* Update stat */
            sys->stat.read_bytes = cache_size;
            sys->stat.read_time = now - start;
            byterate = (CLOCK_FREQ * sys->stat.read_bytes ) /
                        (sys->stat.read_time -1);

            msg_Dbg(s, "prebuffering done %zu bytes "
                    "in %"PRIu64"s - %"PRIu64"u KiB/s", cache_size,
                    SEC_FROM_VLC_TICK(sys->stat.read_time), byterate / 1024 );
            break;
        }

        /* Fetch a block */
        block_t *b = vlc_stream_ReadBlock(s->s);
        if (b == NULL)
        {
            if (vlc_stream_Eof(s->s))
                break;
            continue;
        }

        block_BytestreamPush( &sys->cache, b);

        if (first)
        {
            msg_Dbg(s, "received first data after %"PRId64" ms",
                    MS_FROM_VLC_TICK(vlc_tick_now() - start));
            first = false;
        }
    }
}

/****************************************************************************
 * AStreamControlReset:
 ****************************************************************************/
static void AStreamControlReset(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;

    block_BytestreamEmpty( &sys->cache );

    /* Do the prebuffering */
    AStreamPrebufferBlock(s);
}

static int AStreamSeekBlock(stream_t *s, uint64_t i_pos)
{
    stream_sys_t *sys = s->p_sys;

    if( block_SkipBytes( &sys->cache, i_pos) == VLC_SUCCESS )
        return VLC_SUCCESS;

    /* Not enought bytes, empty and seek */
    /* Do the access seek */
    if (vlc_stream_Seek(s->s, i_pos)) return VLC_EGENERIC;

    block_BytestreamEmpty( &sys->cache );

    /* Refill a block */
    if (AStreamRefillBlock(s))
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static ssize_t AStreamReadBlock(stream_t *s, void *buf, size_t len)
{
    stream_sys_t *sys = s->p_sys;

    ssize_t i_current = block_BytestreamRemaining( &sys->cache );
    size_t i_copy = VLC_CLIP((size_t)i_current, 0, len);

    /**
     * we should not signal end-of-file if we have not exhausted
     * the cache. i_copy == 0 just means that the cache currently does
     * not contain data at the offset that we want, not EOF.
     **/
    if( i_copy == 0 )
    {
        /* Return EOF if we are unable to refill cache, most likely
         * really EOF */
        if( AStreamRefillBlock(s) == VLC_EGENERIC )
            return 0;
    }

    /* Copy data */
    if( block_GetBytes( &sys->cache, buf, i_copy ) )
        return -1;


    /* If we ended up on refill, try to read refilled cache */
    if( i_copy == 0 && sys->cache.p_chain )
        return AStreamReadBlock( s, buf, len );

    return i_copy;
}

/****************************************************************************
 * AStreamControl:
 ****************************************************************************/
static int AStreamControl(stream_t *s, int i_query, va_list args)
{
    switch(i_query)
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
        case STREAM_GET_TAGS:
        case STREAM_SET_PAUSE_STATE:
        case STREAM_SET_PRIVATE_ID_STATE:
        case STREAM_SET_PRIVATE_ID_CA:
        case STREAM_GET_PRIVATE_ID_STATE:
            return vlc_stream_vaControl(s->s, i_query, args);

        case STREAM_SET_TITLE:
        case STREAM_SET_SEEKPOINT:
        {
            int ret = vlc_stream_vaControl(s->s, i_query, args);
            if (ret == VLC_SUCCESS)
                AStreamControlReset(s);
            return ret;
        }

        case STREAM_SET_RECORD_STATE:
        default:
            msg_Err(s, "invalid vlc_stream_vaControl query=0x%x", i_query);
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int Open(vlc_object_t *obj)
{
    stream_t *s = (stream_t *)obj;

    if (s->s->pf_block == NULL)
        return VLC_EGENERIC;

    stream_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    msg_Dbg(s, "Using block method for AStream*");

    /* Init all fields of sys->block */
    block_BytestreamInit( &sys->cache );

    s->p_sys = sys;
    /* Do the prebuffering */
    AStreamPrebufferBlock(s);

    if (block_BytestreamRemaining( &sys->cache ) <= 0)
    {
        msg_Err(s, "cannot pre fill buffer");
        free(sys);
        return VLC_EGENERIC;
    }

    s->pf_read = AStreamReadBlock;
    s->pf_seek = AStreamSeekBlock;
    s->pf_control = AStreamControl;
    return VLC_SUCCESS;
}

/****************************************************************************
 * AStreamDestroy:
 ****************************************************************************/
static void Close(vlc_object_t *obj)
{
    stream_t *s = (stream_t *)obj;
    stream_sys_t *sys = s->p_sys;

    block_BytestreamEmpty( &sys->cache );
    free(sys);
}

vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_STREAM_FILTER)
    set_capability("stream_filter", 0)
    add_shortcut("cache")

    set_description(N_("Block stream cache"))
    set_callbacks(Open, Close)
vlc_module_end()
