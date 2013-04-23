/*****************************************************************************
 * Copyright (C) 2000-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
 *          Rémi Denis-Courmont
 *          Rafaël Carré <funman at videolan dot org>
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

#pragma mark includes

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif
#import <vlc_common.h>
#import <vlc_plugin.h>
#import <vlc_aout.h>
#import <AudioToolbox/AudioQueue.h>
#import <TargetConditionals.h>
#if TARGET_OS_IPHONE
#import <AudioToolbox/AudioSession.h>
#else
#define AudioSessionSetActive(x)
#endif

#pragma mark -
#pragma mark private declarations

struct aout_sys_t
{
    AudioQueueRef           audioQueueRef;
    AudioQueueTimelineRef   timelineRef;

    bool                    b_started;

    mtime_t                 i_played_length;
    int                     i_rate;
    float                   f_volume;
};
static int  Open                     (vlc_object_t *);
static void Close                    (vlc_object_t *);
static void Play                     (audio_output_t *, block_t *);
static void Pause                    (audio_output_t *p_aout, bool pause, mtime_t date);
static void Flush                    (audio_output_t *p_aout, bool wait);
static int  TimeGet                  (audio_output_t *aout, mtime_t *);
static void UnusedAudioQueueCallback (void *, AudioQueueRef, AudioQueueBufferRef);
static int Start(audio_output_t *, audio_sample_format_t *);
static void Stop(audio_output_t *);
static int VolumeSet(audio_output_t *, float );
vlc_module_begin ()
set_shortname("AudioQueue")
set_description(N_("AudioQueue (iOS / Mac OS) audio output"))
set_capability("audio output", 40)
set_category(CAT_AUDIO)
set_subcategory(SUBCAT_AUDIO_AOUT)
add_shortcut("audioqueue")
set_callbacks(Open, Close)
vlc_module_end ()

#pragma mark -
#pragma mark initialization

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

static int VolumeSet(audio_output_t * p_aout, float volume)
{
    struct aout_sys_t *p_sys = p_aout->sys;
    OSStatus ostatus;

    aout_VolumeReport(p_aout, volume);
    p_sys->f_volume = volume;

    /* Set volume for output unit */
    ostatus = AudioQueueSetParameter(p_sys->audioQueueRef, kAudioQueueParam_Volume, volume * volume * volume);

    return ostatus;
}

static int Start(audio_output_t *p_aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t *p_sys = p_aout->sys;
    OSStatus error = 0;

    // prepare the format description for our output
    AudioStreamBasicDescription streamDescription;
    streamDescription.mSampleRate = fmt->i_rate;
    streamDescription.mFormatID = kAudioFormatLinearPCM;
    streamDescription.mFormatFlags = kAudioFormatFlagsNativeFloatPacked; // FL32
    streamDescription.mFramesPerPacket = 1;
    streamDescription.mChannelsPerFrame = 2;
    streamDescription.mBitsPerChannel = 32;
    streamDescription.mBytesPerFrame = streamDescription.mBitsPerChannel * streamDescription.mChannelsPerFrame / 8;
    streamDescription.mBytesPerPacket = streamDescription.mBytesPerFrame * streamDescription.mFramesPerPacket;

    // init new output instance
    error = AudioQueueNewOutput(&streamDescription,           // Format
                                 UnusedAudioQueueCallback,    // Unused Callback, which needs to be provided to have a proper instance
                                 NULL,                        // User data, passed to the callback
                                 NULL,                        // RunLoop
                                 kCFRunLoopCommonModes,       // RunLoop mode
                                 0,                           // Flags ; must be zero (per documentation)...
                                 &(p_sys->audioQueueRef));    // Output
    msg_Dbg(p_aout, "New AudioQueue instance created (status = %li)", error);
    if (error != noErr)
        return VLC_EGENERIC;
    fmt->i_format = VLC_CODEC_FL32;
    fmt->i_physical_channels = AOUT_CHANS_STEREO;
    aout_FormatPrepare(fmt);
    p_aout->sys->i_rate = fmt->i_rate;

    // start queue
    error = AudioQueueStart(p_sys->audioQueueRef, NULL);
    msg_Dbg(p_aout, "Starting AudioQueue (status = %li)", error);

    // start timeline for synchro
    error = AudioQueueCreateTimeline(p_sys->audioQueueRef, &p_sys->timelineRef);
    msg_Dbg(p_aout, "AudioQueue Timeline started (status = %li)", error);
    if (error != noErr)
        return VLC_EGENERIC;

#if TARGET_OS_IPHONE
    // start audio session so playback continues if mute switch is on
    AudioSessionInitialize (NULL,
                            kCFRunLoopCommonModes,
                            NULL,
                            NULL);

	// Set audio session to mediaplayback
	UInt32 sessionCategory = kAudioSessionCategory_MediaPlayback;
	AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(sessionCategory),&sessionCategory);
	AudioSessionSetActive(true);
