/*****************************************************************************
 * auhal.c: AUHAL and Coreaudio output plugin
 *****************************************************************************
 * Copyright (C) 2005 - 2013 VLC authors and VideoLAN
 * $Id$
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

#pragma mark includes

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <vlc_common.h>
#import <vlc_plugin.h>
#import <vlc_dialog.h>                      // dialog_Fatal
#import <vlc_aout.h>                        // aout_*

#import <AudioUnit/AudioUnit.h>             // AudioUnit
#import <CoreAudio/CoreAudio.h>             // AudioDeviceID
#import <AudioToolbox/AudioFormat.h>        // AudioFormatGetProperty
#import <CoreServices/CoreServices.h>

#import "TPCircularBuffer.h"

#pragma mark -
#pragma mark private declarations

#ifndef verify_noerr
# define verify_noerr(a) assert((a) == noErr)
#endif

#define STREAM_FORMAT_MSG(pre, sfm) \
    pre "[%f][%4.4s][%u][%u][%u][%u][%u][%u]", \
    sfm.mSampleRate, (char *)&sfm.mFormatID, \
    (unsigned int)sfm.mFormatFlags, (unsigned int)sfm.mBytesPerPacket, \
    (unsigned int)sfm.mFramesPerPacket, (unsigned int)sfm.mBytesPerFrame, \
    (unsigned int)sfm.mChannelsPerFrame, (unsigned int)sfm.mBitsPerChannel

#define AOUT_VAR_SPDIF_FLAG 0xf00000

#define AUDIO_BUFFER_SIZE_IN_SECONDS (AOUT_MAX_ADVANCE_TIME / CLOCK_FREQ)


#define AOUT_VOLUME_DEFAULT             256
#define AOUT_VOLUME_MAX                 512

#define VOLUME_TEXT N_("Audio volume")
#define VOLUME_LONGTEXT VOLUME_TEXT

#define DEVICE_TEXT N_("Last audio device")
#define DEVICE_LONGTEXT DEVICE_TEXT

/*****************************************************************************
 * aout_sys_t: private audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the CoreAudio specific properties of an output thread.
 *****************************************************************************/
struct aout_sys_t
{
    AudioObjectID               i_selected_dev;     /* DeviceID of the selected device */
    AudioObjectID               i_new_selected_dev; /* DeviceID of device which will be selected on start */
    bool                        b_selected_dev_is_digital;
    AudioDeviceIOProcID         i_procID;           /* DeviceID of current device */
    bool                        b_digital;          /* Are we running in digital mode? */

    uint8_t                     chans_to_reorder;   /* do we need channel reordering */
    uint8_t                     chan_table[AOUT_CHAN_MAX];

    TPCircularBuffer            circular_buffer;    /* circular buffer to swap the audio data */

    /* AUHAL specific */
    AudioComponent              au_component;       /* The AudioComponent we use */
    AudioUnit                   au_unit;            /* The AudioUnit we use */

    /* CoreAudio SPDIF mode specific */
    pid_t                       i_hog_pid;          /* The keep the pid of our hog status */
    AudioStreamID               i_stream_id;        /* The StreamID that has a cac3 streamformat */
    int                         i_stream_index;     /* The index of i_stream_id in an AudioBufferList */
    AudioStreamBasicDescription stream_format;      /* The format we changed the stream to */
    AudioStreamBasicDescription sfmt_revert;        /* The original format of the stream */
    bool                        b_revert;           /* Whether we need to revert the stream format */
    bool                        b_changed_mixing;   /* Whether we need to set the mixing mode back */


    bool                        b_got_first_sample; /* did the aout core provide something to render? */

    int                         i_rate;             /* media sample rate */
    int                         i_bytes_per_sample;

    CFArrayRef                  device_list;

    vlc_mutex_t                 var_lock;           /* protects access to device_list and i_selected_dev */

    float                       f_volume;
    bool                        b_mute;

    vlc_mutex_t                 lock;
    vlc_cond_t                  cond;
};

#pragma mark -
#pragma mark local prototypes & module descriptor

static int      Open                    (vlc_object_t *);
static void     Close                   (vlc_object_t *);
static int      Start                   (audio_output_t *, audio_sample_format_t *);
static int      StartAnalog             (audio_output_t *, audio_sample_format_t *);
static int      StartSPDIF              (audio_output_t *, audio_sample_format_t *);
static void     Stop                    (audio_output_t *);

static void     RebuildDeviceList       (audio_output_t *);
static int      SwitchAudioDevice       (audio_output_t *p_aout, const char *name);
static int      VolumeSet               (audio_output_t *, float);
static int      MuteSet                 (audio_output_t *, bool);

static void     Play                    (audio_output_t *, block_t *);
static void     Pause                   (audio_output_t *, bool, mtime_t);
static void     Flush                   (audio_output_t *, bool);
static int      TimeGet                 (audio_output_t *, mtime_t *);
static OSStatus RenderCallbackAnalog    (vlc_object_t *, AudioUnitRenderActionFlags *, const AudioTimeStamp *,
                                         UInt32 , UInt32, AudioBufferList *);

static OSStatus RenderCallbackSPDIF     (AudioDeviceID, const AudioTimeStamp *, const void *, const AudioTimeStamp *,
                                         AudioBufferList *, const AudioTimeStamp *, void *);

static OSStatus DevicesListener         (AudioObjectID, UInt32, const AudioObjectPropertyAddress *, void *);
static OSStatus DeviceAliveListener     (AudioObjectID, UInt32, const AudioObjectPropertyAddress *, void *);
static OSStatus StreamsChangedListener  (AudioObjectID, UInt32, const AudioObjectPropertyAddress *, void *);

static OSStatus StreamListener          (AudioObjectID, UInt32, const AudioObjectPropertyAddress *, void *);

static int      ManageAudioStreamsCallback(audio_output_t *p_aout, AudioDeviceID i_dev_id, bool b_register);
static int      AudioDeviceHasOutput    (AudioDeviceID);
static int      AudioDeviceSupportsDigital(audio_output_t *, AudioDeviceID);
static int      AudioStreamSupportsDigital(audio_output_t *, AudioStreamID);
static int      AudioStreamChangeFormat (audio_output_t *, AudioStreamID, AudioStreamBasicDescription);


vlc_module_begin ()
    set_shortname("auhal")
    set_description(N_("HAL AudioUnit output"))
    set_capability("audio output", 101)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    set_callbacks(Open, Close)
    add_integer("auhal-volume", AOUT_VOLUME_DEFAULT,
                VOLUME_TEXT, VOLUME_LONGTEXT, true)
    change_integer_range(0, AOUT_VOLUME_MAX)
    add_string("auhal-audio-device", "", DEVICE_TEXT, DEVICE_LONGTEXT, true)
    add_obsolete_integer("macosx-audio-device") /* since 2.1.0 */
vlc_module_end ()

#pragma mark -
#pragma mark initialization

static int Open(vlc_object_t *obj)
{
    audio_output_t *p_aout = (audio_output_t *)obj;
    aout_sys_t *p_sys = malloc(sizeof (*p_sys));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    OSStatus err = noErr;

    vlc_mutex_init(&p_sys->var_lock);
    vlc_mutex_init(&p_sys->lock);
    vlc_cond_init(&p_sys->cond);
    p_sys->b_digital = false;

    p_aout->sys = p_sys;
    p_aout->start = Start;
    p_aout->stop = Stop;
    p_aout->volume_set = VolumeSet;
    p_aout->mute_set = MuteSet;
    p_aout->device_select = SwitchAudioDevice;
    p_sys->device_list = CFArrayCreate(kCFAllocatorDefault, NULL, 0, NULL);

    /* Attach a Listener so that we are notified of a change in the Device setup */
    AudioObjectPropertyAddress audioDevicesAddress = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &audioDevicesAddress, DevicesListener, (void *)p_aout);
    if (err != noErr)
        msg_Err(p_aout, "failed to add listener for audio device configuration [%4.4s]", (char *)&err);

    RebuildDeviceList(p_aout);

    /* remember the volume */
    p_sys->f_volume = var_InheritInteger(p_aout, "auhal-volume") / (float)AOUT_VOLUME_DEFAULT;
    aout_VolumeReport(p_aout, p_sys->f_volume);
    p_sys->b_mute = var_InheritBool(p_aout, "mute");
    aout_MuteReport(p_aout, p_sys->b_mute);

    char *psz_audio_device = config_GetPsz(p_aout, "auhal-audio-device");
    SwitchAudioDevice(p_aout, psz_audio_device);
    free(psz_audio_device);

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *p_aout = (audio_output_t *)obj;
    aout_sys_t *p_sys = p_aout->sys;

    OSStatus err = noErr;

    /* remove audio devices callback */
    AudioObjectPropertyAddress audioDevicesAddress = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    err = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &audioDevicesAddress, DevicesListener, (void *)p_aout);
    if (err != noErr)
        msg_Err(p_aout, "AudioHardwareRemovePropertyListener failed [%4.4s]", (char *)&err);

    vlc_mutex_lock(&p_sys->var_lock);
    /* remove streams callbacks */
    CFIndex count = CFArrayGetCount(p_sys->device_list);
    if (count > 0) {
        for (CFIndex x = 0; x < count; x++) {
            AudioDeviceID deviceId = 0;
            CFNumberRef cfn_device_id = CFArrayGetValueAtIndex(p_sys->device_list, x);
            if (!cfn_device_id)
                continue;

            CFNumberGetValue(cfn_device_id, kCFNumberSInt32Type, &deviceId);
            if (!(deviceId & AOUT_VAR_SPDIF_FLAG)) {
                ManageAudioStreamsCallback(p_aout, deviceId, false);
            }
        }
    }

    CFRelease(p_sys->device_list);
    vlc_mutex_unlock(&p_sys->var_lock);

    char *psz_device = aout_DeviceGet(p_aout);
    config_PutPsz(p_aout, "auhal-audio-device", psz_device);
    free(psz_device);

    vlc_mutex_destroy(&p_sys->var_lock);
    vlc_mutex_destroy(&p_sys->lock);
    vlc_cond_destroy(&p_sys->cond);

    free(p_sys);
}

