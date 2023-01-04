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

#include "coreaudio_common.h"
#include <CoreAudio/CoreAudioTypes.h>

#define TIMING_REPORT_DELAY_TICKS VLC_TICK_FROM_MS(1000)

static inline uint64_t
BytesToFrames(struct aout_sys_common *p_sys, size_t i_bytes)
{
    return i_bytes * p_sys->i_frame_length / p_sys->i_bytes_per_frame;
}

static inline vlc_tick_t
FramesToTicks(struct aout_sys_common *p_sys, int64_t i_nb_frames)
{
    return vlc_tick_from_samples(i_nb_frames, p_sys->i_rate);
}

static inline vlc_tick_t
BytesToTicks(struct aout_sys_common *p_sys, size_t i_bytes)
{
    return FramesToTicks(p_sys, BytesToFrames(p_sys, i_bytes));
}

static inline size_t
FramesToBytes(struct aout_sys_common *p_sys, uint64_t i_frames)
{
    return i_frames * p_sys->i_bytes_per_frame / p_sys->i_frame_length;
}

static inline int64_t
TicksToFrames(struct aout_sys_common *p_sys, vlc_tick_t i_ticks)
{
    return samples_from_vlc_tick(i_ticks, p_sys->i_rate);
}

static inline size_t
TicksToBytes(struct aout_sys_common *p_sys, vlc_tick_t i_ticks)
{
    return FramesToBytes(p_sys, TicksToFrames(p_sys, i_ticks));
}

/**
 * Convert a relative audio host time to vlc_ticks
 *
 * \warning  This function may only be used to convert relative
 *           host times to ticks, as vlc_ticks do not have the
 *           same clock origin as the audio host clock!
 */
static inline vlc_tick_t
HostTimeToTick(struct aout_sys_common *p_sys, int64_t i_host_time)
{
    return VLC_TICK_FROM_NS(i_host_time * p_sys->tinfo.numer / p_sys->tinfo.denom);
}

static void
ca_ClearOutBuffers(audio_output_t *p_aout)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    block_ChainRelease(p_sys->p_out_chain);
    p_sys->p_out_chain = NULL;
    p_sys->pp_out_last = &p_sys->p_out_chain;
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

    lock_init(p_sys);
    p_sys->p_out_chain = NULL;
    p_sys->pp_out_last = &p_sys->p_out_chain;
    p_sys->chans_to_reorder = 0;

    p_aout->play = ca_Play;
    p_aout->pause = ca_Pause;
    p_aout->flush = ca_Flush;
    p_aout->time_get = NULL;

    return VLC_SUCCESS;
}

static vlc_tick_t
GetLatency(audio_output_t *p_aout)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    vlc_tick_t dev_latency_ticks =
        p_sys->get_latency != NULL ? p_sys->get_latency(p_aout)
                                   : p_sys->i_dev_latency_ticks;

    /* Add the AudioUnit latency to the auhal/audiounit_ios latency */
    return dev_latency_ticks + p_sys->au_latency_ticks;
}