#endif

    p_aout->sys->b_started = true;

    p_aout->time_get = TimeGet;
    p_aout->play = Play;
    p_aout->pause = Pause;
    p_aout->flush = Flush;
    return VLC_SUCCESS;
}

static void Stop(audio_output_t *p_aout)
{
    AudioSessionSetActive(false);

    p_aout->sys->i_played_length = 0;
    AudioQueueDisposeTimeline(p_aout->sys->audioQueueRef, p_aout->sys->timelineRef);
    AudioQueueStop(p_aout->sys->audioQueueRef, true);
    AudioQueueDispose(p_aout->sys->audioQueueRef, true);
    msg_Dbg(p_aout, "audioqueue stopped and disposed");
}

#pragma mark -
#pragma mark actual playback

static void Play(audio_output_t *p_aout, block_t *p_block)
{
    AudioQueueBufferRef inBuffer = NULL;
    OSStatus status;

    status = AudioQueueAllocateBuffer(p_aout->sys->audioQueueRef, p_block->i_buffer, &inBuffer);
    if (status == noErr) {
        memcpy(inBuffer->mAudioData, p_block->p_buffer, p_block->i_buffer);
        inBuffer->mAudioDataByteSize = p_block->i_buffer;

        status = AudioQueueEnqueueBuffer(p_aout->sys->audioQueueRef, inBuffer, 0, NULL);
        if (status == noErr)
            p_aout->sys->i_played_length += p_block->i_length;
        else
            msg_Err(p_aout, "enqueuing buffer failed (%li)", status);
    } else
            msg_Err(p_aout, "buffer alloction failed (%li)", status);

    block_Release(p_block);
}

void UnusedAudioQueueCallback(void * inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
    /* this function does nothing, but needs to be here to make the AudioQueue API happy.
     * additionally, we clean-up after empty buffers */
    VLC_UNUSED(inUserData);
    AudioQueueFreeBuffer(inAQ, inBuffer);
}

static void Pause(audio_output_t *p_aout, bool pause, mtime_t date)
{
    VLC_UNUSED(date);

    if (pause) {
        AudioQueuePause(p_aout->sys->audioQueueRef);
        AudioSessionSetActive(false);
    } else {
        AudioQueueStart(p_aout->sys->audioQueueRef, NULL);
        AudioSessionSetActive(true);
    }
}

static void Flush(audio_output_t *p_aout, bool wait)
{
    if (!p_aout->sys->audioQueueRef)
        return;

    AudioQueueDisposeTimeline(p_aout->sys->audioQueueRef, p_aout->sys->timelineRef);

    if (wait)
        AudioQueueStop(p_aout->sys->audioQueueRef, false);
    else
        AudioQueueStop(p_aout->sys->audioQueueRef, true);

    p_aout->sys->b_started = false;
    p_aout->sys->i_played_length = 0;
    AudioQueueStart(p_aout->sys->audioQueueRef, NULL);
    AudioQueueCreateTimeline(p_aout->sys->audioQueueRef, &p_aout->sys->timelineRef);
    p_aout->sys->b_started = true;
}

static int TimeGet(audio_output_t *p_aout, mtime_t *restrict delay)
{
    AudioTimeStamp outTimeStamp;
    Boolean b_discontinuity;
    OSStatus status = AudioQueueGetCurrentTime(p_aout->sys->audioQueueRef, p_aout->sys->timelineRef, &outTimeStamp, &b_discontinuity);

    if (status != noErr)
        return -1;

    bool b_started = p_aout->sys->b_started;

    if (!b_started)
        return -1;

    if (b_discontinuity) {
        msg_Dbg(p_aout, "detected output discontinuity");
        return -1;
    }

    mtime_t i_pos = (mtime_t) outTimeStamp.mSampleTime * CLOCK_FREQ / p_aout->sys->i_rate;
    if (i_pos > 0) {
        *delay = p_aout->sys->i_played_length - i_pos;
        return 0;
    } else
        return -1;
}