static int Start(audio_output_t *p_aout, audio_sample_format_t *restrict fmt)
{
    OSStatus                err = noErr;
    UInt32                  i_param_size = 0;
    struct aout_sys_t       *p_sys = NULL;

    /* Use int here, to match kAudioDevicePropertyDeviceIsAlive
     * property size */
    int                     b_alive = false;

    bool                    b_start_digital = false;

    p_sys = p_aout->sys;
    p_sys->b_digital = false;
    p_sys->au_component = NULL;
    p_sys->au_unit = NULL;
    p_sys->i_hog_pid = -1;
    p_sys->i_stream_id = 0;
    p_sys->i_stream_index = -1;
    p_sys->b_revert = false;
    p_sys->b_changed_mixing = false;
    p_sys->i_bytes_per_sample = 0;

    vlc_mutex_lock(&p_sys->var_lock);
    p_sys->i_selected_dev = p_sys->i_new_selected_dev;

    aout_FormatPrint(p_aout, "VLC is looking for:", fmt);

    msg_Dbg(p_aout, "attempting to use device %i", p_sys->i_selected_dev);

    AudioObjectPropertyAddress audioDeviceAliveAddress = { kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    if (p_sys->i_selected_dev > 0) {
        /* Check if the desired device is alive and usable */
        i_param_size = sizeof(b_alive);
        err = AudioObjectGetPropertyData(p_sys->i_selected_dev, &audioDeviceAliveAddress, 0, NULL, &i_param_size, &b_alive);
        if (err != noErr) {
            /* Be tolerant, only give a warning here */
            msg_Warn(p_aout, "could not check whether device [0x%x] is alive [%4.4s]",
                     (unsigned int)p_sys->i_selected_dev, (char *)&err);
            b_alive = false;
        }

        if (!b_alive)
            msg_Warn(p_aout, "selected audio device is not alive, switching to default device");
    }

    if (!b_alive || p_sys->i_selected_dev == 0) {
        AudioObjectID defaultDeviceID = 0;
        UInt32 propertySize = 0;
        AudioObjectPropertyAddress defaultDeviceAddress = { kAudioHardwarePropertyDefaultOutputDevice, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
        propertySize = sizeof(AudioObjectID);
        err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &defaultDeviceAddress, 0, NULL, &propertySize, &defaultDeviceID);
        if (err != noErr) {
            msg_Err(p_aout, "could not get default audio device [%4.4s]", (char *)&err);
            goto error;
        }
        else
            msg_Dbg(p_aout, "using default audio device %i", defaultDeviceID);

        p_sys->i_selected_dev = defaultDeviceID;
        p_sys->b_selected_dev_is_digital = var_InheritBool(p_aout, "spdif");
    }

    // recheck if device still supports digital
    b_start_digital = p_sys->b_selected_dev_is_digital;
    if(!AudioDeviceSupportsDigital(p_aout, p_sys->i_selected_dev))
        b_start_digital = false;

    if (b_start_digital)
        msg_Dbg(p_aout, "Using audio device for digital output");
    else
        msg_Dbg(p_aout, "Audio device supports PCM mode only");

    /* add a callback to see if the device dies later on */
    err = AudioObjectAddPropertyListener(p_sys->i_selected_dev, &audioDeviceAliveAddress, DeviceAliveListener, (void *)p_aout);
    if (err != noErr) {
        /* Be tolerant, only give a warning here */
        msg_Warn(p_aout, "could not set alive check callback on device [0x%x] [%4.4s]",
                 (unsigned int)p_sys->i_selected_dev, (char *)&err);
    }

    AudioObjectPropertyAddress audioDeviceHogModeAddress = { kAudioDevicePropertyHogMode,
                                  kAudioDevicePropertyScopeOutput,
                                  kAudioObjectPropertyElementMaster };
    i_param_size = sizeof(p_sys->i_hog_pid);
    err = AudioObjectGetPropertyData(p_sys->i_selected_dev, &audioDeviceHogModeAddress, 0, NULL, &i_param_size, &p_sys->i_hog_pid);
    if (err != noErr) {
        /* This is not a fatal error. Some drivers simply don't support this property */
        msg_Warn(p_aout, "could not check whether device is hogged [%4.4s]",
                 (char *)&err);
        p_sys->i_hog_pid = -1;
    }

    if (p_sys->i_hog_pid != -1 && p_sys->i_hog_pid != getpid()) {
        msg_Err(p_aout, "Selected audio device is exclusively in use by another program.");
        dialog_Fatal(p_aout, _("Audio output failed"), "%s",
                        _("The selected audio output device is exclusively in "
                          "use by another program."));
        goto error;
    }

    bool b_success = false;

    /* Check for Digital mode or Analog output mode */
    if (AOUT_FMT_SPDIF (fmt) && b_start_digital) {
        if (StartSPDIF (p_aout, fmt)) {
            msg_Dbg(p_aout, "digital output successfully opened");
            b_success = true;
        }
    } else {
        if (StartAnalog(p_aout, fmt)) {
            msg_Dbg(p_aout, "analog output successfully opened");
            b_success = true;
        }
    }
    vlc_mutex_unlock(&p_sys->var_lock);

    if (b_success) {
        p_aout->play = Play;
        p_aout->flush = Flush;
        p_aout->time_get = TimeGet;
        p_aout->pause = Pause;
        return VLC_SUCCESS;
    }

error:
    /* If we reach this, this aout has failed */
    vlc_mutex_unlock(&p_sys->var_lock);
    msg_Err(p_aout, "opening auhal output failed");
    return VLC_EGENERIC;
}

/*
 * StartAnalog: open and setup a HAL AudioUnit to do PCM audio output
 */
static int StartAnalog(audio_output_t *p_aout, audio_sample_format_t *fmt)
{
    struct aout_sys_t           *p_sys = p_aout->sys;
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0;
    int                         i_original;
    AudioComponentDescription   desc;
    AudioStreamBasicDescription DeviceFormat;
    AudioChannelLayout          *layout;
    AudioChannelLayout          new_layout;
    AURenderCallbackStruct      input;
    p_aout->sys->chans_to_reorder = 0;

    SInt32 currentMinorSystemVersion;
    if(Gestalt(gestaltSystemVersionMinor, &currentMinorSystemVersion) != noErr)
        msg_Err(p_aout, "failed to check OSX version");

    /* Lets go find our Component */
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    p_sys->au_component = AudioComponentFindNext(NULL, &desc);
    if (p_sys->au_component == NULL) {
        msg_Err(p_aout, "cannot find any HAL component, PCM output failed");
        return false;
    }

    err = AudioComponentInstanceNew(p_sys->au_component, &p_sys->au_unit);
    if (err != noErr) {
        msg_Err(p_aout, "cannot open HAL component, PCM output failed [%4.4s]", (char *)&err);
        return false;
    }

    /* Set the device we will use for this output unit */
    err = AudioUnitSetProperty(p_sys->au_unit,
                         kAudioOutputUnitProperty_CurrentDevice,
                         kAudioUnitScope_Global,
                         0,
                         &p_sys->i_selected_dev,
                         sizeof(AudioObjectID));

    if (err != noErr) {
        msg_Err(p_aout, "cannot select audio output device, PCM output failed [%4.4s]", (char *)&err);
        return false;
    }

    /* Get the current format */
    i_param_size = sizeof(AudioStreamBasicDescription);

    err = AudioUnitGetProperty(p_sys->au_unit,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Output,
                                   0,
                                   &DeviceFormat,
                                   &i_param_size);

    if (err != noErr) {
        msg_Err(p_aout, "failed to detect supported stream formats [%4.4s]", (char *)&err);
        return false;
    } else
        msg_Dbg(p_aout, STREAM_FORMAT_MSG("current format is: ", DeviceFormat));

    /* Get the channel layout of the device side of the unit (vlc -> unit -> device) */
    err = AudioUnitGetPropertyInfo(p_sys->au_unit,
                                   kAudioDevicePropertyPreferredChannelLayout,
                                   kAudioUnitScope_Output,
                                   0,
                                   &i_param_size,
                                   NULL);

    if (err == noErr) {
        layout = (AudioChannelLayout *)malloc(i_param_size);

        verify_noerr(AudioUnitGetProperty(p_sys->au_unit,
                                       kAudioDevicePropertyPreferredChannelLayout,
                                       kAudioUnitScope_Output,
                                       0,
                                       layout,
                                       &i_param_size));

        /* We need to "fill out" the ChannelLayout, because there are multiple ways that it can be set */
        if (layout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap) {
            /* bitmap defined channellayout */
            verify_noerr(AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForBitmap,
                                    sizeof(UInt32), &layout->mChannelBitmap,
                                    &i_param_size,
                                    layout));
        } else if (layout->mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelDescriptions)
        {
            /* layouttags defined channellayout */
            verify_noerr(AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag,
                                    sizeof(AudioChannelLayoutTag), &layout->mChannelLayoutTag,
                                    &i_param_size,
                                    layout));
        }

        msg_Dbg(p_aout, "layout of AUHAL has %i channels" , layout->mNumberChannelDescriptions);

        if (layout->mNumberChannelDescriptions == 0) {
            msg_Err(p_aout, "insufficient number of output channels");
            return false;
        }

        /* Initialize the VLC core channel count */
        fmt->i_physical_channels = 0;
        i_original = fmt->i_original_channels & AOUT_CHAN_PHYSMASK;

        if (i_original == AOUT_CHAN_CENTER || layout->mNumberChannelDescriptions < 2) {
            /* We only need Mono or cannot output more than 1 channel */
            fmt->i_physical_channels = AOUT_CHAN_CENTER;
        } else if (i_original == (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT) || layout->mNumberChannelDescriptions < 3) {
            /* We only need Stereo or cannot output more than 2 channels */
            fmt->i_physical_channels = AOUT_CHANS_STEREO;
        } else {
            /* We want more than stereo and we can do that */
            for (unsigned int i = 0; i < layout->mNumberChannelDescriptions; i++) {
#ifndef NDEBUG
                msg_Dbg(p_aout, "this is channel: %d", (int)layout->mChannelDescriptions[i].mChannelLabel);
#endif

                switch(layout->mChannelDescriptions[i].mChannelLabel) {
                    case kAudioChannelLabel_Left:
                        fmt->i_physical_channels |= AOUT_CHAN_LEFT;
                        continue;
                    case kAudioChannelLabel_Right:
                        fmt->i_physical_channels |= AOUT_CHAN_RIGHT;
                        continue;
                    case kAudioChannelLabel_Center:
                        fmt->i_physical_channels |= AOUT_CHAN_CENTER;
                        continue;
                    case kAudioChannelLabel_LFEScreen:
                        fmt->i_physical_channels |= AOUT_CHAN_LFE;
                        continue;
                    case kAudioChannelLabel_LeftSurround:
                        fmt->i_physical_channels |= AOUT_CHAN_REARLEFT;
                        continue;
                    case kAudioChannelLabel_RightSurround:
                        fmt->i_physical_channels |= AOUT_CHAN_REARRIGHT;
                        continue;
                    case kAudioChannelLabel_RearSurroundLeft:
                        fmt->i_physical_channels |= AOUT_CHAN_MIDDLELEFT;
                        continue;
                    case kAudioChannelLabel_RearSurroundRight:
                        fmt->i_physical_channels |= AOUT_CHAN_MIDDLERIGHT;
                        continue;
                    case kAudioChannelLabel_CenterSurround:
                        fmt->i_physical_channels |= AOUT_CHAN_REARCENTER;
                        continue;
                    default:
                        msg_Warn(p_aout, "unrecognized channel form provided by driver: %d", (int)layout->mChannelDescriptions[i].mChannelLabel);
                }
            }
            if (fmt->i_physical_channels == 0) {
                fmt->i_physical_channels = AOUT_CHANS_STEREO;
                msg_Err(p_aout, "You should configure your speaker layout with Audio Midi Setup in /Applications/Utilities. VLC will output Stereo only.");
                dialog_Fatal(p_aout, _("Audio device is not configured"), "%s",
                                _("You should configure your speaker layout with "
                                  "\"Audio Midi Setup\" in /Applications/"
                                  "Utilities. VLC will output Stereo only."));
            }
        }
        free(layout);
    } else {
        msg_Warn(p_aout, "device driver does not support kAudioDevicePropertyPreferredChannelLayout - using stereo fallback [%4.4s]", (char *)&err);
        fmt->i_physical_channels = AOUT_CHANS_STEREO;
    }

    msg_Dbg(p_aout, "selected %d physical channels for device output", aout_FormatNbChannels(fmt));
    msg_Dbg(p_aout, "VLC will output: %s", aout_FormatPrintChannels(fmt));

    memset (&new_layout, 0, sizeof(new_layout));
    uint32_t chans_out[AOUT_CHAN_MAX];
    memset(&chans_out, 0, sizeof(chans_out));

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

    switch(aout_FormatNbChannels(fmt)) {
        case 1:
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
            break;
        case 2:
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
            break;
        case 3:
            if (fmt->i_physical_channels & AOUT_CHAN_CENTER)
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_7; // L R C
            else if (fmt->i_physical_channels & AOUT_CHAN_LFE)
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_4; // L R LFE
            break;
        case 4:
            if (fmt->i_physical_channels & (AOUT_CHAN_CENTER | AOUT_CHAN_LFE))
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_10; // L R C LFE
            else if (fmt->i_physical_channels & (AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT))
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_3; // L R Ls Rs
            else if (fmt->i_physical_channels & (AOUT_CHAN_CENTER | AOUT_CHAN_REARCENTER))
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_3; // L R C Cs
            break;
        case 5:
            if (fmt->i_physical_channels & (AOUT_CHAN_CENTER))
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_19; // L R Ls Rs C
            else if (fmt->i_physical_channels & (AOUT_CHAN_LFE))
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_18; // L R Ls Rs LFE
            break;
        case 6:
            if (fmt->i_physical_channels & (AOUT_CHAN_LFE)) {
                /* note: for some reason, kAudioChannelLayoutTag_DVD_20, which would not require channel reordering, 
                 does not work anymore */
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_AudioUnit_5_1; // L R C LFE Ls Rs

                chans_out[0] = AOUT_CHAN_LEFT;
                chans_out[1] = AOUT_CHAN_RIGHT;
                chans_out[2] = AOUT_CHAN_CENTER;
                chans_out[3] = AOUT_CHAN_LFE;
                chans_out[4] = AOUT_CHAN_REARLEFT;
                chans_out[5] = AOUT_CHAN_REARRIGHT;

                p_aout->sys->chans_to_reorder = aout_CheckChannelReorder(NULL, chans_out, fmt->i_physical_channels, p_aout->sys->chan_table);
                if (p_aout->sys->chans_to_reorder)
                    msg_Dbg(p_aout, "channel reordering needed for 5.1 output");
            } else {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_AudioUnit_6_0; // L R Ls Rs C Cs

                chans_out[0] = AOUT_CHAN_LEFT;
                chans_out[1] = AOUT_CHAN_RIGHT;
                chans_out[2] = AOUT_CHAN_REARLEFT;
                chans_out[3] = AOUT_CHAN_REARRIGHT;
                chans_out[4] = AOUT_CHAN_CENTER;
                chans_out[5] = AOUT_CHAN_REARCENTER;

                p_aout->sys->chans_to_reorder = aout_CheckChannelReorder(NULL, chans_out, fmt->i_physical_channels, p_aout->sys->chan_table);
                if (p_aout->sys->chans_to_reorder)
                    msg_Dbg(p_aout, "channel reordering needed for 6.0 output");
            }
            break;
        case 7:
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_6_1_A; // L R C LFE Ls Rs Cs

            chans_out[0] = AOUT_CHAN_LEFT;
            chans_out[1] = AOUT_CHAN_RIGHT;
            chans_out[2] = AOUT_CHAN_CENTER;
            chans_out[3] = AOUT_CHAN_LFE;
            chans_out[4] = AOUT_CHAN_REARLEFT;
            chans_out[5] = AOUT_CHAN_REARRIGHT;
            chans_out[6] = AOUT_CHAN_REARCENTER;

            p_aout->sys->chans_to_reorder = aout_CheckChannelReorder(NULL, chans_out, fmt->i_physical_channels, p_aout->sys->chan_table);
            if (p_aout->sys->chans_to_reorder)
                msg_Dbg(p_aout, "channel reordering needed for 6.1 output");

            break;
        case 8:
            if (fmt->i_physical_channels & (AOUT_CHAN_LFE) || currentMinorSystemVersion < 7) {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_7_1_A; // L R C LFE Ls Rs Lc Rc

                chans_out[0] = AOUT_CHAN_LEFT;
                chans_out[1] = AOUT_CHAN_RIGHT;
                chans_out[2] = AOUT_CHAN_CENTER;
                chans_out[3] = AOUT_CHAN_LFE;
                chans_out[4] = AOUT_CHAN_MIDDLELEFT;
                chans_out[5] = AOUT_CHAN_MIDDLERIGHT;
                chans_out[6] = AOUT_CHAN_REARLEFT;
                chans_out[7] = AOUT_CHAN_REARRIGHT;

                if (!(fmt->i_physical_channels & (AOUT_CHAN_LFE)))
                    msg_Warn(p_aout, "8.0 audio output not supported on OS X 10.%i, layout will be incorrect", currentMinorSystemVersion);
            }
#ifdef MAC_OS_X_VERSION_10_7
            else {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DTS_8_0_B; // Lc C Rc L R Ls Cs Rs

                chans_out[0] = AOUT_CHAN_MIDDLELEFT;
                chans_out[1] = AOUT_CHAN_CENTER;
                chans_out[2] = AOUT_CHAN_MIDDLERIGHT;
                chans_out[3] = AOUT_CHAN_LEFT;
                chans_out[4] = AOUT_CHAN_RIGHT;
                chans_out[5] = AOUT_CHAN_REARLEFT;
                chans_out[6] = AOUT_CHAN_REARCENTER;
                chans_out[7] = AOUT_CHAN_REARRIGHT;
            }
#endif
            p_aout->sys->chans_to_reorder = aout_CheckChannelReorder(NULL, chans_out, fmt->i_physical_channels, p_aout->sys->chan_table);
            if (p_aout->sys->chans_to_reorder)
                msg_Dbg(p_aout, "channel reordering needed for 7.1 / 8.0 output");

            break;
        case 9:
            if (currentMinorSystemVersion < 7) {
                msg_Warn(p_aout, "8.1 audio output not supported on OS X 10.%i", currentMinorSystemVersion);
                break;
            }

#ifdef MAC_OS_X_VERSION_10_7
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DTS_8_1_B; // Lc C Rc L R Ls Cs Rs LFE
            chans_out[0] = AOUT_CHAN_MIDDLELEFT;
            chans_out[1] = AOUT_CHAN_CENTER;
            chans_out[2] = AOUT_CHAN_MIDDLERIGHT;
            chans_out[3] = AOUT_CHAN_LEFT;
            chans_out[4] = AOUT_CHAN_RIGHT;
            chans_out[5] = AOUT_CHAN_REARLEFT;
            chans_out[6] = AOUT_CHAN_REARCENTER;
            chans_out[7] = AOUT_CHAN_REARRIGHT;
            chans_out[8] = AOUT_CHAN_LFE;

            p_aout->sys->chans_to_reorder = aout_CheckChannelReorder(NULL, chans_out, fmt->i_physical_channels, p_aout->sys->chan_table);
            if (p_aout->sys->chans_to_reorder)
                msg_Dbg(p_aout, "channel reordering needed for 8.1 output");
#endif
            break;
    }

    /* Set up the format to be used */
    DeviceFormat.mSampleRate = fmt->i_rate;
    DeviceFormat.mFormatID = kAudioFormatLinearPCM;
    p_sys->i_rate = fmt->i_rate;

    /* We use float 32 since this is VLC's endorsed format */
    fmt->i_format = VLC_CODEC_FL32;
    DeviceFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    DeviceFormat.mBitsPerChannel = 32;
    DeviceFormat.mChannelsPerFrame = aout_FormatNbChannels(fmt);

    /* Calculate framesizes and stuff */
    DeviceFormat.mFramesPerPacket = 1;
    DeviceFormat.mBytesPerFrame = DeviceFormat.mBitsPerChannel * DeviceFormat.mChannelsPerFrame / 8;
    DeviceFormat.mBytesPerPacket = DeviceFormat.mBytesPerFrame * DeviceFormat.mFramesPerPacket;

    /* Set the desired format */
    i_param_size = sizeof(AudioStreamBasicDescription);
    verify_noerr(AudioUnitSetProperty(p_sys->au_unit,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Input,
                                   0,
                                   &DeviceFormat,
                                   i_param_size));

    msg_Dbg(p_aout, STREAM_FORMAT_MSG("we set the AU format: " , DeviceFormat));

    /* Retrieve actual format */
    verify_noerr(AudioUnitGetProperty(p_sys->au_unit,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Input,
                                   0,
                                   &DeviceFormat,
                                   &i_param_size));

    msg_Dbg(p_aout, STREAM_FORMAT_MSG("the actual set AU format is " , DeviceFormat));

    /* Do the last VLC aout setups */
    aout_FormatPrepare(fmt);

    /* set the IOproc callback */
    input.inputProc = (AURenderCallback) RenderCallbackAnalog;
    input.inputProcRefCon = p_aout;

    verify_noerr(AudioUnitSetProperty(p_sys->au_unit,
                            kAudioUnitProperty_SetRenderCallback,
                            kAudioUnitScope_Input,
                            0, &input, sizeof(input)));

    /* Set the new_layout as the layout VLC will use to feed the AU unit */
    verify_noerr(AudioUnitSetProperty(p_sys->au_unit,
                            kAudioUnitProperty_AudioChannelLayout,
                            kAudioUnitScope_Output,
                            0, &new_layout, sizeof(new_layout)));

    if (new_layout.mNumberChannelDescriptions > 0)
        free(new_layout.mChannelDescriptions);

    /* AU initiliaze */
    verify_noerr(AudioUnitInitialize(p_sys->au_unit));

    /* setup circular buffer */
    TPCircularBufferInit(&p_sys->circular_buffer, AUDIO_BUFFER_SIZE_IN_SECONDS *
                         fmt->i_rate * fmt->i_bytes_per_frame);

    p_sys->b_got_first_sample = false;

    /* Set volume for output unit */
    VolumeSet(p_aout, p_sys->f_volume);
    MuteSet(p_aout, p_sys->b_mute);

    return true;
}

