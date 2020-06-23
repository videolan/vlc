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
#import <CoreAudio/CoreAudioTypes.h>

static inline uint64_t
BytesToFrames(struct aout_sys_common *p_sys, size_t i_bytes)
{
    return i_bytes * p_sys->i_frame_length / p_sys->i_bytes_per_frame;
}

static inline vlc_tick_t
FramesToUs(struct aout_sys_common *p_sys, uint64_t i_nb_frames)
{
    return vlc_tick_from_samples(i_nb_frames, p_sys->i_rate);
}

static inline size_t
FramesToBytes(struct aout_sys_common *p_sys, uint64_t i_frames)
{
    return i_frames * p_sys->i_bytes_per_frame / p_sys->i_frame_length;
}

static inline uint64_t
UsToFrames(struct aout_sys_common *p_sys, vlc_tick_t i_us)
{
    return samples_from_vlc_tick(i_us, p_sys->i_rate);
}

static inline vlc_tick_t
HostTimeToTick(struct aout_sys_common *p_sys, uint64_t i_host_time)
{
    return VLC_TICK_FROM_NS(i_host_time * p_sys->tinfo.numer / p_sys->tinfo.denom);
}

static inline uint64_t
TickToHostTime(struct aout_sys_common *p_sys, vlc_tick_t i_us)
{
    return NS_FROM_VLC_TICK(i_us * p_sys->tinfo.denom / p_sys->tinfo.numer);
}

static void
ca_ClearOutBuffers(audio_output_t *p_aout)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    block_ChainRelease(p_sys->p_out_chain);
    p_sys->p_out_chain = NULL;
    p_sys->pp_out_last = &p_sys->p_out_chain;

    p_sys->i_out_size = 0;
}

static inline void
lock_init(struct aout_sys_common *p_sys)
{
    if (likely(os_unfair_lock_lock))
        p_sys->lock.unfair = OS_UNFAIR_LOCK_INIT;
    else
        vlc_mutex_init(&p_sys->lock.mutex);
}

static inline void
lock_lock(struct aout_sys_common *p_sys)
{
    if (likely(os_unfair_lock_lock))
        os_unfair_lock_lock(&p_sys->lock.unfair);
    else
        vlc_mutex_lock(&p_sys->lock.mutex);
}

static inline void
lock_unlock(struct aout_sys_common *p_sys)
{
    if (likely(os_unfair_lock_lock))
        os_unfair_lock_unlock(&p_sys->lock.unfair);
    else
        vlc_mutex_unlock(&p_sys->lock.mutex);
}

int
ca_Open(audio_output_t *p_aout)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    if (mach_timebase_info(&p_sys->tinfo) != KERN_SUCCESS)
        return VLC_EGENERIC;

    assert(p_sys->tinfo.denom != 0 && p_sys->tinfo.numer != 0);

    vlc_sem_init(&p_sys->flush_sem, 0);
    lock_init(p_sys);
    p_sys->p_out_chain = NULL;

    p_aout->play = ca_Play;
    p_aout->pause = ca_Pause;
    p_aout->flush = ca_Flush;
    p_aout->time_get = ca_TimeGet;

    return VLC_SUCCESS;
}

