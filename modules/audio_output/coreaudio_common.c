/*****************************************************************************
 * coreaudio_common.c: Common AudioUnit code for iOS and macOS
 *****************************************************************************
 * Copyright (C) 2005 - 2017 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

#import "coreaudio_common.h"

static inline uint64_t
BytesToFrames(struct aout_sys_common *p_sys, size_t i_bytes)
{
    return i_bytes * p_sys->i_frame_length / p_sys->i_bytes_per_frame;
}

static inline mtime_t
FramesToUs(struct aout_sys_common *p_sys, uint64_t i_nb_frames)
{
    return i_nb_frames * CLOCK_FREQ / p_sys->i_rate;
}

/* Called from render callbacks. No lock, wait, and IO here */
void
ca_Render(audio_output_t *p_aout, uint8_t *p_output, size_t i_requested)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    /* Pull audio from buffer */
    int32_t i_available;
    void *p_data = TPCircularBufferTail(&p_sys->circular_buffer,
                                        &i_available);
    if (i_available < 0)
        i_available = 0;

    size_t i_tocopy = __MIN(i_requested, (size_t) i_available);

    if (i_tocopy > 0)
    {
        memcpy(p_output, p_data, i_tocopy);
        TPCircularBufferConsume(&p_sys->circular_buffer, i_tocopy);
    }

    /* Pad with 0 */
    if (i_requested > i_tocopy)
    {
        atomic_fetch_add(&p_sys->i_underrun_size, i_requested - i_tocopy);
        memset(&p_output[i_tocopy], 0, i_requested - i_tocopy);
    }
}

int
ca_TimeGet(audio_output_t *p_aout, mtime_t *delay)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    int32_t i_bytes;
    TPCircularBufferTail(&p_sys->circular_buffer, &i_bytes);

    int64_t i_frames = BytesToFrames(p_sys, i_bytes) + p_sys->i_device_latency;
    *delay = FramesToUs(p_sys, i_frames);

    return 0;
}

void
ca_Flush(audio_output_t *p_aout, bool wait)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    if (wait)
    {
        int32_t i_bytes;

        while (TPCircularBufferTail(&p_sys->circular_buffer, &i_bytes) != NULL)
        {
            /* Calculate the duration of the circular buffer, in order to wait
             * for the render thread to play it all */
            const mtime_t i_frame_us =
                FramesToUs(p_sys, BytesToFrames(p_sys, i_bytes)) + 10000;

            /* Don't sleep less than 10ms */
            msleep(__MAX(i_frame_us, 10000));
        }
    }
    else
    {
        /* flush circular buffer if data is left */
        TPCircularBufferClear(&p_sys->circular_buffer);
    }
}

void
ca_Play(audio_output_t * p_aout, block_t * p_block)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    /* Do the channel reordering */
    if (p_sys->chans_to_reorder)
       aout_ChannelReorder(p_block->p_buffer, p_block->i_buffer,
                           p_sys->chans_to_reorder, p_sys->chan_table,
                           VLC_CODEC_FL32);

    /* move data to buffer */
    while (!TPCircularBufferProduceBytes(&p_sys->circular_buffer,
                                         p_block->p_buffer, p_block->i_buffer))
    {
        if (unlikely(p_block->i_buffer >
            (uint32_t) p_sys->circular_buffer.length))
        {
            msg_Err(p_aout, "the block is too big for the circular buffer");
            assert(false);
            break;
        }

        /* Wait for the render buffer to play the remaining data */
        int32_t i_avalaible_bytes;
        TPCircularBufferTail(&p_sys->circular_buffer, &i_avalaible_bytes);
        assert(i_avalaible_bytes >= 0);
        if (unlikely((size_t) i_avalaible_bytes >= p_block->i_buffer))
            continue;
        int32_t i_waiting_bytes = p_block->i_buffer - i_avalaible_bytes;

        const mtime_t i_frame_us =
            FramesToUs(p_sys, BytesToFrames(p_sys, i_waiting_bytes));

        /* Don't sleep less than 10ms */
        msleep(__MAX(i_frame_us, 10000));
    }

    unsigned i_underrun_size = atomic_exchange(&p_sys->i_underrun_size, 0);
    if (i_underrun_size > 0)
        msg_Warn(p_aout, "underrun of %u bytes", i_underrun_size);

    block_Release(p_block);
}

int
ca_Init(audio_output_t *p_aout, const audio_sample_format_t *fmt,
        size_t i_audio_buffer_size)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    /* setup circular buffer */
    if (!TPCircularBufferInit(&p_sys->circular_buffer, i_audio_buffer_size))
        return VLC_EGENERIC;

    atomic_init(&p_sys->i_underrun_size, 0);

    p_sys->i_rate = fmt->i_rate;
    p_sys->i_bytes_per_frame = fmt->i_bytes_per_frame;
    p_sys->i_frame_length = fmt->i_frame_length;

    p_aout->play = ca_Play;
    p_aout->flush = ca_Flush;
    p_aout->time_get = ca_TimeGet;
    return VLC_SUCCESS;
}

void
ca_Clean(audio_output_t *p_aout)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    /* clean-up circular buffer */
    TPCircularBufferCleanup(&p_sys->circular_buffer);
}