/*
 * StartSPDIF: Setup an encoded digital stream (SPDIF) output
 */
static int StartSPDIF(audio_output_t * p_aout, audio_sample_format_t *fmt)
{
    struct aout_sys_t       *p_sys = p_aout->sys;
    OSStatus                err = noErr;
    UInt32                  i_param_size = 0, b_mix = 0;
    Boolean                 b_writeable = false;
    AudioStreamID           *p_streams = NULL;
    unsigned                i_streams = 0;

    /* Start doing the SPDIF setup proces */
    p_sys->b_digital = true;

    /* Hog the device */
    AudioObjectPropertyAddress audioDeviceHogModeAddress = { kAudioDevicePropertyHogMode, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    i_param_size = sizeof(p_sys->i_hog_pid);
    p_sys->i_hog_pid = getpid() ;

    err = AudioObjectSetPropertyData(p_sys->i_selected_dev, &audioDeviceHogModeAddress, 0, NULL, i_param_size, &p_sys->i_hog_pid);

    if (err != noErr) {
        msg_Err(p_aout, "failed to set hogmode [%4.4s]", (char *)&err);
        return false;
    }

    AudioObjectPropertyAddress audioDeviceSupportsMixingAddress = { kAudioDevicePropertySupportsMixing , kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };

    if (AudioObjectHasProperty(p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress)) {
        /* Set mixable to false if we are allowed to */
        err = AudioObjectIsPropertySettable(p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, &b_writeable);
        err = AudioObjectGetPropertyDataSize(p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, 0, NULL, &i_param_size);
        err = AudioObjectGetPropertyData(p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, 0, NULL, &i_param_size, &b_mix);

        if (err == noErr && b_writeable) {
            b_mix = 0;
            err = AudioObjectSetPropertyData(p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, 0, NULL, i_param_size, &b_mix);
            p_sys->b_changed_mixing = true;
        }

        if (err != noErr) {
            msg_Err(p_aout, "failed to set mixmode [%4.4s]", (char *)&err);
            return false;
        }
    }

    /* Get a list of all the streams on this device */
    AudioObjectPropertyAddress streamsAddress = { kAudioDevicePropertyStreams, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    err = AudioObjectGetPropertyDataSize(p_sys->i_selected_dev, &streamsAddress, 0, NULL, &i_param_size);
    if (err != noErr) {
        msg_Err(p_aout, "could not get size of stream description packet [%4.4s]", (char *)&err);
        return false;
    }

    i_streams = i_param_size / sizeof(AudioStreamID);
    p_streams = (AudioStreamID *)malloc(i_param_size);
    if (p_streams == NULL)
        return false;

    err = AudioObjectGetPropertyData(p_sys->i_selected_dev, &streamsAddress, 0, NULL, &i_param_size, p_streams);

    if (err != noErr) {
        msg_Err(p_aout, "could not fetch stream descriptions [%4.4s]", (char *)&err);
        free(p_streams);
        return false;
    }

    AudioObjectPropertyAddress physicalFormatsAddress = { kAudioStreamPropertyAvailablePhysicalFormats, kAudioObjectPropertyScopeGlobal, 0 };
    for (unsigned i = 0; i < i_streams && p_sys->i_stream_index < 0 ; i++) {
        /* Find a stream with a cac3 stream */
        AudioStreamRangedDescription *p_format_list = NULL;
        int                          i_formats = 0;
        bool                         b_digital = false;

        /* Retrieve all the stream formats supported by each output stream */
        err = AudioObjectGetPropertyDataSize(p_streams[i], &physicalFormatsAddress, 0, NULL, &i_param_size);
        if (err != noErr) {
            msg_Err(p_aout, "could not get number of streamformats: [%4.4s] (%i)", (char *)&err, (int32_t)err);
            continue;
        }

        i_formats = i_param_size / sizeof(AudioStreamRangedDescription);
        p_format_list = (AudioStreamRangedDescription *)malloc(i_param_size);
        if (p_format_list == NULL)
            continue;

        err = AudioObjectGetPropertyData(p_streams[i], &physicalFormatsAddress, 0, NULL, &i_param_size, p_format_list);
        if (err != noErr) {
            msg_Err(p_aout, "could not get the list of streamformats: [%4.4s]", (char *)&err);
            free(p_format_list);
            continue;
        }

        /* Check if one of the supported formats is a digital format */
        for (int j = 0; j < i_formats; j++) {
            if (p_format_list[j].mFormat.mFormatID == 'IAC3' ||
               p_format_list[j].mFormat.mFormatID == 'iac3' ||
               p_format_list[j].mFormat.mFormatID == kAudioFormat60958AC3 ||
               p_format_list[j].mFormat.mFormatID == kAudioFormatAC3) {
                b_digital = true;
                break;
            }
        }

        if (b_digital) {
            /* if this stream supports a digital (cac3) format, then go set it. */
            int i_requested_rate_format = -1;
            int i_current_rate_format = -1;
            int i_backup_rate_format = -1;

            p_sys->i_stream_id = p_streams[i];
            p_sys->i_stream_index = i;

            if (!p_sys->b_revert) {
                AudioObjectPropertyAddress currentPhysicalFormatAddress = { kAudioStreamPropertyPhysicalFormat, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
                /* Retrieve the original format of this stream first if not done so already */
                i_param_size = sizeof(p_sys->sfmt_revert);
                err = AudioObjectGetPropertyData(p_sys->i_stream_id, &currentPhysicalFormatAddress, 0, NULL, &i_param_size, &p_sys->sfmt_revert);
                if (err != noErr) {
                    msg_Err(p_aout, "could not retrieve the original streamformat [%4.4s]", (char *)&err);
                    continue;
                }
                p_sys->b_revert = true;
            }

            for (int j = 0; j < i_formats; j++) {
                if (p_format_list[j].mFormat.mFormatID == 'IAC3' ||
                   p_format_list[j].mFormat.mFormatID == 'iac3' ||
                   p_format_list[j].mFormat.mFormatID == kAudioFormat60958AC3 ||
                   p_format_list[j].mFormat.mFormatID == kAudioFormatAC3) {
                    if (p_format_list[j].mFormat.mSampleRate == fmt->i_rate) {
                        i_requested_rate_format = j;
                        break;
                    } else if (p_format_list[j].mFormat.mSampleRate == p_sys->sfmt_revert.mSampleRate)
                        i_current_rate_format = j;
                    else {
                        if (i_backup_rate_format < 0 || p_format_list[j].mFormat.mSampleRate > p_format_list[i_backup_rate_format].mFormat.mSampleRate)
                            i_backup_rate_format = j;
                    }
                }

            }

            if (i_requested_rate_format >= 0) /* We prefer to output at the samplerate of the original audio */
                p_sys->stream_format = p_format_list[i_requested_rate_format].mFormat;
            else if (i_current_rate_format >= 0) /* If not possible, we will try to use the current samplerate of the device */
                p_sys->stream_format = p_format_list[i_current_rate_format].mFormat;
            else
                p_sys->stream_format = p_format_list[i_backup_rate_format].mFormat; /* And if we have to, any digital format will be just fine (highest rate possible) */
        }
        free(p_format_list);
    }
    free(p_streams);

    msg_Dbg(p_aout, STREAM_FORMAT_MSG("original stream format: ", p_sys->sfmt_revert));

    if (!AudioStreamChangeFormat(p_aout, p_sys->i_stream_id, p_sys->stream_format)) {
        msg_Err(p_aout, "failed to change stream format for SPDIF output");
        return false;
    }

    /* Set the format flags */
    if (p_sys->stream_format.mFormatFlags & kAudioFormatFlagIsBigEndian)
        fmt->i_format = VLC_CODEC_SPDIFB;
    else
        fmt->i_format = VLC_CODEC_SPDIFL;
    fmt->i_bytes_per_frame = AOUT_SPDIF_SIZE;
    fmt->i_frame_length = A52_FRAME_NB;
    fmt->i_rate = (unsigned int)p_sys->stream_format.mSampleRate;
    p_sys->i_rate = fmt->i_rate;
    aout_FormatPrepare(fmt);

    /* Add IOProc callback */
    err = AudioDeviceCreateIOProcID(p_sys->i_selected_dev,
                                   (AudioDeviceIOProc)RenderCallbackSPDIF,
                                   (void *)p_aout,
                                   &p_sys->i_procID);
    if (err != noErr) {
        msg_Err(p_aout, "Failed to create Process ID [%4.4s]", (char *)&err);
        return false;
    }

    /* Start device */
    err = AudioDeviceStart(p_sys->i_selected_dev, p_sys->i_procID);
    if (err != noErr) {
        msg_Err(p_aout, "Failed to start audio device [%4.4s]", (char *)&err);

        err = AudioDeviceDestroyIOProcID(p_sys->i_selected_dev, p_sys->i_procID);
        if (err != noErr)
            msg_Err(p_aout, "Failed to destroy process ID [%4.4s]", (char *)&err);

        return false;
    }

    /* setup circular buffer */
    TPCircularBufferInit(&p_sys->circular_buffer, 200 * AOUT_SPDIF_SIZE);

    return true;
}

static void Stop(audio_output_t *p_aout)
{
    struct aout_sys_t   *p_sys = p_aout->sys;
    OSStatus            err = noErr;
    UInt32              i_param_size = 0;

    if (p_sys->au_unit) {
        verify_noerr(AudioOutputUnitStop(p_sys->au_unit));
        verify_noerr(AudioUnitUninitialize(p_sys->au_unit));
        verify_noerr(AudioComponentInstanceDispose(p_sys->au_unit));
    }

    vlc_mutex_lock(&p_sys->var_lock);
    if (p_sys->b_digital) {
        /* Stop device */
        err = AudioDeviceStop(p_sys->i_selected_dev,
                               p_sys->i_procID);
        if (err != noErr)
            msg_Err(p_aout, "Failed to stop audio device [%4.4s]", (char *)&err);

        /* Remove IOProc callback */
        err = AudioDeviceDestroyIOProcID(p_sys->i_selected_dev,
                                          p_sys->i_procID);
        if (err != noErr)
            msg_Err(p_aout, "Failed to destroy Process ID [%4.4s]", (char *)&err);

        if (p_sys->b_revert)
            AudioStreamChangeFormat(p_aout, p_sys->i_stream_id, p_sys->sfmt_revert);

        if (p_sys->b_changed_mixing && p_sys->sfmt_revert.mFormatID != kAudioFormat60958AC3) {
            int b_mix;
            Boolean b_writeable = false;
            /* Revert mixable to true if we are allowed to */
            AudioObjectPropertyAddress audioDeviceSupportsMixingAddress = { kAudioDevicePropertySupportsMixing , kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
            err = AudioObjectIsPropertySettable(p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, &b_writeable);
            err = AudioObjectGetPropertyData(p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, 0, NULL, &i_param_size, &b_mix);

            if (err == noErr && b_writeable) {
                msg_Dbg(p_aout, "mixable is: %d", b_mix);
                b_mix = 1;
                err = AudioObjectSetPropertyData(p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, 0, NULL, i_param_size, &b_mix);
            }

            if (err != noErr)
                msg_Err(p_aout, "failed to re-set mixmode [%4.4s]", (char *)&err);
        }
    }

    if (p_sys->i_hog_pid == getpid()) {
        p_sys->i_hog_pid = -1;
        i_param_size = sizeof(p_sys->i_hog_pid);
        AudioObjectPropertyAddress audioDeviceHogModeAddress = { kAudioDevicePropertyHogMode,
            kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMaster };
        err = AudioObjectSetPropertyData(p_sys->i_selected_dev, &audioDeviceHogModeAddress, 0, NULL, i_param_size, &p_sys->i_hog_pid);
        if (err != noErr)
            msg_Err(p_aout, "Failed to release hogmode [%4.4s]", (char *)&err);
    }

    /* remove audio device alive callback */
    AudioObjectPropertyAddress deviceAliveAddress = { kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    err = AudioObjectRemovePropertyListener(p_sys->i_selected_dev, &deviceAliveAddress, DeviceAliveListener, (void *)p_aout);
    if (err != noErr) {
        /* Be tolerant, only give a warning here */
        msg_Warn(p_aout, "failed to remove audio device life checker [%4.4s]", (char *)&err);
    }

    vlc_mutex_unlock(&p_sys->var_lock);

    p_sys->i_bytes_per_sample = 0;
    p_sys->b_digital = false;

    /* clean-up circular buffer */
    TPCircularBufferCleanup(&p_sys->circular_buffer);
}

#pragma mark -
#pragma mark core interaction

static void ReportDevice(audio_output_t *p_aout, UInt32 i_id, char *name)
{
    char deviceid[10];
    sprintf(deviceid, "%i", i_id);

    aout_HotplugReport(p_aout, deviceid, name);
}

static void RebuildDeviceList(audio_output_t * p_aout)
{
    OSStatus            err = noErr;
    UInt32              propertySize = 0;
    AudioObjectID       *deviceIDs;
    UInt32              numberOfDevices;
    CFMutableArrayRef   currentListOfDevices;

    struct aout_sys_t   *p_sys = p_aout->sys;

    ReportDevice(p_aout, 0, _("System Sound Output Device"));

    /* setup local array */
    currentListOfDevices = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);

    /* Get number of devices */
    AudioObjectPropertyAddress audioDevicesAddress = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &audioDevicesAddress, 0, NULL, &propertySize);
    if (err != noErr) {
        msg_Err(p_aout, "Could not get number of devices: [%4.4s]", (char *)&err);
        return;
    }

    numberOfDevices = propertySize / sizeof(AudioDeviceID);

    if (numberOfDevices < 1) {
        msg_Err(p_aout, "No audio output devices found.");
        return;
    }
    msg_Dbg(p_aout, "found %i audio device(s)", numberOfDevices);

    /* Allocate DeviceID array */
    deviceIDs = (AudioDeviceID *)calloc(numberOfDevices, sizeof(AudioDeviceID));
    if (deviceIDs == NULL)
        return;

    /* Populate DeviceID array */
    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &audioDevicesAddress, 0, NULL, &propertySize, deviceIDs);
    if (err != noErr) {
        msg_Err(p_aout, "could not get the device IDs [%4.4s]", (char *)&err);
        return;
    }

    AudioObjectPropertyAddress deviceNameAddress = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };

    for (unsigned int i = 0; i < numberOfDevices; i++) {
        CFStringRef device_name_ref;
        char *psz_name;
        CFIndex length;
        bool b_digital = false;
        UInt32 i_id = deviceIDs[i];

        /* Retrieve the length of the device name */
        err = AudioObjectGetPropertyDataSize(deviceIDs[i], &deviceNameAddress, 0, NULL, &propertySize);
        if (err != noErr) {
            msg_Dbg(p_aout, "failed to get name size for device %i", deviceIDs[i]);
            continue;
        }

        /* Retrieve the name of the device */
        err = AudioObjectGetPropertyData(deviceIDs[i], &deviceNameAddress, 0, NULL, &propertySize, &device_name_ref);
        if (err != noErr) {
            msg_Dbg(p_aout, "failed to get name for device %i", deviceIDs[i]);
            continue;
        }
        length = CFStringGetLength(device_name_ref);
        length++;
        psz_name = (char *)malloc(length);
        CFStringGetCString(device_name_ref, psz_name, length, kCFStringEncodingUTF8);

        msg_Dbg(p_aout, "DevID: %i DevName: %s", deviceIDs[i], psz_name);

        if (!AudioDeviceHasOutput(deviceIDs[i])) {
            msg_Dbg(p_aout, "this '%s' is INPUT only. skipping...", psz_name);
            free(psz_name);
            continue;
        }

        ReportDevice(p_aout, i_id, psz_name);
        CFArrayAppendValue(currentListOfDevices, CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i_id));

        if (AudioDeviceSupportsDigital(p_aout, deviceIDs[i])) {
            b_digital = true;
            msg_Dbg(p_aout, "'%s' supports digital output", psz_name);
            char *psz_encoded_name = nil;
            asprintf(&psz_encoded_name, _("%s (Encoded Output)"), psz_name);
            i_id = i_id | AOUT_VAR_SPDIF_FLAG;
            ReportDevice(p_aout, i_id, psz_encoded_name);
            CFArrayAppendValue(currentListOfDevices, CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i_id));
            free(psz_encoded_name);
        }

        // TODO: only register once for each device
        ManageAudioStreamsCallback(p_aout, deviceIDs[i], true);

        CFRelease(device_name_ref);
        free(psz_name);
    }

    vlc_mutex_lock(&p_sys->var_lock);
    CFIndex count = 0;
    if (p_sys->device_list)
        count = CFArrayGetCount(p_sys->device_list);

    if (count > 0) {
        CFNumberRef cfn_device_id;
        int i_device_id = 0;
        for (CFIndex x = 0; x < count; x++) {
            if (!CFArrayContainsValue(currentListOfDevices, CFRangeMake(0, count), CFArrayGetValueAtIndex(p_sys->device_list, x))) {
                cfn_device_id = CFArrayGetValueAtIndex(p_sys->device_list, x);

                if (cfn_device_id) {
                    CFNumberGetValue(cfn_device_id, kCFNumberSInt32Type, &i_device_id);
                    ReportDevice(p_aout, i_device_id, NULL);
                }
            }
        }
    }
    CFRelease(p_sys->device_list);
    p_sys->device_list = CFArrayCreateCopy(kCFAllocatorDefault, currentListOfDevices);
    CFRelease(currentListOfDevices);
    vlc_mutex_unlock(&p_sys->var_lock);

    free(deviceIDs);
}

