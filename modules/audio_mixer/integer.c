/*****************************************************************************
 * integer.c: integer software volume
 *****************************************************************************
 * Copyright (C) 2011 Rémi Denis-Courmont
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

#include <math.h>
#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_aout_volume.h>

static int Activate (vlc_object_t *);

vlc_module_begin ()
    set_category (CAT_AUDIO)
    set_subcategory (SUBCAT_AUDIO_MISC)
    set_description (N_("Integer audio volume"))
    set_capability ("audio volume", 9)
    set_callbacks (Activate, NULL)
vlc_module_end ()

static void FilterS32N (audio_volume_t *vol, block_t *block, float volume)
{
    int32_t *p = (int32_t *)block->p_buffer;

    int32_t mult = lroundf (volume * 0x1.p24f);
    if (mult == (1 << 24))
        return;

    for (size_t n = block->i_buffer / sizeof (*p); n > 0; n--)
    {
        int64_t s = *p * (int64_t)mult;
        if (s >= (INT32_MAX << INT64_C(24)))
            *p = INT32_MAX;
        else
        if (s < (INT32_MIN << INT64_C(24)))
            *p = INT32_MIN;
        else
            *p = s >> INT64_C(24);
        p++;
    }
    (void) vol;
}

static void FilterS16N (audio_volume_t *vol, block_t *block, float volume)
{
    int16_t *p = (int16_t *)block->p_buffer;

    int16_t mult = lroundf (volume * 0x1.p8f);
    if (mult == (1 << 8))
        return;

    for (size_t n = block->i_buffer / sizeof (*p); n > 0; n--)
    {
        int32_t s = *p * (int32_t)mult;
        if (s >= (INT16_MAX << 8))
            *p = INT16_MAX;
        else
        if (s < (INT16_MIN << 8))
            *p = INT16_MIN;
        else
            *p = s >> 8;
        p++;
    }
    (void) vol;
}

static void FilterU8 (audio_volume_t *vol, block_t *block, float volume)
{
    uint8_t *p = (uint8_t *)block->p_buffer;

    int16_t mult = lroundf (volume * 0x1.p8f);
    if (mult == (1 << 8))
        return;

    for (size_t n = block->i_buffer / sizeof (*p); n > 0; n--)
    {
        int32_t s = (*p - 128) * mult;
        if (s >= (INT8_MAX << 8))
            *p = 255;
        else
        if (s < (INT8_MIN << 8))
            *p = 0;
        else
            *p = (s >> 8) + 128;
        p++;
    }
    (void) vol;
}

static int Activate (vlc_object_t *obj)
{
    audio_volume_t *vol = (audio_volume_t *)obj;

    switch (vol->format)
    {
        case VLC_CODEC_S32N:
            vol->amplify = FilterS32N;
            break;
        case VLC_CODEC_S16N:
            vol->amplify = FilterS16N;
            break;
        case VLC_CODEC_U8:
            vol->amplify = FilterU8;
            break;
        default:
            return -1;
    }
    return 0;
}
