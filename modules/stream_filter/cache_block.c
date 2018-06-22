/*****************************************************************************
 * cache_block.c
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
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <vlc_interrupt.h>

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

struct stream_sys_t
{
    uint64_t     i_pos;      /* Current reading offset */

    uint64_t     i_start;        /* Offset of block for p_first */
    uint64_t     i_offset;       /* Offset for data in p_current */
    block_t     *p_current;     /* Current block */

    uint64_t     i_size;         /* Total amount of data in the list */
    block_t     *p_first;
    block_t    **pp_last;

    struct
    {
        /* Stat about reading data */
        uint64_t i_read_count;
        uint64_t i_bytes;
        uint64_t i_read_time;
    } stat;
};

static int AStreamRefillBlock(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;

    /* Release data */
    while (sys->i_size >= STREAM_CACHE_SIZE &&
           sys->p_first != sys->p_current)
    {
        block_t *b = sys->p_first;

        sys->i_start += b->i_buffer;
        sys->i_size  -= b->i_buffer;
        sys->p_first  = b->p_next;

        block_Release(b);
    }
    if (sys->i_size >= STREAM_CACHE_SIZE &&
        sys->p_current == sys->p_first &&
        sys->p_current->p_next)    /* At least 2 packets */
    {
        /* Enough data, don't read more */
        return VLC_SUCCESS;
    }

    /* Now read a new block */
    const vlc_tick_t start = mdate();
    block_t *b;

    for (;;)
    {
        if (vlc_killed())
            return VLC_EGENERIC;

        /* Fetch a block */
        if ((b = vlc_stream_ReadBlock(s->p_source)))
            break;
        if (vlc_stream_Eof(s->p_source))
            return VLC_EGENERIC;
    }

    sys->stat.i_read_time += mdate() - start;
    while (b)
    {
        /* Append the block */
        sys->i_size += b->i_buffer;
        *sys->pp_last = b;
        sys->pp_last = &b->p_next;

        /* Fix p_current */
        if (sys->p_current == NULL)
            sys->p_current = b;

        /* Update stat */
        sys->stat.i_bytes += b->i_buffer;
        sys->stat.i_read_count++;

        b = b->p_next;
    }
    return VLC_SUCCESS;
}

static void AStreamPrebufferBlock(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;
    vlc_tick_t start = mdate();
    bool first = true;

    msg_Dbg(s, "starting pre-buffering");
    for (;;)
    {
        const int64_t now = mdate();

        if (vlc_killed() || sys->i_size > STREAM_CACHE_PREBUFFER_SIZE)
        {
            int64_t i_byterate;

            /* Update stat */
            sys->stat.i_bytes = sys->i_size;
            sys->stat.i_read_time = now - start;
            i_byterate = (CLOCK_FREQ * sys->stat.i_bytes) /
                         (sys->stat.i_read_time + 1);

            msg_Dbg(s, "prebuffering done %"PRId64" bytes in %"PRId64"s - "
                     "%"PRId64" KiB/s",
                     sys->stat.i_bytes,
                     sys->stat.i_read_time / CLOCK_FREQ,
                     i_byterate / 1024);
            break;
        }

        /* Fetch a block */
        block_t *b = vlc_stream_ReadBlock(s->p_source);
        if (b == NULL)
        {
            if (vlc_stream_Eof(s->p_source))
                break;
            continue;
        }

        while (b)
        {
            /* Append the block */
            sys->i_size += b->i_buffer;
            *sys->pp_last = b;
            sys->pp_last = &b->p_next;

            sys->stat.i_read_count++;
            b = b->p_next;
        }

        if (first)
        {
            msg_Dbg(s, "received first data after %"PRId64" ms",
                    (mdate() - start) / 1000);
            first = false;
        }
    }

    sys->p_current = sys->p_first;
}

/****************************************************************************
 * AStreamControlReset:
 ****************************************************************************/
static void AStreamControlReset(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;

    sys->i_pos = 0;

    block_ChainRelease(sys->p_first);

    /* Init all fields of sys->block */
    sys->i_start = 0;
    sys->i_offset = 0;
    sys->p_current = NULL;
    sys->i_size = 0;
    sys->p_first = NULL;
    sys->pp_last = &sys->p_first;

    /* Do the prebuffering */
    AStreamPrebufferBlock(s);
}

