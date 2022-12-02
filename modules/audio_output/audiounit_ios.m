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
#pragma mark private declarations

/* aout wrapper: used as observer for notifications */
@interface AoutWrapper : NSObject
- (instancetype)initWithAout:(audio_output_t *)aout;
@property (readonly, assign) audio_output_t* aout;
@end

enum au_dev
{
    AU_DEV_PCM,
    AU_DEV_ENCODED,
};

static const struct {
    const char *psz_id;
    const char *psz_name;
    enum au_dev au_dev;
} au_devs[] = {
    { "pcm", "Up to 9 channels PCM output", AU_DEV_PCM },
    { "encoded", "Encoded output if available (via HDMI/SPDIF) or PCM output",
      AU_DEV_ENCODED }, /* This can also be forced with the --spdif option */
};

#if ((__IPHONE_OS_VERSION_MAX_ALLOWED && __IPHONE_OS_VERSION_MAX_ALLOWED < 150000) || (__TV_OS_MAX_VERSION_ALLOWED && __TV_OS_MAX_VERSION_ALLOWED < 150000))

extern NSString *const AVAudioSessionSpatialAudioEnabledKey = @"AVAudioSessionSpatializationEnabledKey";
extern NSString *const AVAudioSessionSpatialPlaybackCapabilitiesChangedNotification = @"AVAudioSessionSpatialPlaybackCapabilitiesChangedNotification";

@interface AVAudioSession (iOS15RoutingConfiguration)
- (BOOL)setSupportsMultichannelContent:(BOOL)inValue error:(NSError **)outError;
@end

@interface AVAudioSessionPortDescription (iOS15RoutingConfiguration)
@property (readonly, getter=isSpatialAudioEnabled) BOOL spatialAudioEnabled;
@end

#endif

@interface SessionManager : NSObject
{
    NSMutableSet *_registeredInstances;
}
+ (SessionManager *)sharedInstance;
- (void)addAoutInstance:(AoutWrapper *)wrapperInstance;
- (NSInteger)removeAoutInstance:(AoutWrapper *)wrapperInstance;
@end

@implementation SessionManager
+ (SessionManager *)sharedInstance
{
    static SessionManager *sharedInstance = nil;
    static dispatch_once_t pred;

    dispatch_once(&pred, ^{
        sharedInstance = [SessionManager new];
    });

    return sharedInstance;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _registeredInstances = [[NSMutableSet alloc] init];
    }
    return self;
}

- (void)addAoutInstance:(AoutWrapper *)wrapperInstance
{
    @synchronized(_registeredInstances) {
        [_registeredInstances addObject:wrapperInstance];
    }
}

- (NSInteger)removeAoutInstance:(AoutWrapper *)wrapperInstance
{
    @synchronized(_registeredInstances) {
        [_registeredInstances removeObject:wrapperInstance];
        return _registeredInstances.count;
    }
}
@end

/*****************************************************************************
 * aout_sys_t: private audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the CoreAudio specific properties of an output thread.
 *****************************************************************************/
typedef struct
{
    struct aout_sys_common c;

    AVAudioSession *avInstance;
    AoutWrapper *aoutWrapper;
    /* The AudioUnit we use */
    AudioUnit au_unit;
    bool      b_muted;
    bool      b_stopped;
    bool      b_preferred_channels_set;
    bool      b_spatial_audio_supported;
    enum au_dev au_dev;

    /* For debug purpose, to print when specific latency changed */
    vlc_tick_t output_latency_ticks;
    vlc_tick_t io_buffer_duration_ticks;

    /* sw gain */
    float               soft_gain;
    bool                soft_mute;
} aout_sys_t;

/* Soft volume helper */
#include "audio_output/volume.h"

enum port_type
{
    PORT_TYPE_DEFAULT,
    PORT_TYPE_USB,
    PORT_TYPE_HDMI,
    PORT_TYPE_HEADPHONES
};

