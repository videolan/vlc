/*****************************************************************************
 * channel_layout.c: Common Channel Layout code for iOS and macOS
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_aout.h>
#include "channel_layout.h"

#include <stdckdint.h>
#include <CoreAudio/CoreAudioTypes.h>

static AudioChannelLabel
VlcChanToAudioChannelLabel(unsigned chan, bool swap_rear_surround)
{
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

int
channel_layout_MapFromVLC(audio_output_t *p_aout, const audio_sample_format_t *fmt,
                          AudioChannelLayout **inlayoutp, size_t *inlayout_size)
{
    unsigned channels = aout_FormatNbChannels(fmt);
    size_t size;

    /* Stereo on xros via avsb doesn't work with the UseChannelDescriptions
     * tag, so use Mono and Stereo tags. */
    if (channels <= 2)
    {
        size = sizeof(AudioChannelLayout);
        AudioChannelLayout *inlayout = malloc(size);
        if (inlayout == NULL)
            return VLC_ENOMEM;
        *inlayoutp = inlayout;
        *inlayout_size = size;

        if (channels == 1)
            inlayout->mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
        else
            inlayout->mChannelLayoutTag =
                p_aout->current_sink_info.headphones ?
                    kAudioChannelLayoutTag_StereoHeadphones :
                    kAudioChannelLayoutTag_Stereo;

        return VLC_SUCCESS;
    }

    if (ckd_mul(&size, channels, sizeof(AudioChannelDescription)) ||
        ckd_add(&size, size, sizeof(AudioChannelLayout)))
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