static int AStreamSeekBlock(stream_t *s, uint64_t i_pos)
{
    stream_sys_t *sys = s->p_sys;
    int64_t    i_offset = i_pos - sys->i_start;
    bool b_seek;

    /* We already have thoses data, just update p_current/i_offset */
    if (i_offset >= 0 && (uint64_t)i_offset < sys->i_size)
    {
        block_t *b = sys->p_first;
        int i_current = 0;

        while (i_current + b->i_buffer < (uint64_t)i_offset)
        {
            i_current += b->i_buffer;
            b = b->p_next;
        }

        sys->p_current = b;
        sys->i_offset = i_offset - i_current;

        sys->i_pos = i_pos;

        return VLC_SUCCESS;
    }

    /* We may need to seek or to read data */
    if (i_offset < 0)
    {
        bool b_aseek;
        vlc_stream_Control(s->p_source, STREAM_CAN_SEEK, &b_aseek);

        if (!b_aseek)
        {
            msg_Err(s, "backward seeking impossible (access not seekable)");
            return VLC_EGENERIC;
        }

        b_seek = true;
    }
    else
    {
        bool b_aseek, b_aseekfast;

        vlc_stream_Control(s->p_source, STREAM_CAN_SEEK, &b_aseek);
        vlc_stream_Control(s->p_source, STREAM_CAN_FASTSEEK, &b_aseekfast);

        if (!b_aseek)
        {
            b_seek = false;
            msg_Warn(s, "%"PRId64" bytes need to be skipped "
                      "(access non seekable)", i_offset - sys->i_size);
        }
        else
        {
            int64_t i_skip = i_offset - sys->i_size;

            /* Avg bytes per packets */
            int i_avg = sys->stat.i_bytes / sys->stat.i_read_count;
            /* TODO compute a seek cost instead of fixed threshold */
            int i_th = b_aseekfast ? 1 : 5;

            if (i_skip <= i_th * i_avg &&
                i_skip < STREAM_CACHE_SIZE)
                b_seek = false;
            else
                b_seek = true;

            msg_Dbg(s, "b_seek=%d th*avg=%d skip=%"PRId64,
                     b_seek, i_th*i_avg, i_skip);
        }
    }

    if (b_seek)
    {
        /* Do the access seek */
        if (vlc_stream_Seek(s->p_source, i_pos)) return VLC_EGENERIC;

        /* Release data */
        block_ChainRelease(sys->p_first);

        /* Reinit */
        sys->i_start = sys->i_pos = i_pos;
        sys->i_offset = 0;
        sys->p_current = NULL;
        sys->i_size = 0;
        sys->p_first = NULL;
        sys->pp_last = &sys->p_first;

        /* Refill a block */
        if (AStreamRefillBlock(s))
            return VLC_EGENERIC;

        return VLC_SUCCESS;
    }
    else
    {
        do
        {
            while (sys->p_current &&
                   sys->i_pos + sys->p_current->i_buffer - sys->i_offset <= i_pos)
            {
                sys->i_pos += sys->p_current->i_buffer - sys->i_offset;
                sys->p_current = sys->p_current->p_next;
                sys->i_offset = 0;
            }
            if (!sys->p_current && AStreamRefillBlock(s))
            {
                if (sys->i_pos != i_pos)
                    return VLC_EGENERIC;
            }
        }
        while (sys->i_start + sys->i_size < i_pos);

        sys->i_offset += i_pos - sys->i_pos;
        sys->i_pos = i_pos;

        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

static ssize_t AStreamReadBlock(stream_t *s, void *buf, size_t len)
{
    stream_sys_t *sys = s->p_sys;

    /* It means EOF */
    if (sys->p_current == NULL)
        return 0;

    ssize_t i_current = sys->p_current->i_buffer - sys->i_offset;
    size_t i_copy = VLC_CLIP((size_t)i_current, 0, len);

    /* Copy data */
    memcpy(buf, &sys->p_current->p_buffer[sys->i_offset], i_copy);

    sys->i_offset += i_copy;
    if (sys->i_offset >= sys->p_current->i_buffer)
    {   /* Current block is now empty, switch to next */
        sys->i_offset = 0;
        sys->p_current = sys->p_current->p_next;

        /* Get a new block if needed */
        if (sys->p_current == NULL)
            AStreamRefillBlock(s);
    }

    /**
     * we should not signal end-of-file if we have not exhausted
     * the blocks we know about, as such we should try again if that
     * is the case. i_copy == 0 just means that the processed block does
     * not contain data at the offset that we want, not EOF.
     **/

    if( i_copy == 0 && sys->p_current )
        return AStreamReadBlock( s, buf, len );

    sys->i_pos += i_copy;
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
        case STREAM_IS_DIRECTORY:
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
            return vlc_stream_vaControl(s->p_source, i_query, args);

        case STREAM_SET_TITLE:
        case STREAM_SET_SEEKPOINT:
        {
            int ret = vlc_stream_vaControl(s->p_source, i_query, args);
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

    stream_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    /* Common field */
    sys->i_pos = 0;

    /* Stats */
    sys->stat.i_bytes = 0;
    sys->stat.i_read_time = 0;
    sys->stat.i_read_count = 0;

    msg_Dbg(s, "Using block method for AStream*");

    /* Init all fields of sys->block */
    sys->i_start = sys->i_pos;
    sys->i_offset = 0;
    sys->p_current = NULL;
    sys->i_size = 0;
    sys->p_first = NULL;
    sys->pp_last = &sys->p_first;

    s->p_sys = sys;
    /* Do the prebuffering */
    AStreamPrebufferBlock(s);

    if (sys->i_size <= 0)
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

    block_ChainRelease(sys->p_first);
    free(sys);
}

vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_STREAM_FILTER)
    set_capability("stream_filter", 0)

    set_description(N_("Block stream cache"))
    set_callbacks(Open, Close)
vlc_module_end()