/* Called from render callbacks. No lock, wait, and IO here */
void
ca_Render(audio_output_t *p_aout, uint32_t i_frames, uint64_t i_host_time,
          uint8_t *p_output, size_t i_requested)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    lock_lock(p_sys);

    if (p_sys->b_do_flush)
    {
        ca_ClearOutBuffers(p_aout);
        /* Signal that the renderer is flushed */
        p_sys->b_do_flush = false;
        vlc_sem_post(&p_sys->flush_sem);
    }

    if (unlikely(p_sys->i_first_render_host_time == 0))
        goto drop;

    if (p_sys->b_paused)
    {
        p_sys->i_render_host_time = i_host_time;
        goto drop;
    }

    /* Start deferred: write silence (zeros) until we reach the first render
     * host time. */
    if (unlikely(p_sys->i_first_render_host_time > i_host_time ))
    {
        /* Convert the requested bytes into host time and check that it does
         * not overlap between the first_render host time and the current one.
         * */
        const size_t i_requested_us =
            FramesToUs(p_sys, BytesToFrames(p_sys, i_requested));
        const uint64_t i_requested_host_time =
            TickToHostTime(p_sys, i_requested_us);
        if (p_sys->i_first_render_host_time >= i_host_time + i_requested_host_time)
        {
            /* Fill the buffer with silence */
            goto drop;
        }

        /* Write silence to reach the first_render host time */
        const vlc_tick_t i_silence_us =
            HostTimeToTick(p_sys, p_sys->i_first_render_host_time - i_host_time);

        const uint64_t i_silence_bytes =
            FramesToBytes(p_sys, UsToFrames(p_sys, i_silence_us));
        assert(i_silence_bytes <= i_requested);
        memset(p_output, 0, i_silence_bytes);

        i_requested -= i_silence_bytes;

        /* Start the first rendering */
    }

    p_sys->i_render_host_time = i_host_time;
    p_sys->i_render_frames = i_frames;

    size_t i_copied = 0;
    block_t *p_block = p_sys->p_out_chain;
    while (p_block != NULL && i_requested != 0)
    {
        size_t i_tocopy = __MIN(i_requested, p_block->i_buffer);
        memcpy(&p_output[i_copied], p_block->p_buffer, i_tocopy);
        i_requested -= i_tocopy;
        i_copied += i_tocopy;
        if (i_tocopy == p_block->i_buffer)
        {
            block_t *p_release = p_block;
            p_block = p_block->p_next;
            block_Release(p_release);
        }
        else
        {
            assert(i_requested == 0);

            p_block->p_buffer += i_tocopy;
            p_block->i_buffer -= i_tocopy;
        }
    }
    p_sys->p_out_chain = p_block;
    if (!p_sys->p_out_chain)
        p_sys->pp_out_last = &p_sys->p_out_chain;
    p_sys->i_out_size -= i_copied;

    /* Pad with 0 */
    if (i_requested > 0)
    {
        assert(p_sys->i_out_size == 0);
        p_sys->i_underrun_size += i_requested;
        memset(&p_output[i_copied], 0, i_requested);
    }

    lock_unlock(p_sys);
    return;

drop:
    memset(p_output, 0, i_requested);
    lock_unlock(p_sys);
}

static vlc_tick_t
ca_GetLatencyLocked(audio_output_t *p_aout)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    const int64_t i_out_frames = BytesToFrames(p_sys, p_sys->i_out_size);
    return FramesToUs(p_sys, i_out_frames + p_sys->i_render_frames)
           + p_sys->i_dev_latency_us;
}

int
ca_TimeGet(audio_output_t *p_aout, vlc_tick_t *delay)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    lock_lock(p_sys);

    if (p_sys->i_render_host_time == 0 || p_sys->i_first_render_host_time == 0)
    {
        /* Not yet started (or reached the first_render host time) */
        lock_unlock(p_sys);
        return -1;
    }

    uint64_t i_render_delay_host_time = p_sys->i_render_host_time
                                      - mach_absolute_time();
    const vlc_tick_t i_render_delay =
        HostTimeToTick(p_sys, i_render_delay_host_time);

    *delay = ca_GetLatencyLocked(p_aout) + i_render_delay;
    lock_unlock(p_sys);
    return 0;
}

void
ca_Flush(audio_output_t *p_aout)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    lock_lock(p_sys);

    assert(!p_sys->b_do_flush);
    if (p_sys->b_paused)
        ca_ClearOutBuffers(p_aout);
    else
    {
        p_sys->b_do_flush = true;
        lock_unlock(p_sys);
        vlc_sem_wait(&p_sys->flush_sem);
        lock_lock(p_sys);
    }

    p_sys->i_render_host_time = p_sys->i_first_render_host_time = 0;
    p_sys->i_render_frames = 0;
    lock_unlock(p_sys);

    p_sys->b_played = false;
}

void
ca_Pause(audio_output_t * p_aout, bool pause, vlc_tick_t date)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;
    VLC_UNUSED(date);

    lock_lock(p_sys);
    p_sys->b_paused = pause;
    lock_unlock(p_sys);
}

