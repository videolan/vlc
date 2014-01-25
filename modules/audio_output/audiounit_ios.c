/*****************************************************************************
 * audiounit_ios.c: AudioUnit output plugin for iOS
 *****************************************************************************
 * Copyright (C) 2012 - 2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
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

#import <AudioUnit/AudioUnit.h>
#import <CoreAudio/CoreAudioTypes.h>
#import <AudioToolbox/AudioToolbox.h>
#import <mach/mach_time.h>

#import "TPCircularBuffer.h"

#pragma mark -
#pragma mark private declarations

#define STREAM_FORMAT_MSG(pre, sfm) \
    pre "[%f][%4.4s][%u][%u][%u][%u][%u][%u]", \
    sfm.mSampleRate, (char *)&sfm.mFormatID, \
    (unsigned int)sfm.mFormatFlags, (unsigned int)sfm.mBytesPerPacket, \
    (unsigned int)sfm.mFramesPerPacket, (unsigned int)sfm.mBytesPerFrame, \
    (unsigned int)sfm.mChannelsPerFrame, (unsigned int)sfm.mBitsPerChannel

#define AUDIO_BUFFER_SIZE_IN_SECONDS (AOUT_MAX_ADVANCE_TIME / CLOCK_FREQ)

/*****************************************************************************
 * aout_sys_t: private audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the CoreAudio specific properties of an output thread.
 *****************************************************************************/
struct aout_sys_t
{
    TPCircularBuffer            circular_buffer;    /* circular buffer to swap the audio data */

    /* AUHAL specific */
    AudioComponent              au_component;       /* The AudioComponent we use */
    AudioUnit                   au_unit;            /* The AudioUnit we use */

    int                         i_rate;             /* media sample rate */
    int                         i_bytes_per_sample;

    bool                        b_paused;

    vlc_mutex_t                 lock;
    vlc_cond_t                  cond;
};

#pragma mark -
#pragma mark local prototypes & module descriptor

static int      Open                    (vlc_object_t *);
static void     Close                   (vlc_object_t *);
static int      Start                   (audio_output_t *, audio_sample_format_t *);
static int      StartAnalog             (audio_output_t *, audio_sample_format_t *);
static void     Stop                    (audio_output_t *);

static void     Play                    (audio_output_t *, block_t *);
static void     Pause                   (audio_output_t *, bool, mtime_t);
static void     Flush                   (audio_output_t *, bool);
static int      TimeGet                 (audio_output_t *, mtime_t *);
static OSStatus RenderCallback    (vlc_object_t *, AudioUnitRenderActionFlags *, const AudioTimeStamp *,
                                         UInt32 , UInt32, AudioBufferList *);

vlc_module_begin ()
    set_shortname("audiounit_ios")
    set_description(N_("AudioUnit output for iOS"))
    set_capability("audio output", 101)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
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

    vlc_mutex_init(&sys->lock);
    vlc_cond_init(&sys->cond);
    sys->b_paused = false;

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    vlc_mutex_destroy(&sys->lock);
    vlc_cond_destroy(&sys->cond);

    free(sys);
}

static int Start(audio_output_t *p_aout, audio_sample_format_t *restrict fmt)
{
    struct aout_sys_t *p_sys = NULL;

    p_sys = p_aout->sys;
    p_sys->au_component = NULL;
    p_sys->au_unit = NULL;
    p_sys->i_bytes_per_sample = 0;

    aout_FormatPrint(p_aout, "VLC is looking for:", fmt);

    if (StartAnalog(p_aout, fmt)) {
        msg_Dbg(p_aout, "analog AudioUnit output successfully opened");
        p_aout->play = Play;
        p_aout->flush = Flush;
        p_aout->time_get = TimeGet;
        p_aout->pause = Pause;
        return VLC_SUCCESS;
    }

    /* If we reach this, this aout has failed */
    msg_Err(p_aout, "opening AudioUnit output failed");
    return VLC_EGENERIC;
}

/*
 * StartAnalog: open and setup a HAL AudioUnit to do PCM audio output
 */