static int SwitchAudioDevice(audio_output_t *p_aout, const char *name)
{
    struct aout_sys_t *p_sys = p_aout->sys;

    if (name)
        p_sys->i_new_selected_dev = atoi(name);
    else
        p_sys->i_new_selected_dev = 0;

    bool b_supports_digital = (p_sys->i_new_selected_dev & AOUT_VAR_SPDIF_FLAG);
    if (b_supports_digital)
        p_sys->b_selected_dev_is_digital = true;
    else
        p_sys->b_selected_dev_is_digital = false;

    p_sys->i_new_selected_dev = p_sys->i_new_selected_dev & ~AOUT_VAR_SPDIF_FLAG;

    aout_DeviceReport(p_aout, name);
    aout_RestartRequest(p_aout, AOUT_RESTART_OUTPUT);

    return 0;
}

static int VolumeSet(audio_output_t * p_aout, float volume)
{
    struct aout_sys_t *p_sys = p_aout->sys;
    OSStatus ostatus;

    if(p_sys->b_digital)
        return VLC_EGENERIC;

    p_sys->f_volume = volume;
    aout_VolumeReport(p_aout, volume);

    /* Set volume for output unit */
    ostatus = AudioUnitSetParameter(p_sys->au_unit,
                                    kHALOutputParam_Volume,
                                    kAudioUnitScope_Global,
                                    0,
                                    volume * volume * volume,
                                    0);

    if (var_InheritBool(p_aout, "volume-save"))
        config_PutInt(p_aout, "auhal-volume", lroundf(volume * AOUT_VOLUME_DEFAULT));

    return ostatus;
}