/* Called from render callbacks. No lock, wait, and IO here */
void
ca_Render(audio_output_t *p_aout, uint64_t host_time,
          uint8_t *data, size_t bytes, bool *is_silence)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    vlc_tick_t host_delay_ticks = 0;
    if (host_time != 0)
    {
        uint64_t now_nsec = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
        host_delay_ticks = HostTimeToTick(p_sys, host_time)
                         - VLC_TICK_FROM_NS(now_nsec);
    }
    const vlc_tick_t bytes_ticks = BytesToTicks(p_sys, bytes);

    const vlc_tick_t now_ticks = vlc_tick_now();
    const vlc_tick_t end_ticks = now_ticks + bytes_ticks + host_delay_ticks;

    lock_lock(p_sys);

    if (p_sys->b_paused)
    {
        if (is_silence != NULL)
            *is_silence = true;
        memset(data, 0, bytes);
        lock_unlock(p_sys);
        return;
    }

    if (!p_sys->started)
    {
        size_t tocopy;

        if (p_sys->first_play_date == VLC_TICK_INVALID)
            tocopy = bytes;
        else
        {
            /* Write silence to reach the first play date */
            vlc_tick_t silence_ticks = p_sys->first_play_date - end_ticks
                                     + bytes_ticks;
            if (silence_ticks > 0)
            {
                tocopy = TicksToBytes(p_sys, silence_ticks);
                if (tocopy > bytes)
                    tocopy = bytes;
            }
            else
                tocopy = 0;
        }

        if (tocopy > 0)
        {
            memset(data, 0, tocopy);

            data += tocopy;
            bytes -= tocopy;

            if (bytes == 0 && is_silence != NULL)
                *is_silence = true;
        }
    }

    size_t bytes_copied = 0;
    while (bytes > 0)
    {
        vlc_frame_t *f = p_sys->p_out_chain;
        if (f == NULL)
        {
            p_sys->i_underrun_size += bytes;
            memset(data, 0, bytes);
            lock_unlock(p_sys);
            return;
        }

        size_t tocopy = f->i_buffer > bytes ? bytes : f->i_buffer;

        p_sys->i_out_size -= tocopy;
        p_sys->i_total_bytes += tocopy;

        p_sys->started = true;

        memcpy(data, f->p_buffer, tocopy);

        data += tocopy;
        bytes -= tocopy;
        f->i_buffer -= tocopy;
        f->p_buffer += tocopy;
        bytes_copied += tocopy;

        if (f->i_buffer == 0)
        {
            p_sys->p_out_chain = f->p_next;
            if (p_sys->p_out_chain == NULL)
                p_sys->pp_out_last = &p_sys->p_out_chain;

            block_Release(f);
        }
    }

    if (p_sys->timing_report_last_written_bytes >=
        p_sys->timing_report_delay_bytes)
    {
        p_sys->timing_report_last_written_bytes = 0;
        vlc_tick_t pos_ticks = BytesToTicks(p_sys, p_sys->i_total_bytes);
        const vlc_tick_t latency_ticks = GetLatency(p_aout);
        aout_LatencyReport(p_aout, latency_ticks);
        aout_TimingReport(p_aout, end_ticks, pos_ticks);
    }
    else
        p_sys->timing_report_last_written_bytes += bytes_copied;

    lock_unlock(p_sys);
}

void
ca_Flush(audio_output_t *p_aout)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;


    lock_lock(p_sys);

    p_sys->started = false;
    p_sys->i_out_size = 0;
    p_sys->i_total_bytes = 0;
    p_sys->first_play_date = VLC_TICK_INVALID;
    p_sys->timing_report_last_written_bytes = 0;

    ca_ClearOutBuffers(p_aout);
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
    p_sys->started = false;
    lock_unlock(p_sys);
}

void
ca_MuteSet(audio_output_t * p_aout, bool muted)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    lock_lock(p_sys);
    p_sys->b_muted = muted;
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

    if (!p_sys->started)
    {
        vlc_tick_t now = vlc_tick_now();
        p_sys->first_play_date = date - BytesToTicks(p_sys, p_sys->i_out_size);

        if (p_sys->first_play_date > now)
            msg_Dbg(p_aout, "deferring start (%"PRId64" us)",
                    p_sys->first_play_date - now);
        else
            msg_Dbg(p_aout, "starting late (%"PRId64" us)",
                    p_sys->first_play_date - now);
    }

    p_sys->i_out_size += p_block->i_buffer;
    block_ChainLastAppend(&p_sys->pp_out_last, p_block);

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
              vlc_tick_t i_dev_latency_ticks, get_latency_cb get_latency)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    p_sys->au_latency_ticks = 0;
    p_sys->i_underrun_size = 0;
    p_sys->b_paused = false;
    p_sys->started = false;
    p_sys->b_muted = false;
    p_sys->i_out_size = 0;
    p_sys->i_total_bytes = 0;
    p_sys->first_play_date = VLC_TICK_INVALID;
    p_sys->timing_report_last_written_bytes = 0;

    p_sys->i_rate = fmt->i_rate;
    p_sys->i_bytes_per_frame = fmt->i_bytes_per_frame;
    p_sys->i_frame_length = fmt->i_frame_length;

    if (get_latency != NULL)
        p_sys->get_latency = get_latency;
    else
        p_sys->i_dev_latency_ticks = i_dev_latency_ticks;
    p_sys->timing_report_delay_bytes = TicksToBytes(p_sys, TIMING_REPORT_DELAY_TICKS);

    ca_ClearOutBuffers(p_aout);
    p_sys->b_played = false;

    return VLC_SUCCESS;
}