static vlc_tick_t
GetLatency(audio_output_t *p_aout)
{
    aout_sys_t *p_sys = p_aout->sys;

    Float64 unit_s;
    vlc_tick_t latency_us = 0, us;
    bool changed = false;

    us = vlc_tick_from_sec([p_sys->avInstance outputLatency]);
    if (us != p_sys->output_latency_ticks)
    {
        msg_Dbg(p_aout, "Current device has a new outputLatency of %" PRId64 "us", us);
        p_sys->output_latency_ticks = us;
        changed = true;
    }
    latency_us += us;

    us = vlc_tick_from_sec([p_sys->avInstance IOBufferDuration]);
    if (us != p_sys->io_buffer_duration_ticks)
    {
        msg_Dbg(p_aout, "Current device has a new IOBufferDuration of %" PRId64 "us", us);
        p_sys->io_buffer_duration_ticks = us;
        changed = true;
    }
    latency_us += us;

    if (changed)
        msg_Dbg(p_aout, "Current device has a new total latency of %" PRId64 "us",
                latency_us);
    return latency_us;
}

#pragma mark -
#pragma mark AVAudioSession route and output handling

@implementation AoutWrapper

- (instancetype)initWithAout:(audio_output_t *)aout
{
    self = [super init];
    if (self)
        _aout = aout;
    return self;
}

- (void)audioSessionRouteChange:(NSNotification *)notification
{
    audio_output_t *p_aout = [self aout];
    aout_sys_t *p_sys = p_aout->sys;
    NSDictionary *userInfo = notification.userInfo;
    NSInteger routeChangeReason =
        [[userInfo valueForKey:AVAudioSessionRouteChangeReasonKey] integerValue];

    msg_Dbg(p_aout, "Audio route changed: %ld", (long) routeChangeReason);

    if (routeChangeReason == AVAudioSessionRouteChangeReasonNewDeviceAvailable
     || routeChangeReason == AVAudioSessionRouteChangeReasonOldDeviceUnavailable)
        aout_RestartRequest(p_aout, AOUT_RESTART_OUTPUT);
    else
        ca_ResetDeviceLatency(p_aout);
}

- (void)handleInterruption:(NSNotification *)notification
{
    audio_output_t *p_aout = [self aout];
    NSDictionary *userInfo = notification.userInfo;
    if (!userInfo || !userInfo[AVAudioSessionInterruptionTypeKey]) {
        return;
    }

    NSUInteger interruptionType = [userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];

    if (interruptionType == AVAudioSessionInterruptionTypeBegan) {
        ca_SetAliveState(p_aout, false);
    } else if (interruptionType == AVAudioSessionInterruptionTypeEnded
               && [userInfo[AVAudioSessionInterruptionOptionKey] unsignedIntegerValue] == AVAudioSessionInterruptionOptionShouldResume) {
        ca_SetAliveState(p_aout, true);
    }
}

- (void)handleSpatialCapabilityChange:(NSNotification *)notification
{
    if (@available(iOS 15.0, tvOS 15.0, *)) {
        audio_output_t *p_aout = [self aout];
        struct aout_sys_t *p_sys = p_aout->sys;
        NSDictionary *userInfo = notification.userInfo;
        BOOL spatialAudioEnabled =
            [[userInfo valueForKey:AVAudioSessionSpatialAudioEnabledKey] boolValue];

        msg_Dbg(p_aout, "Spatial Audio availability changed: %i", spatialAudioEnabled);

        if (spatialAudioEnabled) {
            aout_RestartRequest(p_aout, AOUT_RESTART_OUTPUT);
        }
    }
}
@end

