/*****************************************************************************
 * audiounit_ios.m: AudioUnit output plugin for iOS
 *****************************************************************************
 * Copyright (C) 2012 - 2017 VLC authors and VideoLAN
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

#import "coreaudio_common.h"

#import <vlc_plugin.h>

#import <AudioUnit/AudioUnit.h>
#import <CoreAudio/CoreAudioTypes.h>
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <mach/mach_time.h>

#pragma mark -
#pragma mark local prototypes & module descriptor

static int  Open  (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname("audiounit_ios")
    set_description("AudioUnit output for iOS")
    set_capability("audio output", 101)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    set_callbacks(Open, Close)
vlc_module_end ()

#pragma mark -
#pragma mark private declarations

#define AUDIO_BUFFER_SIZE_IN_SECONDS (AOUT_MAX_ADVANCE_TIME / CLOCK_FREQ)

/*****************************************************************************
 * aout_sys_t: private audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the CoreAudio specific properties of an output thread.
 *****************************************************************************/
struct aout_sys_t
{
    struct aout_sys_common c;

    /* The AudioUnit we use */
    AudioUnit au_unit;
};

#pragma mark -
#pragma mark actual playback

static int SetPlayback(audio_output_t *p_aout, bool start)
{
    struct aout_sys_t * p_sys = p_aout->sys;

    AVAudioSession *instance = [AVAudioSession sharedInstance];
    OSStatus err;
    BOOL ret = false;
    NSError *error = nil;

    if (start)
    {
        err = AudioOutputUnitStart(p_sys->au_unit);
        if (err != noErr)
            goto error;

        ret = [instance setCategory:AVAudioSessionCategoryPlayback error:&error];
        ret = ret && [instance setMode:AVAudioSessionModeMoviePlayback error:&error];
        ret = ret && [instance setActive:YES error:&error];
    }
    else
    {
        err = AudioOutputUnitStop(p_sys->au_unit);
        if (err != noErr)
            goto error;
        ret = [instance setActive:NO error:&error];
    }
    if (!ret)
        goto error;

    return VLC_SUCCESS;

error:
    if (err != noErr)
    {
        msg_Err(p_aout, "AudioOutputUnit%s failed [%4.4s]",
                start ? "Start" : "Stop", (const char *) &err);
    }
    else
    {
        if (start)
            AudioOutputUnitStop(p_sys->au_unit);
        msg_Err(p_aout, "AVAudioSession playback change failed: %s(%d)",
                error.domain.UTF8String, (int)error.code);
    }
    return VLC_EGENERIC;
}

static void Pause (audio_output_t *p_aout, bool pause, mtime_t date)
{
    VLC_UNUSED(date);

    /* we need to start / stop the audio unit here because otherwise
     * the OS won't believe us that we stopped the audio output
     * so in case of an interruption, our unit would be permanently
     * silenced.
     * in case of multi-tasking, the multi-tasking view would still
     * show a playing state despite we are paused, same for lock screen */

    SetPlayback(p_aout, !pause);
}

