/*****************************************************************************
 * avaudiosession_common.m: AVAudioSession common code for iOS aouts
 *****************************************************************************
 * Copyright (C) 2012 - 2024 VLC authors and VideoLAN
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

#import "config.h"

#import <vlc_common.h>
#import <vlc_aout.h>
#import <vlc_atomic.h>

#import <AVFoundation/AVFoundation.h>
#import "avaudiosession_common.h"

// work-around to fix compilation on older Xcode releases
#if defined(TARGET_OS_VISION) && TARGET_OS_VISION
#define MIN_VISIONOS 1.0
#define VISIONOS_API_AVAILABLE , visionos(MIN_VISIONOS)
#else
#define VISIONOS_API_AVAILABLE
#endif

void
avas_PrepareFormat(audio_output_t *p_aout, AVAudioSession *instance,
                   audio_sample_format_t *fmt, bool spatial_audio)
{
    if (aout_BitsPerSample(fmt->i_format) == 0)
        return; /* Don't touch the number of channels for passthrough */

    NSInteger max_channel_count = [instance maximumOutputNumberOfChannels];
    unsigned channel_count = aout_FormatNbChannels(fmt);

    /* Increase the preferred number of output channels if possible */
    if (channel_count > max_channel_count)
    {
        if (!spatial_audio)
            msg_Warn(p_aout, "Requested channel count %u not fully supported, "
                     "downmixing to %ld\n", channel_count, (long)max_channel_count);
        channel_count = max_channel_count;
    }

#if !TARGET_OS_WATCH
    NSError *error = nil;
    BOOL success = [instance setPreferredOutputNumberOfChannels:channel_count
                                                          error:&error];
    if (!success || [instance outputNumberOfChannels] != channel_count)
    {
        /* Not critical, output channels layout will be Stereo */
        msg_Warn(p_aout, "setPreferredOutputNumberOfChannels failed %s(%d)",
                 !success ? error.domain.UTF8String : "",
                 !success ? (int)error.code : 0);
        channel_count = 2;
    }
#else
    channel_count = 2;
#endif

    if (spatial_audio)
    {
        if (@available(iOS 15.0, watchOS 8.0, tvOS 15.0, *))
        {
            /* Not mandatory, SpatialAudio can work without it. It just signals to
             * the user that he is playing spatial content */
            [instance setSupportsMultichannelContent:aout_FormatNbChannels(fmt) > 2
                                               error:nil];
        }
    }
    else if (channel_count == 2 && aout_FormatNbChannels(fmt) > 2)
    {
        /* Ask the core to downmix to stereo if the preferred number of
         * channels can't be set. */
        fmt->i_physical_channels = AOUT_CHANS_STEREO;
        aout_FormatPrepare(fmt);
    }

#if !TARGET_OS_WATCH
    success = [instance setPreferredSampleRate:fmt->i_rate error:&error];
    if (!success)
    {
        /* Not critical, we can use any sample rates */
        msg_Dbg(p_aout, "setPreferredSampleRate failed %s(%d)",
                error.domain.UTF8String, (int)error.code);
    }
#endif
}

int
avas_GetPortType(audio_output_t *p_aout, AVAudioSession *instance,
                 enum port_type *pport_type)
{
    (void) p_aout;
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

        *pport_type = port_type;
        if (port_type == PORT_TYPE_HDMI) /* Prefer HDMI */
            break;
    }

    return VLC_SUCCESS;
}

struct API_AVAILABLE(ios(11.0), watchos(7.0) VISIONOS_API_AVAILABLE)
role2policy
{
    char role[sizeof("accessibility")];
    AVAudioSessionRouteSharingPolicy policy;
};

static int API_AVAILABLE(ios(11.0), watchos(7.0) VISIONOS_API_AVAILABLE)
role2policy_cmp(const void *key, const void *val)
{
    const struct role2policy *entry = val;
    return strcmp(key, entry->role);
}

static AVAudioSessionRouteSharingPolicy API_AVAILABLE(ios(11.0), watchos(7.0) VISIONOS_API_AVAILABLE)
GetRouteSharingPolicy(audio_output_t *p_aout)
{
#if __IPHONEOS_VERSION_MAX_ALLOWED < 130000
    AVAudioSessionRouteSharingPolicy AVAudioSessionRouteSharingPolicyLongFormAudio =
        AVAudioSessionRouteSharingPolicyLongForm;
    AVAudioSessionRouteSharingPolicy AVAudioSessionRouteSharingPolicyLongFormVideo =
        AVAudioSessionRouteSharingPolicyLongForm;
#endif
    /* LongFormAudio by default */
    AVAudioSessionRouteSharingPolicy policy = AVAudioSessionRouteSharingPolicyLongFormAudio;
    AVAudioSessionRouteSharingPolicy video_policy;
#if TARGET_OS_IOS || TARGET_OS_VISION
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

int
avas_SetActive(audio_output_t *p_aout, AVAudioSession *instance, bool active,
               NSUInteger options)
{
    static vlc_atomic_rc_t active_rc = VLC_STATIC_RC;

    BOOL ret = false;
    NSError *error = nil;

    if (active)
    {
        if (@available(iOS 11.0, watchOS 7.0, tvOS 11.0, *))
        {
            AVAudioSessionRouteSharingPolicy policy = GetRouteSharingPolicy(p_aout);

            ret = [instance setCategory:AVAudioSessionCategoryPlayback
                                   mode:AVAudioSessionModeMoviePlayback
                     routeSharingPolicy:policy
                                options:0
                                  error:&error];
        }
        else
        {
            ret = [instance setCategory:AVAudioSessionCategoryPlayback
                                  error:&error];
            ret = ret && [instance setMode:AVAudioSessionModeMoviePlayback
                                     error:&error];
            /* Not AVAudioSessionRouteSharingPolicy on older devices */
        }
        ret = ret && [instance setActive:YES withOptions:options error:&error];
#if TARGET_OS_VISION
        ret = ret && [instance setIntendedSpatialExperience:AVAudioSessionSpatialExperienceFixed
                                                    options:nil
                                                      error:&error];
#endif
        if (ret)
            vlc_atomic_rc_inc(&active_rc);
    } else {
        if (vlc_atomic_rc_dec(&active_rc))
            ret = [instance setActive:NO withOptions:options error:&error];
        else
            ret = true;
    }

    if (!ret)
    {
        msg_Err(p_aout, "AVAudioSession playback change failed: %s(%d)",
                error.domain.UTF8String, (int)error.code);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}