static int MuteSet(audio_output_t * p_aout, bool mute)
{
    struct   aout_sys_t *p_sys = p_aout->sys;
    OSStatus ostatus;

    if(p_sys->b_digital)
        return VLC_EGENERIC;

    p_sys->b_mute = mute;
    aout_MuteReport(p_aout, mute);

    float volume = .0;
    if (!mute)
        volume = p_sys->f_volume;

    ostatus = AudioUnitSetParameter(p_sys->au_unit,
                                    kHALOutputParam_Volume,
                                    kAudioUnitScope_Global,
                                    0,
                                    volume * volume * volume,
                                    0);

    return ostatus;
}

#pragma mark -
#pragma mark actual playback

static void Play(audio_output_t * p_aout, block_t * p_block)
{
    struct aout_sys_t *p_sys = p_aout->sys;

    if (p_block->i_nb_samples > 0) {
        if (!p_sys->b_got_first_sample) {
            /* Start the AU */
            verify_noerr(AudioOutputUnitStart(p_sys->au_unit));
            p_sys->b_got_first_sample = true;
        }

        /* Do the channel reordering */
        if (p_sys->chans_to_reorder && !p_sys->b_digital) {
           aout_ChannelReorder(p_block->p_buffer,
                               p_block->i_buffer,
                               p_sys->chans_to_reorder,
                               p_sys->chan_table,
                               VLC_CODEC_FL32);
        }

        /* move data to buffer */
        if (unlikely(!TPCircularBufferProduceBytes(&p_sys->circular_buffer, p_block->p_buffer, p_block->i_buffer)))
            msg_Warn(p_aout, "dropped buffer");

        if (!p_sys->i_bytes_per_sample)
            p_sys->i_bytes_per_sample = p_block->i_buffer / p_block->i_nb_samples;
    }

    block_Release(p_block);
}

