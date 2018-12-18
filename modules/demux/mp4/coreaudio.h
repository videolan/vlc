/*****************************************************************************
 * coreaudio.h : CoreAudio definitions for vlc
 *****************************************************************************
 * Copyright (C) 2014-2018 VideoLabs, VLC authors and VideoLAN
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
#include <vlc_aout.h>

/* According to Apple's CoreAudio_Bitmap/CoreAudio_BitmapTypes.h */
enum
{
    CoreAudio_Bitmap_LEFT                 = (1<<0),
    CoreAudio_Bitmap_RIGHT                = (1<<1),
    CoreAudio_Bitmap_CENTER               = (1<<2),
    CoreAudio_Bitmap_LFESCREEN            = (1<<3),
    CoreAudio_Bitmap_BACKLEFT             = (1<<4),
    CoreAudio_Bitmap_BACKRIGHT            = (1<<5),
    CoreAudio_Bitmap_LEFTCENTER           = (1<<6),
    CoreAudio_Bitmap_RIGHTCENTER          = (1<<7),
    CoreAudio_Bitmap_BACKCENTER           = (1<<8),
    CoreAudio_Bitmap_SIDELEFT             = (1<<9),
    CoreAudio_Bitmap_SIDERIGHT            = (1<<10),
    CoreAudio_Bitmap_TOPCENTER            = (1<<11),
    CoreAudio_Bitmap_TOPFRONTLEFT         = (1<<12),
    CoreAudio_Bitmap_TOPFRONTENTER        = (1<<13),
    CoreAudio_Bitmap_TOPFRONTRIGHT        = (1<<14),
    CoreAudio_Bitmap_TOPBACKLEFT          = (1<<15),
    CoreAudio_Bitmap_TOPBACKCENTER        = (1<<16),
    CoreAudio_Bitmap_TOPBACKRIGHT         = (1<<17),
};

static const uint32_t pi_vlc_chan_order_CoreAudio[] =
{
    AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,
    AOUT_CHAN_LFE,
    AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_REARCENTER,
    AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
    0
};

static const struct
{
    uint32_t i_bitmap;
    uint32_t i_vlc_bitmap;
} CoreAudio_Bitmap_mapping[] = {
    { CoreAudio_Bitmap_LEFT,         AOUT_CHAN_LEFT },
    { CoreAudio_Bitmap_RIGHT,        AOUT_CHAN_RIGHT },
    { CoreAudio_Bitmap_CENTER,       AOUT_CHAN_CENTER },
    { CoreAudio_Bitmap_LFESCREEN,    AOUT_CHAN_LFE },
    { CoreAudio_Bitmap_BACKLEFT,     AOUT_CHAN_REARLEFT },
    { CoreAudio_Bitmap_BACKRIGHT,    AOUT_CHAN_REARRIGHT },
    { CoreAudio_Bitmap_LEFTCENTER,   AOUT_CHAN_MIDDLELEFT },
    { CoreAudio_Bitmap_RIGHTCENTER,  AOUT_CHAN_MIDDLERIGHT },
    { CoreAudio_Bitmap_BACKCENTER,   AOUT_CHAN_REARCENTER },
    { CoreAudio_Bitmap_SIDELEFT,     AOUT_CHAN_LEFT },
    { CoreAudio_Bitmap_SIDERIGHT,    AOUT_CHAN_RIGHT },
    { CoreAudio_Bitmap_TOPCENTER,    AOUT_CHAN_CENTER },
    { CoreAudio_Bitmap_TOPFRONTLEFT, AOUT_CHAN_LEFT },
    { CoreAudio_Bitmap_TOPFRONTENTER,AOUT_CHAN_CENTER },
    { CoreAudio_Bitmap_TOPFRONTRIGHT,AOUT_CHAN_RIGHT },
    { CoreAudio_Bitmap_TOPBACKLEFT,  AOUT_CHAN_REARLEFT },
    { CoreAudio_Bitmap_TOPBACKCENTER,AOUT_CHAN_REARCENTER },
    { CoreAudio_Bitmap_TOPBACKRIGHT, AOUT_CHAN_REARRIGHT },
};

enum CoreAudio_Layout
{
    CoreAudio_Layout_DESC                 = 0,
    CoreAudio_Layout_BITMAP               = (1<<16),
};

static inline int CoreAudio_Bitmap_to_vlc_bitmap( uint32_t i_corebitmap,
                                                  uint16_t *pi_mapping,
                                                  uint8_t *pi_channels,
                                                  const uint32_t **pp_chans_order )
{
    *pp_chans_order = pi_vlc_chan_order_CoreAudio;
    *pi_mapping = 0;
    *pi_channels = 0;
    for (uint8_t i=0;i<ARRAY_SIZE(CoreAudio_Bitmap_mapping);i++)
    {
        if ( CoreAudio_Bitmap_mapping[i].i_bitmap & i_corebitmap )
        {
            if ( (CoreAudio_Bitmap_mapping[i].i_vlc_bitmap & *pi_mapping) ||
                 *pi_channels >= AOUT_CHAN_MAX )
            {
                /* double mapping or unsupported number of channels */
                *pi_mapping = 0;
                *pi_channels = 0;
                return VLC_EGENERIC;
            }
            *pi_mapping |= CoreAudio_Bitmap_mapping[i].i_vlc_bitmap;
        }
    }
    return VLC_SUCCESS;
}