void
ca_Play(audio_output_t * p_aout, block_t * p_block, vlc_tick_t date)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    /* Do the channel reordering */
    if (p_sys->chans_to_reorder)
       aout_ChannelReorder(p_block->p_buffer, p_block->i_buffer,
                           p_sys->chans_to_reorder, p_sys->chan_table,
                           VLC_CODEC_FL32);

    lock_lock(p_sys);

    if (p_sys->i_render_host_time == 0)
    {
        /* Setup the first render time, this date must be updated until the
         * first (non-silence/zero) frame is rendered by the render callback.
         * Once the rendering is truly started, the date can be ignored. */

        /* We can't convert date to host time directly since the clock source
         * may be different (MONOTONIC vs continue during sleep). The solution
         * is to convert it to a relative time and then add it to
         * mach_absolute_time() */
        const vlc_tick_t first_render_delay = date - vlc_tick_now()
                                            - ca_GetLatencyLocked(p_aout);
        p_sys->i_first_render_host_time
            = mach_absolute_time() + TickToHostTime(p_sys, first_render_delay);
    }

    do
    {
        const size_t i_avalaible_bytes =
            __MIN(p_block->i_buffer, p_sys->i_out_max_size - p_sys->i_out_size);

        if (unlikely(i_avalaible_bytes != p_block->i_buffer))
        {
            /* Not optimal but unlikely code path. */

            lock_unlock(p_sys);

            block_t *p_new = block_Alloc(i_avalaible_bytes);
            if (!p_new)
            {
                block_Release(p_block);
                return;
            }

            memcpy(p_new->p_buffer, p_block->p_buffer, i_avalaible_bytes);

            p_block->p_buffer += i_avalaible_bytes;
            p_block->i_buffer -= i_avalaible_bytes;

            lock_lock(p_sys);

            block_ChainLastAppend(&p_sys->pp_out_last, p_new);
            p_sys->i_out_size += i_avalaible_bytes;

            if (p_sys->b_paused)
            {
                lock_unlock(p_sys);
                block_Release(p_block);
                return;
            }

            const vlc_tick_t i_frame_us =
                FramesToUs(p_sys, BytesToFrames(p_sys, p_block->i_buffer));

            /* Wait for the render buffer to play the remaining data */
            lock_unlock(p_sys);
            vlc_tick_sleep(i_frame_us);
            lock_lock(p_sys);
        }
        else
        {
            block_ChainLastAppend(&p_sys->pp_out_last, p_block);
            p_sys->i_out_size += i_avalaible_bytes;
            p_block = NULL;
        }
    } while (p_block != NULL);

    size_t i_underrun_size = p_sys->i_underrun_size;
    p_sys->i_underrun_size = 0;

    lock_unlock(p_sys);

    if (!p_sys->b_played)
        p_sys->b_played = true;
    else if (i_underrun_size > 0)
        msg_Warn(p_aout, "underrun of %zu bytes", i_underrun_size);

    (void) date;
}

int
ca_Initialize(audio_output_t *p_aout, const audio_sample_format_t *fmt,
              vlc_tick_t i_dev_latency_us)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    p_sys->i_underrun_size = 0;
    p_sys->b_paused = false;
    p_sys->i_render_host_time = p_sys->i_first_render_host_time = 0;
    p_sys->i_render_frames = 0;

    p_sys->i_rate = fmt->i_rate;
    p_sys->i_bytes_per_frame = fmt->i_bytes_per_frame;
    p_sys->i_frame_length = fmt->i_frame_length;
    p_sys->chans_to_reorder = 0;

    p_sys->i_dev_latency_us = i_dev_latency_us;

    /* setup circular buffer */
    size_t i_audiobuffer_size = fmt->i_rate * fmt->i_bytes_per_frame
                              / p_sys->i_frame_length;
    if (fmt->channel_type == AUDIO_CHANNEL_TYPE_AMBISONICS)
    {
        /* lower latency: 200 ms of buffering. XXX: Decrease when VLC's core
         * can handle lower audio latency */
        p_sys->i_out_max_size = i_audiobuffer_size / 5;
    }
    else
    {
        /* 2 seconds of buffering */
        p_sys->i_out_max_size = i_audiobuffer_size * 2;
    }

    ca_ClearOutBuffers(p_aout);
    p_sys->b_played = false;

    return VLC_SUCCESS;
}