void
ca_Uninitialize(audio_output_t *p_aout)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;
    ca_ClearOutBuffers(p_aout);
    p_sys->chans_to_reorder = 0;
}

void
ca_SetAliveState(audio_output_t *p_aout, bool alive)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    lock_lock(p_sys);
    p_sys->b_paused = !alive;
    lock_unlock(p_sys);
}

void ca_ResetDeviceLatency(audio_output_t *p_aout)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;

    lock_lock(p_sys);
    /* Trigger aout_TimingReport() to be called from the next render callback */
    p_sys->timing_report_last_written_bytes = p_sys->timing_report_delay_bytes;
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
 * RenderCallback: This function is called every time the AudioUnit wants
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
    VLC_UNUSED(inNumberFrames);

    uint64_t i_host_time = (inTimeStamp->mFlags & kAudioTimeStampHostTimeValid)
                         ? inTimeStamp->mHostTime : 0;

    bool is_silence;
    ca_Render(p_data, i_host_time, ioData->mBuffers[0].mData,
              ioData->mBuffers[0].mDataByteSize, &is_silence);
    if (is_silence)
        *ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;

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

static unsigned
AudioChannelLabelToVlcChan(AudioChannelLabel chan, bool swap_rear_surround)
{
    /* maps auhal channels to vlc ones */
    switch (chan)
    {
        case kAudioChannelLabel_Left:
            return AOUT_CHAN_LEFT;
        case kAudioChannelLabel_Right:
            return AOUT_CHAN_RIGHT;
        case kAudioChannelLabel_Center:
            return AOUT_CHAN_CENTER;
        case kAudioChannelLabel_LFEScreen:
            return AOUT_CHAN_LFE;
        case kAudioChannelLabel_LeftSurround:
            return swap_rear_surround ? AOUT_CHAN_MIDDLELEFT
                                      : AOUT_CHAN_REARLEFT;
        case kAudioChannelLabel_RightSurround:
            return swap_rear_surround ? AOUT_CHAN_MIDDLERIGHT
                                      : AOUT_CHAN_REARRIGHT;
        case kAudioChannelLabel_RearSurroundLeft:
            return swap_rear_surround ? AOUT_CHAN_REARLEFT
                                      : AOUT_CHAN_MIDDLELEFT;
        case kAudioChannelLabel_RearSurroundRight:
            return swap_rear_surround ? AOUT_CHAN_REARRIGHT
                                      : AOUT_CHAN_MIDDLERIGHT;
        case kAudioChannelLabel_CenterSurround:
            return AOUT_CHAN_REARCENTER;
        case kAudioChannelLabel_LeftSurroundDirect:
            return AOUT_CHAN_MIDDLELEFT;
        case kAudioChannelLabel_RightSurroundDirect:
            return AOUT_CHAN_MIDDLERIGHT;
        default:
            return 0;
    }
}

static AudioChannelLabel
VlcChanToAudioChannelLabel(unsigned chan, bool swap_rear_surround)
{
    /* maps auhal channels to vlc ones */
    switch (chan)
    {
        case AOUT_CHAN_LEFT:
            return kAudioChannelLabel_Left;
        case AOUT_CHAN_RIGHT:
            return kAudioChannelLabel_Right;
        case AOUT_CHAN_CENTER:
            return kAudioChannelLabel_Center;
        case AOUT_CHAN_LFE:
            return kAudioChannelLabel_LFEScreen;
        case AOUT_CHAN_REARLEFT:
            return swap_rear_surround ? kAudioChannelLabel_RearSurroundLeft
                                      : kAudioChannelLabel_LeftSurround;
        case AOUT_CHAN_REARRIGHT:
            return swap_rear_surround ? kAudioChannelLabel_RearSurroundRight
                                      : kAudioChannelLabel_RightSurround;
        case AOUT_CHAN_MIDDLELEFT:
            return swap_rear_surround ? kAudioChannelLabel_LeftSurround
                                      : kAudioChannelLabel_RearSurroundLeft;
        case AOUT_CHAN_MIDDLERIGHT:
            return swap_rear_surround ? kAudioChannelLabel_RightSurround
                                      : kAudioChannelLabel_RearSurroundRight;
        case AOUT_CHAN_REARCENTER:
            return kAudioChannelLabel_CenterSurround;
        default:
            vlc_assert_unreachable();
    }
}

