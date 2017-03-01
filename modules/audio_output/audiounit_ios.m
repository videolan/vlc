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

#import <CoreAudio/CoreAudioTypes.h>
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
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
    bool      b_muted;
};

#pragma mark -
#pragma mark AVAudioSession route and output handling

static int
avas_SetActive(audio_output_t *p_aout, bool active, NSUInteger options)
{
    struct aout_sys_t * p_sys = p_aout->sys;

    AVAudioSession *instance = [AVAudioSession sharedInstance];
    BOOL ret = false;
    NSError *error = nil;

    if (active)
    {
        ret = [instance setCategory:AVAudioSessionCategoryPlayback error:&error];
        ret = ret && [instance setMode:AVAudioSessionModeMoviePlayback error:&error];
        ret = ret && [instance setActive:YES withOptions:options error:&error];
    }
    else
        ret = [instance setActive:NO withOptions:options error:&error];

    if (!ret)
    {
        msg_Err(p_aout, "AVAudioSession playback change failed: %s(%d)",
                error.domain.UTF8String, (int)error.code);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

#pragma mark -
#pragma mark actual playback

static void
Pause (audio_output_t *p_aout, bool pause, mtime_t date)
{
    struct aout_sys_t * p_sys = p_aout->sys;

    /* We need to start / stop the audio unit here because otherwise the OS
     * won't believe us that we stopped the audio output so in case of an
     * interruption, our unit would be permanently silenced. In case of
     * multi-tasking, the multi-tasking view would still show a playing state
     * despite we are paused, same for lock screen */

    OSStatus err;
    if (pause)
    {
        err = AudioOutputUnitStop(p_sys->au_unit);
        if (err != noErr)
            msg_Err(p_aout, "AudioOutputUnitStart failed [%4.4s]",
                    (const char *) &err);
        avas_SetActive(p_aout, false, 0);
    }
    else
    {
        if (avas_SetActive(p_aout, true, 0) == VLC_SUCCESS)
        {
            err = AudioOutputUnitStart(p_sys->au_unit);
            if (err != noErr)
            {
                msg_Err(p_aout, "AudioOutputUnitStart failed [%4.4s]",
                        (const char *) &err);
                /* Do not un-pause, the Render Callback won't run, and next call
                 * of ca_Play will deadlock */
                return;
            }
        }
    }
    ca_Pause(p_aout, pause, date);
}

static int
MuteSet(audio_output_t *p_aout, bool mute)
{
    struct aout_sys_t * p_sys = p_aout->sys;

    p_sys->b_muted = mute;
    if (p_sys->au_unit != NULL)
    {
        Pause(p_aout, mute, 0);
        if (mute)
            ca_Flush(p_aout, false);
    }

    return VLC_SUCCESS;
}

static void
Play(audio_output_t * p_aout, block_t * p_block)
{
    struct aout_sys_t * p_sys = p_aout->sys;

    if (p_sys->b_muted)
        block_Release(p_block);
    else
        ca_Play(p_aout, p_block);
}

#pragma mark initialization

static void
Stop(audio_output_t *p_aout)
{
    struct aout_sys_t   *p_sys = p_aout->sys;
    OSStatus err;

    err = AudioOutputUnitStop(p_sys->au_unit);
    if (err != noErr)
        msg_Warn(p_aout, "AudioOutputUnitStop failed [%4.4s]",
                 (const char *)&err);

    err = AudioUnitUninitialize(p_sys->au_unit);
    if (err != noErr)
        msg_Warn(p_aout, "AudioUnitUninitialize failed [%4.4s]",
                 (const char *)&err);

    err = AudioComponentInstanceDispose(p_sys->au_unit);
    if (err != noErr)
        msg_Warn(p_aout, "AudioComponentInstanceDispose failed [%4.4s]",
                 (const char *)&err);

    avas_SetActive(p_aout, false,
                   AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation);

    ca_Clean(p_aout);
}

static int
Start(audio_output_t *p_aout, audio_sample_format_t *restrict fmt)
{
    struct aout_sys_t *p_sys = p_aout->sys;
    OSStatus err;

    if (aout_FormatNbChannels(fmt) == 0)
        return VLC_EGENERIC;

    aout_FormatPrint(p_aout, "VLC is looking for:", fmt);

    p_sys->au_unit = NULL;

    /* Activate the AVAudioSession */
    if (avas_SetActive(p_aout, true, 0) != VLC_SUCCESS)
        return VLC_EGENERIC;

    fmt->i_format = VLC_CODEC_FL32;
    fmt->i_physical_channels = fmt->i_original_channels = AOUT_CHANS_STEREO;

    p_sys->au_unit = au_NewOutputInstance(p_aout, kAudioUnitSubType_RemoteIO);
    if (p_sys->au_unit == NULL)
        goto error;

    err = AudioUnitSetProperty(p_sys->au_unit,
                               kAudioOutputUnitProperty_EnableIO,
                               kAudioUnitScope_Output, 0,
                               &(UInt32){ 1 }, sizeof(UInt32));
    if (err != noErr)
        msg_Warn(p_aout, "failed to set IO mode [%4.4s]", (const char *)&err);

    int ret = au_Initialize(p_aout, p_sys->au_unit, fmt, NULL);
    if (ret != VLC_SUCCESS)
        goto error;

    ret = ca_Init(p_aout, fmt, AUDIO_BUFFER_SIZE_IN_SECONDS * fmt->i_rate *
                  fmt->i_bytes_per_frame);
    if (ret != VLC_SUCCESS)
    {
        AudioUnitUninitialize(p_sys->au_unit);
        goto error;
    }
    p_aout->play = Play;

    err = AudioOutputUnitStart(p_sys->au_unit);
    if (err != noErr)
    {
        msg_Err(p_aout, "AudioOutputUnitStart failed [%4.4s]",
                (const char *) &err);
        AudioUnitUninitialize(p_sys->au_unit);
        ca_Clean(p_aout);
        goto error;
    }

    if (p_sys->b_muted)
        Pause(p_aout, true, 0);

    p_aout->mute_set  = MuteSet;
    p_aout->pause = Pause;
    msg_Dbg(p_aout, "analog AudioUnit output successfully opened");
    return VLC_SUCCESS;

error:
    avas_SetActive(p_aout, false,
                   AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation);
    AudioComponentInstanceDispose(p_sys->au_unit);
    msg_Err(p_aout, "opening AudioUnit output failed");
    return VLC_EGENERIC;
}

static void
Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    free(sys);
}

static int
Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = calloc(1, sizeof (*sys));

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->b_muted = false;
    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;

    return VLC_SUCCESS;
}