static void Pause(audio_output_t *p_aout, bool pause, mtime_t date)
{
    struct aout_sys_t * p_sys = p_aout->sys;
    VLC_UNUSED(date);

    if (p_aout->sys->b_digital) {
        if (pause)
            AudioDeviceStop(p_sys->i_selected_dev, p_sys->i_procID);
        else
            AudioDeviceStart(p_sys->i_selected_dev, p_sys->i_procID);
    } else {
        if (pause)
            AudioOutputUnitStop(p_sys->au_unit);
        else
            AudioOutputUnitStart(p_sys->au_unit);
    }
}

static void Flush(audio_output_t *p_aout, bool wait)
{
    struct aout_sys_t *p_sys = p_aout->sys;

    if (wait) {
        int32_t availableBytes;
        vlc_mutex_lock(&p_sys->lock);
        TPCircularBufferTail(&p_sys->circular_buffer, &availableBytes);
        while (availableBytes > 0) {
            vlc_cond_wait(&p_sys->cond, &p_sys->lock);
            TPCircularBufferTail(&p_sys->circular_buffer, &availableBytes);
        }
        vlc_mutex_unlock(&p_sys->lock);

    } else {
        p_sys->b_got_first_sample = false;

        /* flush circular buffer */
        AudioOutputUnitStop(p_aout->sys->au_unit);
        TPCircularBufferClear(&p_aout->sys->circular_buffer);
    }
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
 * RenderCallbackAnalog: This function is called everytime the AudioUnit wants
 * us to provide some more audio data.
 * Don't print anything during normal playback, calling blocking function from
 * this callback is not allowed.
 *****************************************************************************/
static OSStatus RenderCallbackAnalog(vlc_object_t *p_obj,
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
    if (!availableBytes) {
        /* return an empty buffer so silence is played until we have data */
        memset(targetBuffer, 0, ioData->mBuffers[0].mDataByteSize);
    } else {
        int32_t bytesToCopy = __MIN(bytesRequested, availableBytes);

        memcpy(targetBuffer, buffer, bytesToCopy);
        TPCircularBufferConsume(&p_sys->circular_buffer, bytesToCopy);
        ioData->mBuffers[0].mDataByteSize = bytesToCopy;
    }

    vlc_cond_signal(&p_sys->cond);
    vlc_mutex_unlock(&p_sys->lock);

    return noErr;
}

/*
 * RenderCallbackSPDIF: callback for SPDIF audio output
 */
static OSStatus RenderCallbackSPDIF(AudioDeviceID inDevice,
                                    const AudioTimeStamp * inNow,
                                    const void * inInputData,
                                    const AudioTimeStamp * inInputTime,
                                    AudioBufferList * outOutputData,
                                    const AudioTimeStamp * inOutputTime,
                                    void * threadGlobals)
{
    VLC_UNUSED(inNow);
    VLC_UNUSED(inDevice);
    VLC_UNUSED(inInputData);
    VLC_UNUSED(inInputTime);
    VLC_UNUSED(inOutputTime);

    audio_output_t * p_aout = (audio_output_t *)threadGlobals;
    struct aout_sys_t * p_sys = p_aout->sys;

    int bytesRequested = outOutputData->mBuffers[p_sys->i_stream_index].mDataByteSize;
    char *targetBuffer = outOutputData->mBuffers[p_sys->i_stream_index].mData;

    vlc_mutex_lock(&p_sys->lock);
    /* Pull audio from buffer */
    int32_t availableBytes;
    char *buffer = TPCircularBufferTail(&p_sys->circular_buffer, &availableBytes);

    /* check if we have enough data */
    if (!availableBytes) {
        /* return an empty buffer so silence is played until we have data */
        memset(targetBuffer, 0, outOutputData->mBuffers[p_sys->i_stream_index].mDataByteSize);
    } else {
        int32_t bytesToCopy = __MIN(bytesRequested, availableBytes);

        memcpy(targetBuffer, buffer, bytesToCopy);
        TPCircularBufferConsume(&p_sys->circular_buffer, bytesToCopy);
        outOutputData->mBuffers[p_sys->i_stream_index].mDataByteSize = bytesToCopy;
    }

    vlc_cond_signal(&p_sys->cond);
    vlc_mutex_unlock(&p_sys->lock);

    return noErr;
}

#pragma mark -
#pragma mark Stream / Hardware Listeners

/*
 * Callback when device list changed
 */
static OSStatus DevicesListener(AudioObjectID inObjectID,  UInt32 inNumberAddresses, const AudioObjectPropertyAddress inAddresses[], void*inClientData)
{
    VLC_UNUSED(inObjectID);
    VLC_UNUSED(inNumberAddresses);
    VLC_UNUSED(inAddresses);

    audio_output_t *p_aout = (audio_output_t *)inClientData;
    if (!p_aout)
        return -1;
    aout_sys_t *p_sys = p_aout->sys;

    msg_Dbg(p_aout, "audio device configuration changed, resetting cache");
    RebuildDeviceList(p_aout);

    vlc_mutex_lock(&p_sys->var_lock);
    if(!CFArrayContainsValue(p_sys->device_list, CFRangeMake(0, CFArrayGetCount(p_sys->device_list)),CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &p_sys->i_selected_dev)))
        aout_RestartRequest(p_aout, AOUT_RESTART_OUTPUT);
    vlc_mutex_unlock(&p_sys->var_lock);

    return noErr;
}