static int
MapOutputLayout(audio_output_t *p_aout, audio_sample_format_t *fmt,
                const AudioChannelLayout *outlayout, bool *warn_configuration)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;
    /* Fill VLC physical_channels from output layout */
    uint32_t i_original = fmt->i_physical_channels;
    fmt->i_physical_channels = 0;
    AudioChannelLayout *reslayout = NULL;
    assert(outlayout != NULL);

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
        uint32_t chans_out[AOUT_CHAN_MAX];

        /* For 7.1, AOUT_CHAN_MIDDLELEFT/RIGHT needs to be swapped with
         * AOUT_CHAN_REARLEFT/RIGHT. Auhal
         * kAudioChannelLabel_Left/RightSurround are used as surround for 5.1,
         * but as middle speakers for rear 7.1. */
        unsigned swap_rear_surround = 0;
        if (outlayout->mNumberChannelDescriptions == 8)
        {
            for (unsigned i = 0; i < outlayout->mNumberChannelDescriptions; i++)
            {
                AudioChannelLabel chan =
                    outlayout->mChannelDescriptions[i].mChannelLabel;
                if (chan == kAudioChannelLabel_RearSurroundLeft
                 || chan == kAudioChannelLabel_RearSurroundRight)
                    swap_rear_surround++;
            }
            if (swap_rear_surround == 2)
                msg_Dbg(p_aout, "swapping Surround and RearSurround channels "
                        "for 7.1 Rear Surround");
        }

        for (unsigned i = 0; i < outlayout->mNumberChannelDescriptions; i++)
        {
            AudioChannelLabel chan =
                outlayout->mChannelDescriptions[i].mChannelLabel;
#ifndef NDEBUG
            msg_Dbg(p_aout, "this is channel: %d", (int) chan);
#endif
            unsigned mapped_chan =
                AudioChannelLabelToVlcChan(chan, swap_rear_surround == 2);
            if (mapped_chan != 0)
            {
                chans_out[i] = mapped_chan;
                fmt->i_physical_channels |= mapped_chan;
            }
            else
            {
                chans_out[i] = 0;
                msg_Dbg(p_aout, "found nonrecognized channel %d at index "
                        "%d", chan, i);
            }
        }
        if (fmt->i_physical_channels == 0)
        {
            fmt->i_physical_channels = AOUT_CHANS_STEREO;
            if (warn_configuration)
                *warn_configuration = true;
        }
        else
        {
            p_sys->chans_to_reorder =
                aout_CheckChannelReorder(NULL, chans_out,
                                         fmt->i_physical_channels,
                                         p_sys->chan_table);
            if (p_sys->chans_to_reorder)
                msg_Dbg(p_aout, "channel reordering needed");
        }
    }

    free(reslayout);
    aout_FormatPrepare(fmt);

    msg_Dbg(p_aout, "VLC will output: %s", aout_FormatPrintChannels(fmt));

    return VLC_SUCCESS;
}