static int MuteSet(audio_output_t *p_aout, bool mute)
{
    struct aout_sys_t * p_sys = p_aout->sys;

    if (p_sys != NULL && p_sys->au_unit != NULL) {
        msg_Dbg(p_aout, "audio output mute set to %d", mute?1:0);
        Pause(p_aout, mute, 0);
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * RenderCallback: This function is called everytime the AudioUnit wants
 * us to provide some more audio data.
 * Don't print anything during normal playback, calling blocking function from
 * this callback is not allowed.
 *****************************************************************************/
static OSStatus RenderCallback(void *p_data,
                               AudioUnitRenderActionFlags *ioActionFlags,
                               const AudioTimeStamp *inTimeStamp,
                               UInt32 inBusNumber,
                               UInt32 inNumberFrames,
                               AudioBufferList *ioData) {
    VLC_UNUSED(ioActionFlags);
    VLC_UNUSED(inTimeStamp);
    VLC_UNUSED(inBusNumber);
    VLC_UNUSED(inNumberFrames);

    ca_Render(p_data, ioData->mBuffers[0].mData,
              ioData->mBuffers[0].mDataByteSize);

    return noErr;
}

#pragma mark initialization

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

    AudioComponent au_component = AudioComponentFindNext(NULL, &desc);
    if (au_component == NULL) {
        msg_Warn(p_aout, "we cannot find our audio component");
        return VLC_EGENERIC;
    }

    status = AudioComponentInstanceNew(au_component, &p_sys->au_unit);
    if (status != noErr) {
        msg_Warn(p_aout, "we cannot open our audio component (%i)", (int)status);
        return VLC_EGENERIC;
    }

    UInt32 flag = 1;
    status = AudioUnitSetProperty(p_sys->au_unit,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Output,
                                  0,
                                  &flag,
                                  sizeof(flag));
    if (status != noErr)
        msg_Warn(p_aout, "failed to set IO mode (%i)", (int)status);

    /* Get the current format */
    AudioStreamBasicDescription streamDescription;
    streamDescription.mSampleRate = fmt->i_rate;
    fmt->i_format = VLC_CODEC_FL32;
    fmt->i_physical_channels = fmt->i_original_channels = AOUT_CHANS_STEREO;
    streamDescription.mFormatID = kAudioFormatLinearPCM;
    streamDescription.mFormatFlags = kAudioFormatFlagsNativeFloatPacked; // FL32
    streamDescription.mChannelsPerFrame = aout_FormatNbChannels(fmt);
    streamDescription.mFramesPerPacket = 1;
    streamDescription.mBitsPerChannel = 32;
    streamDescription.mBytesPerFrame = streamDescription.mBitsPerChannel * streamDescription.mChannelsPerFrame / 8;
    streamDescription.mBytesPerPacket = streamDescription.mBytesPerFrame * streamDescription.mFramesPerPacket;
    i_param_size = sizeof(streamDescription);

    /* Set the desired format */
    i_param_size = sizeof(AudioStreamBasicDescription);
    status = AudioUnitSetProperty(p_sys->au_unit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  0,
                                  &streamDescription,
                                  i_param_size);
    if (status != noErr) {
        msg_Err(p_aout, "failed to set stream format (%i)", (int)status);
        goto error;
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
        msg_Warn(p_aout, "failed to verify stream format (%i)", (int)status);
    msg_Dbg(p_aout, STREAM_FORMAT_MSG("the actual set AU format is " , streamDescription));

    /* Do the last VLC aout setups */
    aout_FormatPrepare(fmt);

    /* set the IOproc callback */
    callback.inputProc = RenderCallback;
    callback.inputProcRefCon = p_aout;

    status = AudioUnitSetProperty(p_sys->au_unit,
                            kAudioUnitProperty_SetRenderCallback,
                            kAudioUnitScope_Input,
                            0, &callback, sizeof(callback));
    if (status != noErr) {
        msg_Err(p_aout, "render callback setup failed (%i)", (int)status);
        goto error;
    }

    /* AU init */
    status = AudioUnitInitialize(p_sys->au_unit);
    if (status != noErr) {
        msg_Err(p_aout, "failed to init AudioUnit (%i)", (int)status);
        goto error;
    }

    int ret = ca_Init(p_aout, fmt, AUDIO_BUFFER_SIZE_IN_SECONDS * fmt->i_rate *
                      fmt->i_bytes_per_frame);
    if (ret != VLC_SUCCESS)
    {
        AudioUnitUninitialize(p_sys->au_unit);
        goto error;
    }

    /* start the unit */
    if (SetPlayback(p_aout, true) != VLC_SUCCESS)
    {
        AudioUnitUninitialize(p_sys->au_unit);
        ca_Clean(p_aout);
        goto error;
    }

    return VLC_SUCCESS;

error:
    AudioComponentInstanceDispose(p_sys->au_unit);
    return VLC_EGENERIC;
}

static void Stop(audio_output_t *p_aout)
{
    struct aout_sys_t   *p_sys = p_aout->sys;
    OSStatus status;

    if (p_sys->au_unit) {
        status = AudioOutputUnitStop(p_sys->au_unit);
        if (status != noErr)
            msg_Warn(p_aout, "failed to stop AudioUnit (%i)", (int)status);

        status = AudioUnitUninitialize(p_sys->au_unit);
        if (status != noErr)
            msg_Warn(p_aout, "failed to uninit AudioUnit (%i)", (int)status);

        status = AudioComponentInstanceDispose(p_sys->au_unit);
        if (status != noErr)
            msg_Warn(p_aout, "failed to dispose Audio Component instance (%i)", (int)status);
    }

    [[AVAudioSession sharedInstance] setActive:NO withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation error:nil];

    ca_Clean(p_aout);
}

static int Start(audio_output_t *p_aout, audio_sample_format_t *restrict fmt)
{
    struct aout_sys_t *p_sys = NULL;

    if (aout_FormatNbChannels(fmt) == 0)
        return VLC_EGENERIC;

    p_sys = p_aout->sys;
    p_sys->au_unit = NULL;

    aout_FormatPrint(p_aout, "VLC is looking for:", fmt);

    if (StartAnalog(p_aout, fmt) == VLC_SUCCESS) {
        msg_Dbg(p_aout, "analog AudioUnit output successfully opened");
        p_aout->mute_set  = MuteSet;
        p_aout->pause = Pause;

        return VLC_SUCCESS;
    }

    /* If we reach this, this aout has failed */
    msg_Err(p_aout, "opening AudioUnit output failed");
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    free(sys);
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = calloc(1, sizeof (*sys));

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;

    return VLC_SUCCESS;
}
