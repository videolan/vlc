/*****************************************************************************
 * cache_read.c
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

// #define STREAM_DEBUG 1

/*
 * Complex scheme using mutliple track to avoid seeking
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

/* Method:
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
    vlc_tick_t date;

    uint64_t i_start;
    uint64_t i_end;

    uint8_t *p_buffer;

} stream_track_t;

typedef struct
{
    uint64_t     i_pos;      /* Current reading offset */

    unsigned     i_offset;   /* Buffer offset in the current track */
    int          i_tk;       /* Current track */
    stream_track_t tk[STREAM_CACHE_TRACK];

    /* Global buffer */
    uint8_t     *p_buffer;

    /* */
    unsigned     i_used; /* Used since last read */
    unsigned     i_read_size;

    struct
    {
        /* Stat about reading data */
        uint64_t i_read_count;
        uint64_t i_bytes;
        vlc_tick_t i_read_time;
    } stat;
} stream_sys_t;

static int AStreamRefillStream(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;
    stream_track_t *tk = &sys->tk[sys->i_tk];

    /* We read but won't increase i_start after initial start + offset */
    int i_toread =
        __MIN(sys->i_used, STREAM_CACHE_TRACK_SIZE -
               (tk->i_end - tk->i_start - sys->i_offset));

    if (i_toread <= 0) return VLC_SUCCESS; /* EOF */

#ifdef STREAM_DEBUG
    msg_Dbg(s, "AStreamRefillStream: used=%d toread=%d",
                 sys->i_used, i_toread);
#endif

    vlc_tick_t start = vlc_tick_now();
    while (i_toread > 0)
    {
        int i_off = tk->i_end % STREAM_CACHE_TRACK_SIZE;
        int i_read;

        if (vlc_killed())
            return VLC_EGENERIC;

        i_read = __MIN(i_toread, STREAM_CACHE_TRACK_SIZE - i_off);
        i_read = vlc_stream_Read(s->s, &tk->p_buffer[i_off], i_read);

        /* msg_Dbg(s, "AStreamRefillStream: read=%d", i_read); */
        if (i_read <  0)
        {
            continue;
        }
        else if (i_read == 0)
            return VLC_SUCCESS;

        /* Update end */
        tk->i_end += i_read;

        /* Windows of STREAM_CACHE_TRACK_SIZE */
        if (tk->i_start + STREAM_CACHE_TRACK_SIZE < tk->i_end)
        {
            unsigned i_invalid = tk->i_end - tk->i_start - STREAM_CACHE_TRACK_SIZE;

            tk->i_start += i_invalid;
            sys->i_offset -= i_invalid;
        }

        i_toread -= i_read;
        sys->i_used -= i_read;

        sys->stat.i_bytes += i_read;
        sys->stat.i_read_count++;
    }

    sys->stat.i_read_time += vlc_tick_now() - start;
    return VLC_SUCCESS;
}

static void AStreamPrebufferStream(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;
    vlc_tick_t start = vlc_tick_now();
    bool first = true;

    msg_Dbg(s, "starting pre-buffering");
    for (;;)
    {
        stream_track_t *tk = &sys->tk[sys->i_tk];
        vlc_tick_t now = vlc_tick_now();

        int i_read;
        int i_buffered = tk->i_end - tk->i_start;

        if (vlc_killed() || i_buffered >= STREAM_CACHE_PREBUFFER_SIZE)
        {
            int64_t i_byterate;

            /* Update stat */
            sys->stat.i_bytes = i_buffered;
            sys->stat.i_read_time = now - start;
            i_byterate = (CLOCK_FREQ * sys->stat.i_bytes) /
                         (sys->stat.i_read_time+1);

            msg_Dbg(s, "pre-buffering done %"PRId64" bytes in %"PRId64"s - "
                    "%"PRId64" KiB/s", sys->stat.i_bytes,
                    SEC_FROM_VLC_TICK(sys->stat.i_read_time), i_byterate / 1024);
            break;
        }

        i_read = STREAM_CACHE_TRACK_SIZE - i_buffered;
        i_read = __MIN((int)sys->i_read_size, i_read);
        i_read = vlc_stream_Read(s->s, &tk->p_buffer[i_buffered], i_read);
        if (i_read <  0)
            continue;
        else if (i_read == 0)
            break;  /* EOF */

        if (first)
        {
            msg_Dbg(s, "received first data after %"PRId64" ms",
                    MS_FROM_VLC_TICK(vlc_tick_now() - start));
            first = false;
        }

        tk->i_end += i_read;
        sys->stat.i_read_count++;
    }
}

/****************************************************************************
 * AStreamControlReset:
 ****************************************************************************/
