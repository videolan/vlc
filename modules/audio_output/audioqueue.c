/*****************************************************************************
 * audioqueue.c : AudioQueue audio output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2010-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Romain Goyet <romain.goyet@likid.org>
 *          Felix Paul Kühne <fkuehne@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

#include <AudioToolBox/AudioToolBox.h>

/*****************************************************************************
 * aout_sys_t: AudioQueue audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    AudioQueueRef           audioQueue;
    AudioQueueTimelineRef   outTimeline;

    int                     i_rate;
    mtime_t                 i_played_length;
    bool                    b_stopped;
    float                   f_volume;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open               (vlc_object_t *);
static void Close              (vlc_object_t *);
static void Play               (audio_output_t *, block_t *);
static void Pause              (audio_output_t *p_aout, bool pause, mtime_t date);
static void Flush              (audio_output_t *p_aout, bool wait);
static int  TimeGet            (audio_output_t *aout, mtime_t *);
static void AudioQueueCallback (void *, AudioQueueRef, AudioQueueBufferRef);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
set_shortname("AudioQueue")
set_description(N_("AudioQueue (iOS / Mac OS) audio output"))
set_capability("audio output", 40)
set_category(CAT_AUDIO)
set_subcategory(SUBCAT_AUDIO_AOUT)
add_shortcut("audioqueue")
set_callbacks(Open, Close)
vlc_module_end ()

/*****************************************************************************
 * Start: open the audio device
 *****************************************************************************/