void
ca_Uninitialize(audio_output_t *p_aout)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;
    ca_ClearOutBuffers(p_aout);
    p_sys->i_out_max_size = 0;
}

void
ca_SetAliveState(audio_output_t *p_aout, bool alive)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    lock_lock(p_sys);

    bool b_sem_post = false;
    p_sys->b_paused = !alive;
    if (!alive && p_sys->b_do_flush)
    {
        ca_ClearOutBuffers(p_aout);
        p_sys->b_played = false;
        p_sys->b_do_flush = false;
        b_sem_post = true;
    }

    lock_unlock(p_sys);

    if (b_sem_post)
        vlc_sem_post(&p_sys->flush_sem);
}

void ca_SetDeviceLatency(audio_output_t *p_aout, vlc_tick_t i_dev_latency_us)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    lock_lock(p_sys);
    p_sys->i_dev_latency_us = i_dev_latency_us;
    lock_unlock(p_sys);
}

AudioUnit
au_NewOutputInstance(audio_output_t *p_aout, OSType comp_sub_type)
{
    AudioComponentDescription desc = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = comp_sub_type,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0,
    };

    AudioComponent au_component;
    au_component = AudioComponentFindNext(NULL, &desc);
    if (au_component == NULL)
    {
        msg_Err(p_aout, "cannot find any AudioComponent, PCM output failed");
        return NULL;
    }

    AudioUnit au;
    OSStatus err = AudioComponentInstanceNew(au_component, &au);
    if (err != noErr)
    {
        ca_LogErr("cannot open AudioComponent, PCM output failed");
        return NULL;
    }
    return au;
}

/*****************************************************************************
 * RenderCallback: This function is called everytime the AudioUnit wants
 * us to provide some more audio data.
 * Don't print anything during normal playback, calling blocking function from
 * this callback is not allowed.
 *****************************************************************************/
static OSStatus
RenderCallback(void *p_data, AudioUnitRenderActionFlags *ioActionFlags,
               const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
               UInt32 inNumberFrames, AudioBufferList *ioData)
{
    VLC_UNUSED(ioActionFlags);
    VLC_UNUSED(inTimeStamp);
    VLC_UNUSED(inBusNumber);

    uint64_t i_host_time = (inTimeStamp->mFlags & kAudioTimeStampHostTimeValid)
                         ? inTimeStamp->mHostTime : 0;

    ca_Render(p_data, inNumberFrames, i_host_time, ioData->mBuffers[0].mData,
              ioData->mBuffers[0].mDataByteSize);

    return noErr;
}

static AudioChannelLayout *
GetLayoutDescription(audio_output_t *p_aout,
                     const AudioChannelLayout *outlayout)
{
    AudioFormatPropertyID id;
    UInt32 size;
    const void *data;
    /* We need to "fill out" the ChannelLayout, because there are multiple
     * ways that it can be set */
    if (outlayout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap)
    {
        id = kAudioFormatProperty_ChannelLayoutForBitmap;
        size = sizeof(UInt32);
        data = &outlayout->mChannelBitmap;
    }
    else
    {
        id = kAudioFormatProperty_ChannelLayoutForTag;
        size = sizeof(AudioChannelLayoutTag);
        data = &outlayout->mChannelLayoutTag;
    }

    UInt32 param_size;
    OSStatus err = AudioFormatGetPropertyInfo(id, size, data, &param_size);
    if (err != noErr)
        return NULL;

    AudioChannelLayout *reslayout = malloc(param_size);
    if (reslayout == NULL)
        return NULL;

    err = AudioFormatGetProperty(id, size, data, &param_size, reslayout);
    if (err != noErr || reslayout->mNumberChannelDescriptions == 0)
    {
        msg_Err(p_aout, "insufficient number of output channels");
        free(reslayout);
        return NULL;
    }

    return reslayout;
}