static void AStreamControlReset(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;

    sys->i_pos = 0;

    /* Setup our tracks */
    sys->i_offset = 0;
    sys->i_tk     = 0;
    sys->i_used   = 0;

    for (unsigned i = 0; i < STREAM_CACHE_TRACK; i++)
    {
        sys->tk[i].date  = 0;
        sys->tk[i].i_start = sys->i_pos;
        sys->tk[i].i_end   = sys->i_pos;
    }

    /* Do the prebuffering */
    AStreamPrebufferStream(s);
}

static ssize_t AStreamReadStream(stream_t *s, void *buf, size_t len)
{
    stream_sys_t *sys = s->p_sys;
    stream_track_t *tk = &sys->tk[sys->i_tk];

    if (tk->i_start >= tk->i_end)
        return 0; /* EOF */

#ifdef STREAM_DEBUG
    msg_Dbg(s, "AStreamReadStream: %zd pos=%"PRId64" tk=%d start=%"PRId64
            " offset=%d end=%"PRId64, len, sys->i_pos, sys->i_tk,
            tk->i_start, sys->i_offset, tk->i_end);
#endif

    unsigned i_off = (tk->i_start + sys->i_offset) % STREAM_CACHE_TRACK_SIZE;
    size_t i_current = __MIN(tk->i_end - tk->i_start - sys->i_offset,
                             STREAM_CACHE_TRACK_SIZE - i_off);
    ssize_t i_copy = __MIN(i_current, len);
    if (i_copy <= 0)
        return 0; /* EOF */

    /* Copy data */
    /* msg_Dbg(s, "AStreamReadStream: copy %zd", i_copy); */
    if (buf != NULL)
        memcpy(buf, &tk->p_buffer[i_off], i_copy);
    sys->i_offset += i_copy;

    /* Update pos now */
    sys->i_pos += i_copy;

    /* */
    sys->i_used += i_copy;

    if (tk->i_end + i_copy <= tk->i_start + sys->i_offset + len)
    {
        const size_t i_read_requested = VLC_CLIP(len - i_copy,
                                                 STREAM_READ_ATONCE / 2,
                                                 STREAM_READ_ATONCE * 10);
        if (sys->i_used < i_read_requested)
            sys->i_used = i_read_requested;

        AStreamRefillStream(s);
    }

    return i_copy;
}