static int Start(audio_output_t *p_aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t *p_sys = p_aout->sys;
    OSStatus status = 0;

    // Setup the audio device.
    AudioStreamBasicDescription deviceFormat;
    deviceFormat.mSampleRate = fmt->i_rate;
    deviceFormat.mFormatID = kAudioFormatLinearPCM;
    deviceFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked; // FL32
    deviceFormat.mFramesPerPacket = 1;
    deviceFormat.mChannelsPerFrame = 2;
    deviceFormat.mBitsPerChannel = 32;
    deviceFormat.mBytesPerFrame = deviceFormat.mBitsPerChannel * deviceFormat.mChannelsPerFrame / 8;
    deviceFormat.mBytesPerPacket = deviceFormat.mBytesPerFrame * deviceFormat.mFramesPerPacket;

    // Create a new output AudioQueue for the device.
    status = AudioQueueNewOutput(&deviceFormat,         // Format
                                 AudioQueueCallback,    // Callback
                                 NULL,                  // User data, passed to the callback
                                 NULL,                  // RunLoop
                                 kCFRunLoopCommonModes, // RunLoop mode
                                 0,                     // Flags ; must be zero (per documentation)...
                                 &(p_sys->audioQueue)); // Output

    msg_Dbg(p_aout, "New AudioQueue output created (status = %li)", status);
    if (status != noErr)
        return VLC_EGENERIC;

    fmt->i_format = VLC_CODEC_FL32;
    fmt->i_physical_channels = AOUT_CHANS_STEREO;
    aout_FormatPrepare(fmt);

    p_aout->sys->b_stopped = false;
    p_aout->sys->i_rate = fmt->i_rate;

    status = AudioQueueStart(p_sys->audioQueue, NULL);
    msg_Dbg(p_aout, "Starting AudioQueue (status = %li)", status);

    status = AudioQueueCreateTimeline(p_sys->audioQueue, &p_sys->outTimeline);
    msg_Dbg(p_aout, "AudioQueue Timeline started (status = %li)", status);

    p_aout->time_get = TimeGet;
    p_aout->play = Play;
    p_aout->pause = Pause;
    p_aout->flush = Flush;

    if (status != noErr)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Stop: close the audio device
 *****************************************************************************/

static void Stop (audio_output_t *p_aout)
{
    p_aout->sys->b_stopped = true;
    p_aout->sys->i_played_length = 0;

    msg_Dbg(p_aout, "Stopping AudioQueue timeline");
    AudioQueueDisposeTimeline(p_aout->sys->audioQueue, p_aout->sys->outTimeline);
    msg_Dbg(p_aout, "Stopping AudioQueue");
    AudioQueueStop(p_aout->sys->audioQueue, true);
    msg_Dbg(p_aout, "Disposing AudioQueue");
    AudioQueueDispose(p_aout->sys->audioQueue, true);
}

/*****************************************************************************
 * actual playback
 *****************************************************************************/

static void Play (audio_output_t *p_aout, block_t *p_block)
{
    AudioQueueBufferRef inBuffer = NULL;
    OSStatus status;

    status = AudioQueueAllocateBuffer(p_aout->sys->audioQueue, p_block->i_buffer, &inBuffer);
    if (status != noErr) {
        msg_Err(p_aout, "buffer alloction failed (%li)", status);
        return;
    }

    memcpy(inBuffer->mAudioData, p_block->p_buffer, p_block->i_buffer);
    inBuffer->mAudioDataByteSize = p_block->i_buffer;

    status = AudioQueueEnqueueBuffer(p_aout->sys->audioQueue, inBuffer, 0, NULL);
    if (status == noErr)
        p_aout->sys->i_played_length += p_block->i_length;
    else
        msg_Err(p_aout, "enqueuing buffer failed (%li)", status);

    block_Release(p_block);
}

void AudioQueueCallback(void * inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
    /* this function does nothing, but needs to be here to make the AudioQueue API happy.
     * without a callback, it will refuse to create an AudioQueue instance. */
    VLC_UNUSED(inUserData);
    VLC_UNUSED(inAQ);
    VLC_UNUSED(inBuffer);
}

static void Pause (audio_output_t *p_aout, bool pause, mtime_t date)
{
    VLC_UNUSED(date);

    if (pause)
        AudioQueuePause(p_aout->sys->audioQueue);
    else
        AudioQueueStart(p_aout->sys->audioQueue, NULL);
}

static void Flush (audio_output_t *p_aout, bool wait)
{
    if (p_aout->sys->b_stopped || !p_aout->sys->audioQueue)
        return;

    AudioQueueDisposeTimeline(p_aout->sys->audioQueue, p_aout->sys->outTimeline);

    if (wait)
        AudioQueueStop(p_aout->sys->audioQueue, false);
    else
        AudioQueueStop(p_aout->sys->audioQueue, true);

    p_aout->sys->i_played_length = 0;
    AudioQueueStart(p_aout->sys->audioQueue, NULL);
    AudioQueueCreateTimeline(p_aout->sys->audioQueue, &p_aout->sys->outTimeline);
}

static int TimeGet (audio_output_t *p_aout, mtime_t *restrict delay)
{
    AudioTimeStamp outTimeStamp;
    Boolean b_discontinuity;
    OSStatus status = AudioQueueGetCurrentTime(p_aout->sys->audioQueue, p_aout->sys->outTimeline, &outTimeStamp, &b_discontinuity);

    if (status != noErr)
        return -1;

    if (b_discontinuity)
        msg_Dbg(p_aout, "detected output discontinuity");

    mtime_t i_pos = (mtime_t) outTimeStamp.mSampleTime * CLOCK_FREQ / p_aout->sys->i_rate;
    *delay = p_aout->sys->i_played_length - i_pos;

    return 0;
}

/*****************************************************************************
 * Module management
 *****************************************************************************/

static int VolumeSet(audio_output_t * p_aout, float volume)
{
    struct aout_sys_t *p_sys = p_aout->sys;
    OSStatus ostatus;

    aout_VolumeReport(p_aout, volume);
    p_sys->f_volume = volume;

    /* Set volume for output unit */
    ostatus = AudioQueueSetParameter(p_sys->audioQueue, kAudioQueueParam_Volume, volume * volume * volume);

    return ostatus;
}

/*****************************************************************************
 * Module management
 *****************************************************************************/

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = malloc(sizeof (*sys));

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout->volume_set = VolumeSet;

    /* reset volume */
    aout_VolumeReport(aout, 1.0);

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    msg_Dbg( aout, "audioqueue: Close");
    aout_sys_t *sys = aout->sys;

    free(sys);
}