static int StartAnalog(audio_output_t *p_aout, audio_sample_format_t *fmt)
{
    struct aout_sys_t           *p_sys = p_aout->sys;
    UInt32                      i_param_size = 0;
    AudioComponentDescription   desc;
    AURenderCallbackStruct      callback;
    OSStatus status;

    /* Lets go find our Component */
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    p_sys->au_component = AudioComponentFindNext(NULL, &desc);
    if (p_sys->au_component == NULL) {
        msg_Warn(p_aout, "we cannot find our audio component");
        return false;
    }

    status = AudioComponentInstanceNew(p_sys->au_component, &p_sys->au_unit);
    if (status != noErr) {
        msg_Warn(p_aout, "we cannot open our audio component (%li)", status);
        return false;
    }

    UInt32 flag = 1;
    status = AudioUnitSetProperty(p_sys->au_unit,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Output,
                                  0,
                                  &flag,
                                  sizeof(flag));
    if (status != noErr)
        msg_Warn(p_aout, "failed to set IO mode (%li)", status);

    /* Get the current format */
    AudioStreamBasicDescription streamDescription;
    streamDescription.mSampleRate = fmt->i_rate;
    fmt->i_format = VLC_CODEC_FL32;
    fmt->i_physical_channels = AOUT_CHANS_STEREO;
    streamDescription.mFormatID = kAudioFormatLinearPCM;
    streamDescription.mFormatFlags = kAudioFormatFlagsNativeFloatPacked; // FL32
    streamDescription.mChannelsPerFrame = aout_FormatNbChannels(fmt);
    streamDescription.mFramesPerPacket = 1;
    streamDescription.mBitsPerChannel = 32;
    streamDescription.mBytesPerFrame = streamDescription.mBitsPerChannel * streamDescription.mChannelsPerFrame / 8;
    streamDescription.mBytesPerPacket = streamDescription.mBytesPerFrame * streamDescription.mFramesPerPacket;
    i_param_size = sizeof(streamDescription);
    p_sys->i_rate = fmt->i_rate;

    /* Set the desired format */
    i_param_size = sizeof(AudioStreamBasicDescription);
    status = AudioUnitSetProperty(p_sys->au_unit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  0,
                                  &streamDescription,
                                  i_param_size);
    if (status != noErr) {
        msg_Err(p_aout, "failed to set stream format (%li)", status);
        return false;
    }
    msg_Dbg(p_aout, STREAM_FORMAT_MSG("we set the AU format: " , streamDescription));

    /* Retrieve actual format */
    status = AudioUnitGetProperty(p_sys->au_unit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  0,
                                  &streamDescription,
                                  &i_param_size);
    if (status != noErr)
        msg_Warn(p_aout, "failed to verify stream format (%li)", status);
    msg_Dbg(p_aout, STREAM_FORMAT_MSG("the actual set AU format is " , streamDescription));

    /* Do the last VLC aout setups */
    aout_FormatPrepare(fmt);

    /* set the IOproc callback */
    callback.inputProc = (AURenderCallback) RenderCallback;
    callback.inputProcRefCon = p_aout;

    status = AudioUnitSetProperty(p_sys->au_unit,
                            kAudioUnitProperty_SetRenderCallback,
                            kAudioUnitScope_Input,
                            0, &callback, sizeof(callback));
    if (status != noErr) {
        msg_Err(p_aout, "render callback setup failed (%li)", status);
        return false;
    }

    /* AU init */
    status = AudioUnitInitialize(p_sys->au_unit);
    if (status != noErr) {
        msg_Err(p_aout, "failed to init AudioUnit (%li)", status);
        return false;
    }

    /* setup circular buffer */
    TPCircularBufferInit(&p_sys->circular_buffer, AUDIO_BUFFER_SIZE_IN_SECONDS * fmt->i_rate * fmt->i_bytes_per_frame);

    /* start audio session so playback continues if mute switch is on */
    AudioSessionInitialize (NULL,
                            kCFRunLoopCommonModes,
                            NULL,
                            NULL);

	/* Set audio session to mediaplayback */
	UInt32 sessionCategory = kAudioSessionCategory_MediaPlayback;
	AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(sessionCategory),&sessionCategory);
	AudioSessionSetActive(true);

    status = AudioOutputUnitStart(p_sys->au_unit);
    msg_Dbg(p_aout, "audio output unit started: %li", status);

    return true;
}

static void Stop(audio_output_t *p_aout)
{
    struct aout_sys_t   *p_sys = p_aout->sys;
    OSStatus status;

    AudioSessionSetActive(false);

    if (p_sys->au_unit) {
        status = AudioOutputUnitStop(p_sys->au_unit);
        if (status != noErr)
            msg_Warn(p_aout, "failed to stop AudioUnit (%li)", status);

        status = AudioComponentInstanceDispose(p_sys->au_unit);
        if (status != noErr)
            msg_Warn(p_aout, "failed to dispose Audio Component instance (%li)", status);
    }
    p_sys->i_bytes_per_sample = 0;

    /* clean-up circular buffer */
    TPCircularBufferCleanup(&p_sys->circular_buffer);
}

#pragma mark -
#pragma mark actual playback

