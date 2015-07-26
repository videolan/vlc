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
#include <vlc_interrupt.h>

#include <libvlc.h>
#include "access.h"
#include "stream.h"
#include "input_internal.h"

struct stream_sys_t
{
    access_t *access;
    block_t  *block;
};

/* Block access */
static ssize_t AStreamReadBlock(stream_t *s, void *buf, size_t len)
{
    stream_sys_t *sys = s->p_sys;
    input_thread_t *input = s->p_input;
    block_t *block = sys->block;

    while (block == NULL)
    {
        if (vlc_access_Eof(sys->access))
            return 0;
        if (vlc_killed())
            return -1;

        block = vlc_access_Block(sys->access);
    }

    if (input != NULL)
    {
        uint64_t total;

        vlc_mutex_lock(&input->p->counters.counters_lock);
        stats_Update(input->p->counters.p_read_bytes, block->i_buffer, &total);
        stats_Update(input->p->counters.p_input_bitrate, total, NULL);
        stats_Update(input->p->counters.p_read_packets, 1, NULL);
        vlc_mutex_unlock(&input->p->counters.counters_lock);
    }

    size_t copy = block->i_buffer < len ? block->i_buffer : len;

    if (likely(copy > 0) && buf != NULL /* skipping data? */)
        memcpy(buf, block->p_buffer, copy);

    block->p_buffer += copy;
    block->i_buffer -= copy;

    if (block->i_buffer == 0)
        block_Release(block);
    else
        sys->block = block;

    return copy;
}

/* Read access */
static ssize_t AStreamReadStream(stream_t *s, void *buf, size_t len)
{
    stream_sys_t *sys = s->p_sys;
    input_thread_t *input = s->p_input;
    ssize_t val = 0;

    do
    {
        if (vlc_access_Eof(sys->access))
            return 0;
        if (vlc_killed())
            return -1;

        val = vlc_access_Read(sys->access, buf, len);
        if (val == 0)
            return 0; /* EOF */
    }
    while (val < 0);

    if (input != NULL)
    {
        uint64_t total;

        vlc_mutex_lock(&input->p->counters.counters_lock);
        stats_Update(input->p->counters.p_read_bytes, val, &total);
        stats_Update(input->p->counters.p_input_bitrate, total, NULL);
        stats_Update(input->p->counters.p_read_packets, 1, NULL);
        vlc_mutex_unlock(&input->p->counters.counters_lock);
    }

    return val;
}

/* Directory */
static input_item_t *AStreamReadDir(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;

    if (sys->access->pf_readdir == NULL)
        return NULL;
    return sys->access->pf_readdir(sys->access);
}

/* Common */
#define static_control_match(foo) \
    static_assert((unsigned) STREAM_##foo == ACCESS_##foo, "Mismatch")

static int AStreamControl(stream_t *s, int cmd, va_list args)
{
    stream_sys_t *sys = s->p_sys;
    access_t *access = sys->access;

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

    switch (cmd)
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
        case STREAM_SET_TITLE:
        case STREAM_SET_SEEKPOINT:
        case STREAM_SET_PRIVATE_ID_STATE:
        case STREAM_SET_PRIVATE_ID_CA:
        case STREAM_GET_PRIVATE_ID_STATE:
            return access_vaControl(access, cmd, args);

        case STREAM_IS_DIRECTORY:
        {
            bool *b;

            *va_arg(args, bool *) = access->pf_readdir != NULL;
            b = va_arg(args, bool *);
            if (b != NULL)
                *b = access->info.b_dir_sorted;
            b = va_arg(args, bool *);
            if (b != NULL)
                *b = access->info.b_dir_can_loop;
            break;
        }

        case STREAM_GET_POSITION:
        {
            uint64_t *ppos =va_arg(args, uint64_t *);

            *ppos = access->info.i_pos;
            if (sys->block != NULL)
            {
                assert(sys->block->i_buffer <= *ppos);
                *ppos -= sys->block->i_buffer;
            }
            break;
        }

        case STREAM_SET_POSITION:
        {
            uint64_t pos = va_arg(args, uint64_t);

            if (sys->block != NULL)
            {
                block_Release(sys->block);
                sys->block = NULL;
            }
            return vlc_access_Seek(sys->access, pos);
        }

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void AStreamDestroy(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;

    stream_CommonDelete(s);
    if (sys->block != NULL)
        block_Release(sys->block);
    vlc_access_Delete(sys->access);
    free(sys);
}

stream_t *stream_AccessNew(vlc_object_t *parent, input_thread_t *input,
                           const char *url)
{
    stream_t *s = stream_CommonNew(parent);
    if (unlikely(s == NULL))
        return NULL;

    s->p_input = input;
    s->psz_url = strdup(url);

    stream_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(s->psz_url == NULL || sys == NULL))
        goto error;

    sys->access = access_New(VLC_OBJECT(s), input, url);
    if (sys->access == NULL)
        goto error;

    sys->block = NULL;

    s->p_sys      = sys;
    if (sys->access->pf_block != NULL)
        s->pf_read = AStreamReadBlock;
    else
        s->pf_read = AStreamReadStream;
    s->pf_readdir = AStreamReadDir;
    s->pf_control = AStreamControl;
    s->pf_destroy = AStreamDestroy;

    return s;
error:
    free(sys);
    stream_CommonDelete(s);
    return NULL;
}