/*
 * Callback when current device is not alive anymore
 */
static OSStatus DeviceAliveListener(AudioObjectID inObjectID,  UInt32 inNumberAddresses, const AudioObjectPropertyAddress inAddresses[], void*inClientData)
{
    VLC_UNUSED(inObjectID);
    VLC_UNUSED(inNumberAddresses);
    VLC_UNUSED(inAddresses);

    audio_output_t *p_aout = (audio_output_t *)inClientData;
    if (!p_aout)
        return -1;

    msg_Warn(p_aout, "audio device died, resetting aout");
    aout_RestartRequest(p_aout, AOUT_RESTART_OUTPUT);

    return noErr;
}

/*
 * Callback when streams of any audio device changed (e.g. SPDIF gets (un)available)
 */
static OSStatus StreamsChangedListener(AudioObjectID inObjectID,  UInt32 inNumberAddresses, const AudioObjectPropertyAddress inAddresses[], void*inClientData)
{
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0;
    AudioStreamID               *p_streams = NULL;
    int                         i_streams = 0;

    VLC_UNUSED(inNumberAddresses);
    VLC_UNUSED(inAddresses);

    audio_output_t *p_aout = (audio_output_t *)inClientData;
    if (!p_aout)
        return -1;

    aout_sys_t *p_sys = p_aout->sys;

    msg_Dbg(p_aout, "available physical formats for audio device changed");
    RebuildDeviceList(p_aout);

    /*
     * check if changed stream id belongs to current device
     */
    vlc_mutex_lock(&p_sys->var_lock);
    AudioObjectPropertyAddress streamsAddress = { kAudioDevicePropertyStreams, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    err = AudioObjectGetPropertyDataSize(p_sys->i_selected_dev, &streamsAddress, 0, NULL, &i_param_size);
    if (err != noErr) {
        msg_Err(p_aout, "could not get number of streams [%4.4s]", (char *)&err);
        return VLC_EGENERIC;
    }

    i_streams = i_param_size / sizeof(AudioStreamID);
    p_streams = (AudioStreamID *)malloc(i_param_size);
    if (p_streams == NULL)
        return VLC_ENOMEM;

    err = AudioObjectGetPropertyData(p_sys->i_selected_dev, &streamsAddress, 0, NULL, &i_param_size, p_streams);
    if (err != noErr) {
        msg_Err(p_aout, "could not get list of streams [%4.4s]", (char *)&err);
        return VLC_EGENERIC;
    }
    vlc_mutex_unlock(&p_sys->var_lock);

    for (int i = 0; i < i_streams; i++) {
        if (p_streams[i] == inObjectID) {
            msg_Dbg(p_aout, "Restart aout as this affects current device");
            aout_RestartRequest(p_aout, AOUT_RESTART_OUTPUT);
            break;
        }
    }
    free(p_streams);

    return noErr;
}

/*
 * StreamListener: check whether the device's physical format changes on-the-fly (unlikely)
 */
static OSStatus StreamListener(AudioObjectID inObjectID,  UInt32 inNumberAddresses, const AudioObjectPropertyAddress inAddresses[], void*inClientData)
{
    OSStatus err = noErr;
    struct { vlc_mutex_t lock; vlc_cond_t cond; } * w = inClientData;

    VLC_UNUSED(inObjectID);

    for (unsigned int i = 0; i < inNumberAddresses; i++) {
        if (inAddresses[i].mSelector == kAudioStreamPropertyPhysicalFormat) {
            vlc_mutex_lock(&w->lock);
            vlc_cond_signal(&w->cond);
            vlc_mutex_unlock(&w->lock);
            break;
        }
    }
    return err;
}

#pragma mark -
#pragma mark helpers

static int ManageAudioStreamsCallback(audio_output_t *p_aout, AudioDeviceID i_dev_id, bool b_register)
{
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0;
    AudioStreamID               *p_streams = NULL;
    int                         i_streams = 0;

    /* Retrieve all the output streams */
    AudioObjectPropertyAddress streamsAddress = { kAudioDevicePropertyStreams, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    err = AudioObjectGetPropertyDataSize(i_dev_id, &streamsAddress, 0, NULL, &i_param_size);
    if (err != noErr) {
        msg_Err(p_aout, "could not get number of streams for device id %i [%4.4s]", i_dev_id, (char *)&err);
        return VLC_EGENERIC;
    }

    i_streams = i_param_size / sizeof(AudioStreamID);
    p_streams = (AudioStreamID *)malloc(i_param_size);
    if (p_streams == NULL)
        return VLC_ENOMEM;

    err = AudioObjectGetPropertyData(i_dev_id, &streamsAddress, 0, NULL, &i_param_size, p_streams);
    if (err != noErr) {
        msg_Err(p_aout, "could not get list of streams [%4.4s]", (char *)&err);
        return VLC_EGENERIC;
    }

    for (int i = 0; i < i_streams; i++) {
        /* get notified when physical formats change */
        AudioObjectPropertyAddress physicalFormatsAddress = { kAudioStreamPropertyAvailablePhysicalFormats, kAudioObjectPropertyScopeGlobal, 0 };

        if (b_register) {
            err = AudioObjectAddPropertyListener(p_streams[i], &physicalFormatsAddress, StreamsChangedListener, (void *)p_aout);
            if (err != noErr) {
                // nope just means that we already have a callback
                if (err == kAudioHardwareIllegalOperationError) {
                    msg_Warn(p_aout, "could not set audio stream formats property callback on stream id %i, callback already set? [%4.4s]", p_streams[i],
                             (char *)&err);
                } else {
                    msg_Err(p_aout, "could not set audio stream formats property callback on stream id %i [%4.4s]", p_streams[i],
                            (char *)&err);
                }
            }

        } else {  /* unregister callback */
            err = AudioObjectRemovePropertyListener(p_streams[i], &physicalFormatsAddress, StreamsChangedListener, (void *)p_aout);
            if (err != noErr)
                msg_Err(p_aout, "failed to remove audio device property streams callback [%4.4s]", (char *)&err);
        }

    }

    free(p_streams);
    return VLC_SUCCESS;
}

/*
 * AudioDeviceHasOutput: Checks if the device is actually an output device
 */
static int AudioDeviceHasOutput(AudioDeviceID i_dev_id)
{
    UInt32 dataSize = 0;
    OSStatus status;

    AudioObjectPropertyAddress streamsAddress = { kAudioDevicePropertyStreams, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    status = AudioObjectGetPropertyDataSize(i_dev_id, &streamsAddress, 0, NULL, &dataSize);

    if (dataSize == 0 || status != noErr)
        return FALSE;

    return TRUE;
}

/*
 * AudioDeviceSupportsDigital: Checks if device supports raw bitstreams
 */
static int AudioDeviceSupportsDigital(audio_output_t *p_aout, AudioDeviceID i_dev_id)
{
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0;
    AudioStreamID               *p_streams = NULL;
    int                         i_streams = 0;
    bool                        b_return = false;

    /* Retrieve all the output streams */
    AudioObjectPropertyAddress streamsAddress = { kAudioDevicePropertyStreams, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    err = AudioObjectGetPropertyDataSize(i_dev_id, &streamsAddress, 0, NULL, &i_param_size);
    if (err != noErr) {
        msg_Err(p_aout, "could not get number of streams [%4.4s] (%i)", (char *)&err, (int32_t)err);
        return false;
    }

    i_streams = i_param_size / sizeof(AudioStreamID);
    p_streams = (AudioStreamID *)malloc(i_param_size);
    if (p_streams == NULL)
        return VLC_ENOMEM;

    err = AudioObjectGetPropertyData(i_dev_id, &streamsAddress, 0, NULL, &i_param_size, p_streams);
    if (err != noErr) {
        msg_Err(p_aout, "could not get list of streams [%4.4s]", (char *)&err);
        return false;
    }

    for (int i = 0; i < i_streams; i++) {
        if (AudioStreamSupportsDigital(p_aout, p_streams[i]))
            b_return = true;
    }

    free(p_streams);
    return b_return;
}

/*
 * AudioStreamSupportsDigital: Checks if audio stream is compatible with raw bitstreams
 */
static int AudioStreamSupportsDigital(audio_output_t *p_aout, AudioStreamID i_stream_id)
{
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0;
    AudioStreamRangedDescription *p_format_list = NULL;
    int                         i_formats = 0;
    bool                        b_return = false;

    /* Retrieve all the stream formats supported by each output stream */
    AudioObjectPropertyAddress physicalFormatsAddress = { kAudioStreamPropertyAvailablePhysicalFormats, kAudioObjectPropertyScopeGlobal, 0 };
    err = AudioObjectGetPropertyDataSize(i_stream_id, &physicalFormatsAddress, 0, NULL, &i_param_size);
    if (err != noErr) {
        msg_Err(p_aout, "could not get number of streamformats [%4.4s] (%i)", (char *)&err, (int32_t)err);
        return false;
    }

    i_formats = i_param_size / sizeof(AudioStreamRangedDescription);
    msg_Dbg(p_aout, "found %i stream formats for stream id %i", i_formats, i_stream_id);

    p_format_list = (AudioStreamRangedDescription *)malloc(i_param_size);
    if (p_format_list == NULL)
        return false;

    err = AudioObjectGetPropertyData(i_stream_id, &physicalFormatsAddress, 0, NULL, &i_param_size, p_format_list);
    if (err != noErr) {
        msg_Err(p_aout, "could not get the list of streamformats [%4.4s]", (char *)&err);
        free(p_format_list);
        p_format_list = NULL;
        return false;
    }

    for (int i = 0; i < i_formats; i++) {
#ifndef NDEBUG
        msg_Dbg(p_aout, STREAM_FORMAT_MSG("supported format: ", p_format_list[i].mFormat));
#endif

        if (p_format_list[i].mFormat.mFormatID == 'IAC3' ||
            p_format_list[i].mFormat.mFormatID == 'iac3' ||
            p_format_list[i].mFormat.mFormatID == kAudioFormat60958AC3 ||
            p_format_list[i].mFormat.mFormatID == kAudioFormatAC3)
            b_return = true;
    }

    free(p_format_list);
    return b_return;
}

/*
 * AudioStreamChangeFormat: switch stream format based on the provided description
 */
static int AudioStreamChangeFormat(audio_output_t *p_aout, AudioStreamID i_stream_id, AudioStreamBasicDescription change_format)
{
    OSStatus            err = noErr;
    UInt32              i_param_size = 0;

    AudioObjectPropertyAddress physicalFormatAddress = { kAudioStreamPropertyPhysicalFormat, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };

    struct { vlc_mutex_t lock; vlc_cond_t cond; } w;

    msg_Dbg(p_aout, STREAM_FORMAT_MSG("setting stream format: ", change_format));

    /* Condition because SetProperty is asynchronious */
    vlc_cond_init(&w.cond);
    vlc_mutex_init(&w.lock);
    vlc_mutex_lock(&w.lock);

    /* Install the callback */
    err = AudioObjectAddPropertyListener(i_stream_id, &physicalFormatAddress, StreamListener, (void *)&w);
    if (err != noErr) {
        msg_Err(p_aout, "AudioObjectAddPropertyListener for kAudioStreamPropertyPhysicalFormat failed [%4.4s]", (char *)&err);
        return false;
    }

    /* change the format */
    err = AudioObjectSetPropertyData(i_stream_id, &physicalFormatAddress, 0, NULL, sizeof(AudioStreamBasicDescription),
                                     &change_format);
    if (err != noErr) {
        msg_Err(p_aout, "could not set the stream format [%4.4s]", (char *)&err);
        return false;
    }

    /* The AudioStreamSetProperty is not only asynchronious (requiring the locks)
     * it is also not atomic in its behaviour.
     * Therefore we check 5 times before we really give up.
     * FIXME: failing isn't actually implemented yet. */
    for (int i = 0; i < 5; i++) {
        AudioStreamBasicDescription actual_format;
        mtime_t timeout = mdate() + 500000;

        if (vlc_cond_timedwait(&w.cond, &w.lock, timeout))
            msg_Dbg(p_aout, "reached timeout");

        i_param_size = sizeof(AudioStreamBasicDescription);
        err = AudioObjectGetPropertyData(i_stream_id, &physicalFormatAddress, 0, NULL, &i_param_size, &actual_format);

        msg_Dbg(p_aout, STREAM_FORMAT_MSG("actual format in use: ", actual_format));
        if (actual_format.mSampleRate == change_format.mSampleRate &&
            actual_format.mFormatID == change_format.mFormatID &&
            actual_format.mFramesPerPacket == change_format.mFramesPerPacket) {
            /* The right format is now active */
            break;
        }
        /* We need to check again */
    }

    /* Removing the property listener */
    err = AudioObjectRemovePropertyListener(i_stream_id, &physicalFormatAddress, StreamListener, (void *)&w);
    if (err != noErr) {
        msg_Err(p_aout, "AudioStreamRemovePropertyListener failed [%4.4s]", (char *)&err);
        return false;
    }

    /* Destroy the lock and condition */
    vlc_mutex_unlock(&w.lock);
    vlc_mutex_destroy(&w.lock);
    vlc_cond_destroy(&w.cond);

    return true;
}