static void
avas_setPreferredNumberOfChannels(audio_output_t *p_aout,
                                  const audio_sample_format_t *fmt)
{
    aout_sys_t *p_sys = p_aout->sys;

    if (aout_BitsPerSample(fmt->i_format) == 0)
        return; /* Don't touch the number of channels for passthrough */

    AVAudioSession *instance = p_sys->avInstance;
    NSInteger max_channel_count = [instance maximumOutputNumberOfChannels];
    unsigned channel_count = aout_FormatNbChannels(fmt);

    /* Increase the preferred number of output channels if possible */
    if (channel_count > 2 && max_channel_count > 2)
    {
        channel_count = __MIN(channel_count, max_channel_count);
        bool success = [instance setPreferredOutputNumberOfChannels:channel_count
                        error:nil];
        if (success && [instance outputNumberOfChannels] == channel_count)
            p_sys->b_preferred_channels_set = true;
        else
        {
            /* Not critical, output channels layout will be Stereo */
            msg_Warn(p_aout, "setPreferredOutputNumberOfChannels failed");
        }
    }
}

static void
avas_resetPreferredNumberOfChannels(audio_output_t *p_aout)
{
    aout_sys_t *p_sys = p_aout->sys;
    AVAudioSession *instance = p_sys->avInstance;

    if (p_sys->b_preferred_channels_set)
    {
        [instance setPreferredOutputNumberOfChannels:2 error:nil];
        p_sys->b_preferred_channels_set = false;
    }
}

static int
avas_GetPortType(audio_output_t *p_aout, enum port_type *pport_type)
{
    aout_sys_t * p_sys = p_aout->sys;
    AVAudioSession *instance = p_sys->avInstance;
    *pport_type = PORT_TYPE_DEFAULT;

    long last_channel_count = 0;
    for (AVAudioSessionPortDescription *out in [[instance currentRoute] outputs])
    {
        /* Choose the layout with the biggest number of channels or the HDMI
         * one */

        enum port_type port_type;
        if ([out.portType isEqualToString: AVAudioSessionPortUSBAudio])
            port_type = PORT_TYPE_USB;
        else if ([out.portType isEqualToString: AVAudioSessionPortHDMI])
            port_type = PORT_TYPE_HDMI;
        else if ([out.portType isEqualToString: AVAudioSessionPortHeadphones])
            port_type = PORT_TYPE_HEADPHONES;
        else
            port_type = PORT_TYPE_DEFAULT;

        if (@available(iOS 15.0, tvOS 15.0, *)) {
            p_sys->b_spatial_audio_supported = out.spatialAudioEnabled;
        }

        *pport_type = port_type;
        if (port_type == PORT_TYPE_HDMI) /* Prefer HDMI */
            break;
    }

    return VLC_SUCCESS;
}

struct role2policy
{
    char role[sizeof("accessibility")];
    AVAudioSessionRouteSharingPolicy policy;
};

static int role2policy_cmp(const void *key, const void *val)
{
    const struct role2policy *entry = val;
    return strcmp(key, entry->role);
}

static AVAudioSessionRouteSharingPolicy
GetRouteSharingPolicy(audio_output_t *p_aout)
{
    /* LongFormAudio by default */
    AVAudioSessionRouteSharingPolicy policy = AVAudioSessionRouteSharingPolicyLongFormAudio;
    AVAudioSessionRouteSharingPolicy video_policy;
#if !TARGET_OS_TV
    if (@available(iOS 13.0, *))
        video_policy = AVAudioSessionRouteSharingPolicyLongFormVideo;
    else
#endif
        video_policy = AVAudioSessionRouteSharingPolicyLongFormAudio;

    char *str = var_InheritString(p_aout, "role");
    if (str != NULL)
    {
        const struct role2policy role_list[] =
        {
            { "accessibility", AVAudioSessionRouteSharingPolicyDefault },
            { "animation",     AVAudioSessionRouteSharingPolicyDefault },
            { "communication", AVAudioSessionRouteSharingPolicyDefault },
            { "game",          AVAudioSessionRouteSharingPolicyLongFormAudio },
            { "music",         AVAudioSessionRouteSharingPolicyLongFormAudio },
            { "notification",  AVAudioSessionRouteSharingPolicyDefault },
            { "production",    AVAudioSessionRouteSharingPolicyDefault },
            { "test",          AVAudioSessionRouteSharingPolicyDefault },
            { "video",         video_policy},
        };

        const struct role2policy *entry =
            bsearch(str, role_list, ARRAY_SIZE(role_list),
                    sizeof (*role_list), role2policy_cmp);
        free(str);
        if (entry != NULL)
            policy = entry->policy;
    }

    return policy;
}