static int
MapOutputLayout(audio_output_t *p_aout, audio_sample_format_t *fmt,
                const AudioChannelLayout *outlayout, bool *warn_configuration)
{
    /* Fill VLC physical_channels from output layout */
    fmt->i_physical_channels = 0;
    uint32_t i_original = fmt->i_physical_channels;
    AudioChannelLayout *reslayout = NULL;

    if (outlayout == NULL)
    {
        msg_Dbg(p_aout, "not output layout, default to Stereo");
        fmt->i_physical_channels = AOUT_CHANS_STEREO;
        goto end;
    }

    if (outlayout->mChannelLayoutTag !=
        kAudioChannelLayoutTag_UseChannelDescriptions)
    {
        reslayout = GetLayoutDescription(p_aout, outlayout);
        if (reslayout == NULL)
            return VLC_EGENERIC;
        outlayout = reslayout;
    }

    if (i_original == AOUT_CHAN_CENTER
     || outlayout->mNumberChannelDescriptions < 2)
    {
        /* We only need Mono or cannot output more than 1 channel */
        fmt->i_physical_channels = AOUT_CHAN_CENTER;
        msg_Dbg(p_aout, "output layout of AUHAL has 1 channel");
    }
    else if (i_original == (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)
          || outlayout->mNumberChannelDescriptions < 3)
    {
        /* We only need Stereo or cannot output more than 2 channels */
        fmt->i_physical_channels = AOUT_CHANS_STEREO;
        msg_Dbg(p_aout, "output layout of AUHAL is Stereo");
    }
    else
    {
        assert(outlayout->mNumberChannelDescriptions > 0);

        msg_Dbg(p_aout, "output layout of AUHAL has %i channels",
                outlayout->mNumberChannelDescriptions);

        /* maps auhal channels to vlc ones */
        static const unsigned i_auhal_channel_mapping[] = {
            [kAudioChannelLabel_Left]           = AOUT_CHAN_LEFT,
            [kAudioChannelLabel_Right]          = AOUT_CHAN_RIGHT,
            [kAudioChannelLabel_Center]         = AOUT_CHAN_CENTER,
            [kAudioChannelLabel_LFEScreen]      = AOUT_CHAN_LFE,
            [kAudioChannelLabel_LeftSurround]   = AOUT_CHAN_REARLEFT,
            [kAudioChannelLabel_RightSurround]  = AOUT_CHAN_REARRIGHT,
            /* needs to be swapped with rear */
            [kAudioChannelLabel_RearSurroundLeft]  = AOUT_CHAN_MIDDLELEFT,
            /* needs to be swapped with rear */
            [kAudioChannelLabel_RearSurroundRight] = AOUT_CHAN_MIDDLERIGHT,
            [kAudioChannelLabel_CenterSurround] = AOUT_CHAN_REARCENTER
        };
        static const size_t i_auhal_size = sizeof(i_auhal_channel_mapping)
                                         / sizeof(i_auhal_channel_mapping[0]);

        /* We want more than stereo and we can do that */
        for (unsigned i = 0; i < outlayout->mNumberChannelDescriptions; i++)
        {
            AudioChannelLabel chan =
                outlayout->mChannelDescriptions[i].mChannelLabel;
#ifndef NDEBUG
            msg_Dbg(p_aout, "this is channel: %d", (int) chan);
#endif
            if (chan < i_auhal_size && i_auhal_channel_mapping[chan] > 0)
                fmt->i_physical_channels |= i_auhal_channel_mapping[chan];
            else
                msg_Dbg(p_aout, "found nonrecognized channel %d at index "
                        "%d", chan, i);
        }
        if (fmt->i_physical_channels == 0)
        {
            fmt->i_physical_channels = AOUT_CHANS_STEREO;
            if (warn_configuration)
                *warn_configuration = true;
        }

    }

end:
    free(reslayout);
    aout_FormatPrepare(fmt);

    msg_Dbg(p_aout, "selected %d physical channels for device output",
            aout_FormatNbChannels(fmt));
    msg_Dbg(p_aout, "VLC will output: %s", aout_FormatPrintChannels(fmt));

    return VLC_SUCCESS;
}