static int
MapInputLayout(audio_output_t *p_aout, const audio_sample_format_t *fmt,
               AudioChannelLayout **inlayoutp, size_t *inlayout_size)
{
    unsigned channels = aout_FormatNbChannels(fmt);

    size_t size;
    if (mul_overflow(channels, sizeof(AudioChannelDescription), &size))
        return VLC_ENOMEM;
    if (add_overflow(size, sizeof(AudioChannelLayout), &size))
        return VLC_ENOMEM;
    AudioChannelLayout *inlayout = malloc(size);
    if (inlayout == NULL)
        return VLC_ENOMEM;

    *inlayoutp = inlayout;
    *inlayout_size = size;
    inlayout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
    inlayout->mNumberChannelDescriptions = aout_FormatNbChannels(fmt);

    bool swap_rear_surround = (fmt->i_physical_channels & AOUT_CHANS_7_0) == AOUT_CHANS_7_0;
    if (swap_rear_surround)
        msg_Dbg(p_aout, "swapping Surround and RearSurround channels "
                "for 7.1 Rear Surround");
    unsigned chan_idx = 0;
    for (unsigned i = 0; i < AOUT_CHAN_MAX; ++i)
    {
        unsigned vlcchan = pi_vlc_chan_order_wg4[i];
        if ((vlcchan & fmt->i_physical_channels) == 0)
            continue;

        inlayout->mChannelDescriptions[chan_idx].mChannelLabel =
            VlcChanToAudioChannelLabel(vlcchan, swap_rear_surround);
        inlayout->mChannelDescriptions[chan_idx].mChannelFlags =
            kAudioChannelFlags_AllOff;
        chan_idx++;
    }

    msg_Dbg(p_aout, "VLC keeping the same input layout");

    return VLC_SUCCESS;
}

int
au_Initialize(audio_output_t *p_aout, AudioUnit au, audio_sample_format_t *fmt,
              const AudioChannelLayout *outlayout, vlc_tick_t i_dev_latency_ticks,
              get_latency_cb get_latency, bool *warn_configuration)
{
    struct aout_sys_common *p_sys = (struct aout_sys_common *) p_aout->sys;
    int ret;
    AudioChannelLayout *inlayout_buf = NULL;
    const AudioChannelLayout *inlayout = NULL;
    size_t inlayout_size = 0;

    if (warn_configuration)
        *warn_configuration = false;

    /* Set the desired format */
    AudioStreamBasicDescription desc;
    if (aout_BitsPerSample(fmt->i_format) != 0)
    {
        /* PCM */
        fmt->i_format = VLC_CODEC_FL32;
        if (outlayout != NULL)
        {
            ret = MapOutputLayout(p_aout, fmt, outlayout, warn_configuration);
            if (ret != VLC_SUCCESS)
                return ret;
        }
        else
        {
            ret = MapInputLayout(p_aout, fmt, &inlayout_buf, &inlayout_size);
            if (ret != VLC_SUCCESS)
                return ret;
            inlayout = inlayout_buf;
        }

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

        static const AudioChannelLayout inlayout_spdif = {
            .mChannelLayoutTag = kAudioChannelLayoutTag_Stereo,
        };
        inlayout = &inlayout_spdif;
        inlayout_size = sizeof(inlayout_spdif);

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
        free(inlayout_buf);
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
        free(inlayout_buf);
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
        free(inlayout_buf);
        return VLC_EGENERIC;
    }

    if (inlayout != NULL)
    {
        /* Set the input_layout as the layout VLC will use to feed the AU unit.
         * Yes, it must be the INPUT scope */
        err = AudioUnitSetProperty(au, kAudioUnitProperty_AudioChannelLayout,
                                   kAudioUnitScope_Input, 0, inlayout,
                                   inlayout_size);
        free(inlayout_buf);
        if (err != noErr)
        {
            ca_LogErr("failed to setup input layout");
            return VLC_EGENERIC;
        }
    }

    /* AU init */
    err = AudioUnitInitialize(au);

    if (err != noErr)
    {
        ca_LogErr("AudioUnitInitialize failed");
        return VLC_EGENERIC;
    }

    ret = ca_Initialize(p_aout, fmt, i_dev_latency_ticks, get_latency);
    if (ret != VLC_SUCCESS)
    {
        AudioUnitUninitialize(au);
        return VLC_EGENERIC;
    }

    Float64 unit_s;
    if (AudioUnitGetProperty(au, kAudioUnitProperty_Latency,
                             kAudioUnitScope_Global, 0, &unit_s,
                             &(UInt32) { sizeof(unit_s) }) == noErr)
    {
        p_sys->au_latency_ticks = vlc_tick_from_sec(unit_s);
        msg_Dbg(p_aout, "AudioUnit latency: %" PRId64 "us", p_sys->au_latency_ticks);
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