static int
avas_SetActive(audio_output_t *p_aout, bool active, NSUInteger options)
{
    aout_sys_t * p_sys = p_aout->sys;
    AVAudioSession *instance = p_sys->avInstance;
    BOOL ret = false;
    NSError *error = nil;

    if (active)
    {
        AVAudioSessionCategory category = AVAudioSessionCategoryPlayback;
        AVAudioSessionMode mode = AVAudioSessionModeMoviePlayback;
        AVAudioSessionRouteSharingPolicy policy = GetRouteSharingPolicy(p_aout);

        if (@available(iOS 11.0, tvOS 11.0, *))
        {
            ret = [instance setCategory:category
                                   mode:mode
                     routeSharingPolicy:policy
                                options:0
                                  error:&error];
        }
        else
        {
            ret = [instance setCategory:category
                                  error:&error];
            ret = ret && [instance setMode:mode error:&error];
            /* Not AVAudioSessionRouteSharingPolicy on older devices */
        }
        if (@available(iOS 15.0, tvOS 15.0, *)) {
            ret = ret && [instance setSupportsMultichannelContent:p_sys->b_spatial_audio_supported error:&error];
        }
        ret = ret && [instance setActive:YES withOptions:options error:&error];
        if (ret)
            [[SessionManager sharedInstance] addAoutInstance: p_sys->aoutWrapper];
    } else {
        NSInteger numberOfRegisteredInstances = [[SessionManager sharedInstance] removeAoutInstance: p_sys->aoutWrapper];
        if (numberOfRegisteredInstances == 0) {
            ret = [instance setActive:NO withOptions:options error:&error];
        } else {
            ret = true;
        }
    }

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
Pause (audio_output_t *p_aout, bool pause, vlc_tick_t date)
{
    aout_sys_t * p_sys = p_aout->sys;

    /* We need to start / stop the audio unit here because otherwise the OS
     * won't believe us that we stopped the audio output so in case of an
     * interruption, our unit would be permanently silenced. In case of
     * multi-tasking, the multi-tasking view would still show a playing state
     * despite we are paused, same for lock screen */

    if (pause == p_sys->b_stopped)
        return;

    OSStatus err;
    if (pause)
    {
        err = AudioOutputUnitStop(p_sys->au_unit);
        if (err != noErr)
            ca_LogErr("AudioOutputUnitStop failed");
        avas_SetActive(p_aout, false, 0);
    }
    else
    {
        if (avas_SetActive(p_aout, true, 0) == VLC_SUCCESS)
        {
            err = AudioOutputUnitStart(p_sys->au_unit);
            if (err != noErr)
            {
                ca_LogErr("AudioOutputUnitStart failed");
                avas_SetActive(p_aout, false, 0);
                /* Do not un-pause, the Render Callback won't run, and next call
                 * of ca_Play will deadlock */
                return;
            }
        }
    }
    p_sys->b_stopped = pause;
    ca_Pause(p_aout, pause, date);
}

static int
MuteSet(audio_output_t *p_aout, bool mute)
{
    ca_MuteSet(p_aout, mute);
    aout_MuteReport(p_aout, mute);

    return VLC_SUCCESS;
}

static void
Play(audio_output_t * p_aout, block_t * p_block, vlc_tick_t date)
{
    aout_sys_t * p_sys = p_aout->sys;

    if (p_sys->b_muted)
        block_Release(p_block);
    else
        ca_Play(p_aout, p_block, date);
}

#pragma mark initialization

static void
Stop(audio_output_t *p_aout)
{
    aout_sys_t   *p_sys = p_aout->sys;
    OSStatus err;

    [[NSNotificationCenter defaultCenter] removeObserver:p_sys->aoutWrapper];

    if (!p_sys->b_stopped)
    {
        err = AudioOutputUnitStop(p_sys->au_unit);
        if (err != noErr)
            ca_LogWarn("AudioOutputUnitStop failed");
    }

    au_Uninitialize(p_aout, p_sys->au_unit);

    err = AudioComponentInstanceDispose(p_sys->au_unit);
    if (err != noErr)
        ca_LogWarn("AudioComponentInstanceDispose failed");

    avas_resetPreferredNumberOfChannels(p_aout);

    avas_SetActive(p_aout, false,
                   AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation);
}

static int
Start(audio_output_t *p_aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t *p_sys = p_aout->sys;
    OSStatus err;
    OSStatus status;
    AudioChannelLayout *layout = NULL;

    if (aout_FormatNbChannels(fmt) == 0 || AOUT_FMT_HDMI(fmt))
        return VLC_EGENERIC;

    /* XXX: No more passthrough since iOS 11 */
    if (AOUT_FMT_SPDIF(fmt))
        return VLC_EGENERIC;

    aout_FormatPrint(p_aout, "VLC is looking for:", fmt);

    p_sys->au_unit = NULL;
    p_sys->output_latency_ticks = 0;
    p_sys->io_buffer_duration_ticks = 0;

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:p_sys->aoutWrapper
                           selector:@selector(audioSessionRouteChange:)
                               name:AVAudioSessionRouteChangeNotification
                             object:nil];
    [notificationCenter addObserver:p_sys->aoutWrapper
                           selector:@selector(handleInterruption:)
                               name:AVAudioSessionInterruptionNotification
                             object:nil];
    if (@available(iOS 15.0, tvOS 15.0, *)) {
        [notificationCenter addObserver:p_sys->aoutWrapper
                               selector:@selector(handleSpatialCapabilityChange:)
                                   name:AVAudioSessionSpatialPlaybackCapabilitiesChangedNotification
                                 object:nil];
    }

    /* Activate the AVAudioSession */
    if (avas_SetActive(p_aout, true, 0) != VLC_SUCCESS)
    {
        [[NSNotificationCenter defaultCenter] removeObserver:p_sys->aoutWrapper];
        return VLC_EGENERIC;
    }

    /* Set the preferred number of channels, then fetch the channel layout that
     * should correspond to this number */
    avas_setPreferredNumberOfChannels(p_aout, fmt);

    BOOL success = [p_sys->avInstance setPreferredSampleRate:fmt->i_rate error:nil];
    if (!success)
    {
        /* Not critical, we can use any sample rates */
        msg_Dbg(p_aout, "failed to set preferred sample rate");
    }

    enum port_type port_type;
    int ret = avas_GetPortType(p_aout, &port_type);
    if (ret != VLC_SUCCESS)
        goto error;

    msg_Dbg(p_aout, "Output on %s, channel count: %ld, spatialAudioEnabled %i",
            port_type == PORT_TYPE_HDMI ? "HDMI" :
            port_type == PORT_TYPE_USB ? "USB" :
            port_type == PORT_TYPE_HEADPHONES ? "Headphones" : "Default",
            (long) [p_sys->avInstance outputNumberOfChannels],
            p_sys->b_spatial_audio_supported);

    p_aout->current_sink_info.headphones = port_type == PORT_TYPE_HEADPHONES;

    p_sys->au_unit = au_NewOutputInstance(p_aout, kAudioUnitSubType_RemoteIO);
    if (p_sys->au_unit == NULL)
        goto error;

    err = AudioUnitSetProperty(p_sys->au_unit,
                               kAudioOutputUnitProperty_EnableIO,
                               kAudioUnitScope_Output, 0,
                               &(UInt32){ 1 }, sizeof(UInt32));
    if (err != noErr)
        ca_LogWarn("failed to set IO mode");

    ret = au_Initialize(p_aout, p_sys->au_unit, fmt, NULL, 0, GetLatency, NULL);
    if (ret != VLC_SUCCESS)
        goto error;

    p_aout->play = Play;

    err = AudioOutputUnitStart(p_sys->au_unit);
    if (err != noErr)
    {
        ca_LogErr("AudioOutputUnitStart failed");
        au_Uninitialize(p_aout, p_sys->au_unit);
        goto error;
    }

    if (p_sys->b_muted)
        Pause(p_aout, true, 0);

    free(layout);
    fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
    p_aout->pause = Pause;

    aout_SoftVolumeStart( p_aout );

    msg_Dbg(p_aout, "analog AudioUnit output successfully opened for %4.4s %s",
            (const char *)&fmt->i_format, aout_FormatPrintChannels(fmt));
    return VLC_SUCCESS;

error:
    if (p_sys->au_unit != NULL)
        AudioComponentInstanceDispose(p_sys->au_unit);
    avas_resetPreferredNumberOfChannels(p_aout);
    avas_SetActive(p_aout, false,
                   AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation);
    [[NSNotificationCenter defaultCenter] removeObserver:p_sys->aoutWrapper];
    msg_Err(p_aout, "opening AudioUnit output failed");
    return VLC_EGENERIC;
}