static int
SetupInputLayout(audio_output_t *p_aout, const audio_sample_format_t *fmt,
                 AudioChannelLayoutTag *inlayout_tag)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;
    uint32_t chans_out[AOUT_CHAN_MAX];

    /* Some channel abbreviations used below:
     * L - left
     * R - right
     * C - center
     * Ls - left surround
     * Rs - right surround
     * Cs - center surround
     * Rls - rear left surround
     * Rrs - rear right surround
     * Lw - left wide
     * Rw - right wide
     * Lsd - left surround direct
     * Rsd - right surround direct
     * Lc - left center
     * Rc - right center
     * Ts - top surround
     * Vhl - vertical height left
     * Vhc - vertical height center
     * Vhr - vertical height right
     * Lt - left matrix total. for matrix encoded stereo.
     * Rt - right matrix total. for matrix encoded stereo. */

    switch (aout_FormatNbChannels(fmt))
    {
        case 1:
            *inlayout_tag = kAudioChannelLayoutTag_Mono;
            break;
        case 2:
            *inlayout_tag = kAudioChannelLayoutTag_Stereo;
            break;
        case 3:
            if (fmt->i_physical_channels & AOUT_CHAN_CENTER) /* L R C */
                *inlayout_tag = kAudioChannelLayoutTag_DVD_7;
            else if (fmt->i_physical_channels & AOUT_CHAN_LFE) /* L R LFE */
                *inlayout_tag = kAudioChannelLayoutTag_DVD_4;
            break;
        case 4:
            if (fmt->i_physical_channels & (AOUT_CHAN_CENTER | AOUT_CHAN_LFE)) /* L R C LFE */
                *inlayout_tag = kAudioChannelLayoutTag_DVD_10;
            else if (fmt->i_physical_channels & AOUT_CHANS_REAR) /* L R Ls Rs */
                *inlayout_tag = kAudioChannelLayoutTag_DVD_3;
            else if (fmt->i_physical_channels & AOUT_CHANS_CENTER) /* L R C Cs */
                *inlayout_tag = kAudioChannelLayoutTag_DVD_3;
            break;
        case 5:
            if (fmt->i_physical_channels & (AOUT_CHAN_CENTER)) /* L R Ls Rs C */
                *inlayout_tag = kAudioChannelLayoutTag_DVD_19;
            else if (fmt->i_physical_channels & (AOUT_CHAN_LFE)) /* L R Ls Rs LFE */
                *inlayout_tag = kAudioChannelLayoutTag_DVD_18;
            break;
        case 6:
            if (fmt->i_physical_channels & (AOUT_CHAN_LFE))
            {
                /* L R Ls Rs C LFE */
                *inlayout_tag = kAudioChannelLayoutTag_DVD_20;

                chans_out[0] = AOUT_CHAN_LEFT;
                chans_out[1] = AOUT_CHAN_RIGHT;
                chans_out[2] = AOUT_CHAN_REARLEFT;
                chans_out[3] = AOUT_CHAN_REARRIGHT;
                chans_out[4] = AOUT_CHAN_CENTER;
                chans_out[5] = AOUT_CHAN_LFE;

                p_sys->chans_to_reorder =
                    aout_CheckChannelReorder(NULL, chans_out,
                                             fmt->i_physical_channels,
                                             p_sys->chan_table);
                if (p_sys->chans_to_reorder)
                    msg_Dbg(p_aout, "channel reordering needed for 5.1 output");
            }
            else
            {
                /* L R Ls Rs C Cs */
                *inlayout_tag = kAudioChannelLayoutTag_AudioUnit_6_0;

                chans_out[0] = AOUT_CHAN_LEFT;
                chans_out[1] = AOUT_CHAN_RIGHT;
                chans_out[2] = AOUT_CHAN_REARLEFT;
                chans_out[3] = AOUT_CHAN_REARRIGHT;
                chans_out[4] = AOUT_CHAN_CENTER;
                chans_out[5] = AOUT_CHAN_REARCENTER;

                p_sys->chans_to_reorder =
                    aout_CheckChannelReorder(NULL, chans_out,
                                             fmt->i_physical_channels,
                                             p_sys->chan_table);
                if (p_sys->chans_to_reorder)
                    msg_Dbg(p_aout, "channel reordering needed for 6.0 output");
            }
            break;
        case 7:
            /* L R C LFE Ls Rs Cs */
            *inlayout_tag = kAudioChannelLayoutTag_MPEG_6_1_A;

            chans_out[0] = AOUT_CHAN_LEFT;
            chans_out[1] = AOUT_CHAN_RIGHT;
            chans_out[2] = AOUT_CHAN_CENTER;
            chans_out[3] = AOUT_CHAN_LFE;
            chans_out[4] = AOUT_CHAN_REARLEFT;
            chans_out[5] = AOUT_CHAN_REARRIGHT;
            chans_out[6] = AOUT_CHAN_REARCENTER;

            p_sys->chans_to_reorder =
                aout_CheckChannelReorder(NULL, chans_out,
                                         fmt->i_physical_channels,
                                         p_sys->chan_table);
            if (p_sys->chans_to_reorder)
                msg_Dbg(p_aout, "channel reordering needed for 6.1 output");

            break;
        case 8:
            if (fmt->i_physical_channels & (AOUT_CHAN_LFE))
            {
                /* L R C LFE Ls Rs Rls Rrs */
                *inlayout_tag = kAudioChannelLayoutTag_MPEG_7_1_C;

                chans_out[0] = AOUT_CHAN_LEFT;
                chans_out[1] = AOUT_CHAN_RIGHT;
                chans_out[2] = AOUT_CHAN_CENTER;
                chans_out[3] = AOUT_CHAN_LFE;
                chans_out[4] = AOUT_CHAN_MIDDLELEFT;
                chans_out[5] = AOUT_CHAN_MIDDLERIGHT;
                chans_out[6] = AOUT_CHAN_REARLEFT;
                chans_out[7] = AOUT_CHAN_REARRIGHT;
            }
            else
            {
                /* Lc C Rc L R Ls Cs Rs */
                *inlayout_tag = kAudioChannelLayoutTag_DTS_8_0_B;

                chans_out[0] = AOUT_CHAN_MIDDLELEFT;
                chans_out[1] = AOUT_CHAN_CENTER;
                chans_out[2] = AOUT_CHAN_MIDDLERIGHT;
                chans_out[3] = AOUT_CHAN_LEFT;
                chans_out[4] = AOUT_CHAN_RIGHT;
                chans_out[5] = AOUT_CHAN_REARLEFT;
                chans_out[6] = AOUT_CHAN_REARCENTER;
                chans_out[7] = AOUT_CHAN_REARRIGHT;
            }
            p_sys->chans_to_reorder =
                aout_CheckChannelReorder(NULL, chans_out,
                                         fmt->i_physical_channels,
                                         p_sys->chan_table);
            if (p_sys->chans_to_reorder)
                msg_Dbg(p_aout, "channel reordering needed for 7.1 / 8.0 output");
            break;
        case 9:
            /* Lc C Rc L R Ls Cs Rs LFE */
            *inlayout_tag = kAudioChannelLayoutTag_DTS_8_1_B;
            chans_out[0] = AOUT_CHAN_MIDDLELEFT;
            chans_out[1] = AOUT_CHAN_CENTER;
            chans_out[2] = AOUT_CHAN_MIDDLERIGHT;
            chans_out[3] = AOUT_CHAN_LEFT;
            chans_out[4] = AOUT_CHAN_RIGHT;
            chans_out[5] = AOUT_CHAN_REARLEFT;
            chans_out[6] = AOUT_CHAN_REARCENTER;
            chans_out[7] = AOUT_CHAN_REARRIGHT;
            chans_out[8] = AOUT_CHAN_LFE;

            p_sys->chans_to_reorder =
                aout_CheckChannelReorder(NULL, chans_out,
                                         fmt->i_physical_channels,
                                         p_sys->chan_table);
            if (p_sys->chans_to_reorder)
                msg_Dbg(p_aout, "channel reordering needed for 8.1 output");
            break;
    }

    return VLC_SUCCESS;
}

