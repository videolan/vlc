/*****************************************************************************
 * windows_audio_common.h: Windows Audio common code
 *****************************************************************************
 * Copyright (C) 2001-2013 VideoLAN
 *
 * Authors: Denis Charmet <typx@videolan.org>
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
#ifndef DMX_WINDOWS_AUDIO_COMMONS_H
#define DMX_WINDOWS_AUDIO_COMMONS_H

#include <vlc_aout.h>
#include <vlc_codecs.h>

static const uint32_t pi_channels_src[] = { WAVE_SPEAKER_FRONT_LEFT,
    WAVE_SPEAKER_FRONT_RIGHT, WAVE_SPEAKER_FRONT_CENTER,
    WAVE_SPEAKER_LOW_FREQUENCY, WAVE_SPEAKER_BACK_LEFT, WAVE_SPEAKER_BACK_RIGHT,
    WAVE_SPEAKER_BACK_CENTER, WAVE_SPEAKER_SIDE_LEFT, WAVE_SPEAKER_SIDE_RIGHT, 0 };

static const uint32_t pi_channels_aout[] = { AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER, AOUT_CHAN_LFE, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_REARCENTER, AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT, 0 };

static inline unsigned getChannelMask( uint32_t * wvfextChannelMask, int i_channels, int * i_match )
{
    unsigned i_channel_mask = 0;
    *i_match = 0;
    for( unsigned i = 0;
         i < sizeof(pi_channels_src)/sizeof(*pi_channels_src) &&
         *i_match < i_channels; i++ )
    {
        if( *wvfextChannelMask & pi_channels_src[i] )
        {
            if( !( i_channel_mask & pi_channels_aout[i] ) )
                 *i_match += 1;

            *wvfextChannelMask &= ~pi_channels_src[i];
            i_channel_mask |= pi_channels_aout[i];
        }
    }
    return i_channel_mask;
}

#endif /*DMX_WINDOWS_AUDIO_COMMONS_H*/