static int DeviceSelect(audio_output_t *p_aout, const char *psz_id)
{
    aout_sys_t *p_sys = p_aout->sys;
    enum au_dev au_dev = AU_DEV_PCM;

    if (psz_id)
    {
        for (unsigned int i = 0; i < sizeof(au_devs) / sizeof(au_devs[0]); ++i)
        {
            if (!strcmp(psz_id, au_devs[i].psz_id))
            {
                au_dev = au_devs[i].au_dev;
                break;
            }
        }
    }

    if (au_dev != p_sys->au_dev)
    {
        p_sys->au_dev = au_dev;
        aout_RestartRequest(p_aout, AOUT_RESTART_OUTPUT);
        msg_Dbg(p_aout, "selected audiounit device: %s", psz_id);
    }
    aout_DeviceReport(p_aout, psz_id);
    return VLC_SUCCESS;
}

static void
Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    [sys->aoutWrapper release];

    free(sys);
}

static int
Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    aout_sys_t *sys = aout->sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    if (ca_Open(aout) != VLC_SUCCESS)
    {
        free(sys);
        return VLC_EGENERIC;
    }

    sys->avInstance = [AVAudioSession sharedInstance];
    assert(sys->avInstance != NULL);

    sys->aoutWrapper = [[AoutWrapper alloc] initWithAout:aout];
    if (sys->aoutWrapper == NULL)
    {
        free(sys);
        return VLC_ENOMEM;
    }

    sys->b_muted = false;
    sys->b_preferred_channels_set = false;
    sys->b_spatial_audio_supported = false;
    sys->au_dev = var_InheritBool(aout, "spdif") ? AU_DEV_ENCODED : AU_DEV_PCM;
    aout->start = Start;
    aout->stop = Stop;
    aout->mute_set  = MuteSet;
    aout->device_select = DeviceSelect;

    aout_SoftVolumeInit( aout );

    for (unsigned int i = 0; i< sizeof(au_devs) / sizeof(au_devs[0]); ++i)
        aout_HotplugReport(aout, au_devs[i].psz_id, au_devs[i].psz_name);

    return VLC_SUCCESS;
}

#pragma mark -
#pragma mark module descriptor

vlc_module_begin ()
    set_shortname("audiounit_ios")
    set_description("AudioUnit output for iOS")
    set_capability("audio output", 101)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    add_sw_gain()
    set_callbacks(Open, Close)
vlc_module_end ()