static void Play (audio_output_t * p_aout, block_t * p_block)
{
    struct aout_sys_t *p_sys = p_aout->sys;

    if (p_block->i_nb_samples > 0) {
        /* move data to buffer */
        if (unlikely(!TPCircularBufferProduceBytes(&p_sys->circular_buffer, p_block->p_buffer, p_block->i_buffer)))
            msg_Warn(p_aout, "Audio buffer was dropped");

        if (!p_sys->i_bytes_per_sample)
            p_sys->i_bytes_per_sample = p_block->i_buffer / p_block->i_nb_samples;
    }

    block_Release(p_block);
}

static void Pause (audio_output_t *p_aout, bool pause, mtime_t date)
{
    struct aout_sys_t * p_sys = p_aout->sys;
    VLC_UNUSED(date);

    vlc_mutex_lock(&p_sys->lock);
    p_sys->b_paused = pause;
    vlc_mutex_unlock(&p_sys->lock);

    /* we need to start / stop the audio unit here because otherwise
     * the OS won't believe us that we stopped the audio output
     * so in case of an interruption, our unit would be permanently
     * silenced.
     * in case of multi-tasking, the multi-tasking view would still
     * show a playing state despite we are paused, same for lock screen */
    if (pause) {
        AudioOutputUnitStop(p_sys->au_unit);
        AudioSessionSetActive(false);
    } else {
        AudioOutputUnitStart(p_sys->au_unit);
        UInt32 sessionCategory = kAudioSessionCategory_MediaPlayback;
        AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(sessionCategory),&sessionCategory);
        AudioSessionSetActive(true);
    }
}

static void Flush(audio_output_t *p_aout, bool wait)
{
    struct aout_sys_t *p_sys = p_aout->sys;

    int32_t availableBytes;
    vlc_mutex_lock(&p_sys->lock);
    TPCircularBufferTail(&p_sys->circular_buffer, &availableBytes);

    if (wait) {
        while (availableBytes > 0) {
            vlc_cond_wait(&p_sys->cond, &p_sys->lock);
            TPCircularBufferTail(&p_sys->circular_buffer, &availableBytes);
        }
    } else {
        /* flush circular buffer if data is left */
        if (availableBytes > 0)
            TPCircularBufferClear(&p_aout->sys->circular_buffer);
    }

    vlc_mutex_unlock(&p_sys->lock);
}

static int TimeGet(audio_output_t *p_aout, mtime_t *delay)
{
    struct aout_sys_t * p_sys = p_aout->sys;

    if (!p_sys->i_bytes_per_sample)
        return -1;

    int32_t availableBytes;
    TPCircularBufferTail(&p_sys->circular_buffer, &availableBytes);

    *delay = (availableBytes / p_sys->i_bytes_per_sample) * CLOCK_FREQ / p_sys->i_rate;

    return 0;
}

/*****************************************************************************
 * RenderCallback: This function is called everytime the AudioUnit wants
 * us to provide some more audio data.
 * Don't print anything during normal playback, calling blocking function from
 * this callback is not allowed.
 *****************************************************************************/
static OSStatus RenderCallback(vlc_object_t *p_obj,
                               AudioUnitRenderActionFlags *ioActionFlags,
                               const AudioTimeStamp *inTimeStamp,
                               UInt32 inBusNumber,
                               UInt32 inNumberFrames,
                               AudioBufferList *ioData) {
    VLC_UNUSED(ioActionFlags);
    VLC_UNUSED(inTimeStamp);
    VLC_UNUSED(inBusNumber);
    VLC_UNUSED(inNumberFrames);

    audio_output_t * p_aout = (audio_output_t *)p_obj;
    struct aout_sys_t * p_sys = p_aout->sys;

    int bytesRequested = ioData->mBuffers[0].mDataByteSize;
    Float32 *targetBuffer = (Float32*)ioData->mBuffers[0].mData;

    vlc_mutex_lock(&p_sys->lock);
    /* Pull audio from buffer */
    int32_t availableBytes;
    Float32 *buffer = TPCircularBufferTail(&p_sys->circular_buffer, &availableBytes);

    /* check if we have enough data */
    if (!availableBytes || p_sys->b_paused) {
        /* return an empty buffer so silence is played until we have data */
        memset(targetBuffer, 0, ioData->mBuffers[0].mDataByteSize);
    } else {
        int32_t bytesToCopy = __MIN(bytesRequested, availableBytes);

        if (likely(bytesToCopy > 0)) {
            memcpy(targetBuffer, buffer, bytesToCopy);
            TPCircularBufferConsume(&p_sys->circular_buffer, bytesToCopy);
            ioData->mBuffers[0].mDataByteSize = bytesToCopy;
        }
    }

    vlc_cond_signal(&p_sys->cond);
    vlc_mutex_unlock(&p_sys->lock);

    return noErr;
}