static int AStreamSeekStream(stream_t *s, uint64_t i_pos)
{
    stream_sys_t *sys = s->p_sys;
    stream_track_t *p_current = &sys->tk[sys->i_tk];

    if (p_current->i_start >= p_current->i_end  && i_pos >= p_current->i_end)
        return 0; /* EOF */

#ifdef STREAM_DEBUG
    msg_Dbg(s, "AStreamSeekStream: to %"PRId64" pos=%"PRId64
             " tk=%d start=%"PRId64" offset=%d end=%"PRId64,
             i_pos, sys->i_pos, sys->i_tk, p_current->i_start,
             sys->i_offset, p_current->i_end);
#endif

    bool   b_aseek;
    vlc_stream_Control(s->s, STREAM_CAN_SEEK, &b_aseek);
    if (!b_aseek && i_pos < p_current->i_start)
    {
        msg_Warn(s, "AStreamSeekStream: can't seek");
        return VLC_EGENERIC;
    }

    bool   b_afastseek;
    vlc_stream_Control(s->s, STREAM_CAN_FASTSEEK, &b_afastseek);

    /* FIXME compute seek cost (instead of static 'stupid' value) */
    uint64_t i_skip_threshold;
    if (b_aseek)
        i_skip_threshold = b_afastseek ? 128 : 3 * sys->i_read_size;
    else
        i_skip_threshold = INT64_MAX;

    /* Date the current track */
    p_current->date = vlc_tick_now();

    /* Search a new track slot */
    stream_track_t *tk = NULL;
    int i_tk_idx = -1;

    /* Prefer the current track */
    if (p_current->i_start <= i_pos && i_pos <= p_current->i_end + i_skip_threshold)
    {
        tk = p_current;
        i_tk_idx = sys->i_tk;
    }
    if (!tk)
    {
        /* Try to maximize already read data */
        for (int i = 0; i < STREAM_CACHE_TRACK; i++)
        {
            stream_track_t *t = &sys->tk[i];

            if (t->i_start > i_pos || i_pos > t->i_end)
                continue;

            if (!tk || tk->i_end < t->i_end)
            {
                tk = t;
                i_tk_idx = i;
            }
        }
    }
    if (!tk)
    {
        /* Use the oldest unused */
        for (int i = 0; i < STREAM_CACHE_TRACK; i++)
        {
            stream_track_t *t = &sys->tk[i];

            if (!tk || tk->date > t->date)
            {
                tk = t;
                i_tk_idx = i;
            }
        }
    }
    assert(i_tk_idx >= 0 && i_tk_idx < STREAM_CACHE_TRACK);

    if (tk != p_current)
        i_skip_threshold = 0;
    if (tk->i_start <= i_pos && i_pos <= tk->i_end + i_skip_threshold)
    {
#ifdef STREAM_DEBUG
        msg_Err(s, "AStreamSeekStream: reusing %d start=%"PRId64
                 " end=%"PRId64"(%s)",
                 i_tk_idx, tk->i_start, tk->i_end,
                 tk != p_current ? "seek" : i_pos > tk->i_end ? "skip" : "noseek");
#endif
        if (tk != p_current)
        {
            assert(b_aseek);

            /* Seek at the end of the buffer
             * TODO it is stupid to seek now, it would be better to delay it
             */
            if (vlc_stream_Seek(s->s, tk->i_end))
            {
                msg_Err(s, "AStreamSeekStream: hard seek failed");
                return VLC_EGENERIC;
            }
        }
        else if (i_pos > tk->i_end)
        {
            uint64_t i_skip = i_pos - tk->i_end;
            while (i_skip > 0)
            {
                const int i_read_max = __MIN(10 * STREAM_READ_ATONCE, i_skip);
                int i_read = 0;
                if ((i_read = AStreamReadStream(s, NULL, i_read_max)) < 0)
                {
                    msg_Err(s, "AStreamSeekStream: skip failed");
                    return VLC_EGENERIC;
                } else if (i_read == 0)
                    return VLC_SUCCESS; /* EOF */
                i_skip -= i_read_max;
            }
        }
    }
    else
    {
#ifdef STREAM_DEBUG
        msg_Err(s, "AStreamSeekStream: hard seek");
#endif
        /* Nothing good, seek and choose oldest segment */
        if (vlc_stream_Seek(s->s, i_pos))
        {
            msg_Err(s, "AStreamSeekStream: hard seek failed");
            return VLC_EGENERIC;
        }

        tk->i_start = i_pos;
        tk->i_end   = i_pos;
    }
    sys->i_offset = i_pos - tk->i_start;
    sys->i_tk = i_tk_idx;
    sys->i_pos = i_pos;

    /* If there is not enough data left in the track, refill  */
    /* TODO How to get a correct value for
     *    - refilling threshold
     *    - how much to refill
     */
    if (tk->i_end < tk->i_start + sys->i_offset + sys->i_read_size)
    {
        if (sys->i_used < STREAM_READ_ATONCE / 2)
            sys->i_used = STREAM_READ_ATONCE / 2;

        if (AStreamRefillStream(s))
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
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

    if (s->s->pf_read == NULL)
        return VLC_EGENERIC;

    stream_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    /* Common field */
    sys->i_pos = 0;

    /* Stats */
    sys->stat.i_bytes = 0;
    sys->stat.i_read_time = 0;
    sys->stat.i_read_count = 0;

    msg_Dbg(s, "Using stream method for AStream*");

    /* Allocate/Setup our tracks */
    sys->i_offset = 0;
    sys->i_tk     = 0;
    sys->p_buffer = malloc(STREAM_CACHE_SIZE);
    if (sys->p_buffer == NULL)
    {
        free(sys);
        return VLC_ENOMEM;
    }

    sys->i_used   = 0;
    sys->i_read_size = STREAM_READ_ATONCE;
#if STREAM_READ_ATONCE < 256
#   error "Invalid STREAM_READ_ATONCE value"
#endif

    for (unsigned i = 0; i < STREAM_CACHE_TRACK; i++)
    {
        sys->tk[i].date  = 0;
        sys->tk[i].i_start = sys->i_pos;
        sys->tk[i].i_end   = sys->i_pos;
        sys->tk[i].p_buffer = &sys->p_buffer[i * STREAM_CACHE_TRACK_SIZE];
    }

    s->p_sys = sys;

    /* Do the prebuffering */
    AStreamPrebufferStream(s);

    if (sys->tk[sys->i_tk].i_end <= 0)
    {
        msg_Err(s, "cannot pre fill buffer");
        free(sys->p_buffer);
        free(sys);
        return VLC_EGENERIC;
    }

    s->pf_read = AStreamReadStream;
    s->pf_seek = AStreamSeekStream;
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

    free(sys->p_buffer);
    free(sys);
}

vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_STREAM_FILTER)
    set_capability("stream_filter", 0)
    add_shortcut("cache")

    set_description(N_("Byte stream cache"))
    set_callbacks(Open, Close)
vlc_module_end()