int
au_Initialize(audio_output_t *p_aout, AudioUnit au, audio_sample_format_t *fmt,
              const AudioChannelLayout *outlayout, vlc_tick_t i_dev_latency_us,
              bool *warn_configuration)
{
    int ret;
    AudioChannelLayoutTag inlayout_tag;

    if (warn_configuration)
        *warn_configuration = false;

    /* Set the desired format */
    AudioStreamBasicDescription desc;
    if (aout_BitsPerSample(fmt->i_format) != 0)
    {
        /* PCM */
        fmt->i_format = VLC_CODEC_FL32;
        ret = MapOutputLayout(p_aout, fmt, outlayout, warn_configuration);
        if (ret != VLC_SUCCESS)
            return ret;

        ret = SetupInputLayout(p_aout, fmt, &inlayout_tag);
        if (ret != VLC_SUCCESS)
            return ret;

        desc.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
        desc.mChannelsPerFrame = aout_FormatNbChannels(fmt);
        desc.mBitsPerChannel = 32;
    }
    else if (AOUT_FMT_SPDIF(fmt))
    {
        /* Passthrough */
        fmt->i_format = VLC_CODEC_SPDIFL;
        fmt->i_bytes_per_frame = 4;
        fmt->i_frame_length = 1;

        inlayout_tag = kAudioChannelLayoutTag_Stereo;

        desc.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger |
                            kLinearPCMFormatFlagIsPacked; /* S16LE */
        desc.mChannelsPerFrame = 2;
        desc.mBitsPerChannel = 16;
    }
    else
        return VLC_EGENERIC;

    desc.mSampleRate = fmt->i_rate;
    desc.mFormatID = kAudioFormatLinearPCM;
    desc.mFramesPerPacket = 1;
    desc.mBytesPerFrame = desc.mBitsPerChannel * desc.mChannelsPerFrame / 8;
    desc.mBytesPerPacket = desc.mBytesPerFrame * desc.mFramesPerPacket;

    OSStatus err = AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat,
                                        kAudioUnitScope_Input, 0, &desc,
                                        sizeof(desc));
    if (err != noErr)
    {
        ca_LogErr("failed to set stream format");
        return VLC_EGENERIC;
    }
    msg_Dbg(p_aout, STREAM_FORMAT_MSG("Current AU format: " , desc));

    /* Retrieve actual format */
    err = AudioUnitGetProperty(au, kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0, &desc,
                               &(UInt32) { sizeof(desc) });
    if (err != noErr)
    {
        ca_LogErr("failed to set stream format");
        return VLC_EGENERIC;
    }

    /* Set the IOproc callback */
    const AURenderCallbackStruct callback = {
        .inputProc = RenderCallback,
        .inputProcRefCon = p_aout,
    };

    err = AudioUnitSetProperty(au, kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input, 0, &callback,
                               sizeof(callback));
    if (err != noErr)
    {
        ca_LogErr("failed to setup render callback");
        return VLC_EGENERIC;
    }

    /* Set the input_layout as the layout VLC will use to feed the AU unit.
     * Yes, it must be the INPUT scope */
    AudioChannelLayout inlayout = {
        .mChannelLayoutTag = inlayout_tag,
    };
    err = AudioUnitSetProperty(au, kAudioUnitProperty_AudioChannelLayout,
                               kAudioUnitScope_Input, 0, &inlayout,
                               sizeof(inlayout));
    if (err != noErr)
    {
        ca_LogErr("failed to setup input layout");
        return VLC_EGENERIC;
    }

    /* AU init */
    err = AudioUnitInitialize(au);

    if (err != noErr)
    {
        ca_LogErr("AudioUnitInitialize failed");
        return VLC_EGENERIC;
    }

    ret = ca_Initialize(p_aout, fmt, i_dev_latency_us);
    if (ret != VLC_SUCCESS)
    {
        AudioUnitUninitialize(au);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

void
au_Uninitialize(audio_output_t *p_aout, AudioUnit au)
{
    OSStatus err = AudioUnitUninitialize(au);
    if (err != noErr)
        ca_LogWarn("AudioUnitUninitialize failed");

    ca_Uninitialize(p_aout);
}
