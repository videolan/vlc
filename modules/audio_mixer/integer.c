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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_aout_mixer.h>

static int Activate (vlc_object_t *);

vlc_module_begin ()
    set_category (CAT_AUDIO)
    set_subcategory (SUBCAT_AUDIO_MISC)
    set_description (N_("Integer audio mixer"))
    set_capability ("audio mixer", 9)
    set_callbacks (Activate, NULL)
vlc_module_end ()

static void FilterS16N (audio_mixer_t *, block_t *, float);

static int Activate (vlc_object_t *obj)
{
    audio_mixer_t *mixer = (audio_mixer_t *)obj;

    switch (mixer->format)
    {
        case VLC_CODEC_S16N:
            mixer->mix = FilterS16N;
            break;
        default:
            return -1;
    }
    return 0;
}

static void FilterS16N (audio_mixer_t *mixer, block_t *block, float volume)
{
    int32_t mult = volume * 0x1.p16;

    if (mult == 0x10000)
        return;

    int16_t *p = (int16_t *)block->p_buffer;

    if (mult < 0x10000)
    {
        for (size_t n = block->i_buffer / sizeof (*p); n > 0; n--)
        {
            *p = (*p * mult) >> 16;
            p++;
        }
    }
    else
    {
        mult >>= 4;
        for (size_t n = block->i_buffer / sizeof (*p); n > 0; n--)
        {
            int32_t v = (*p * mult) >> 12;
            if (abs (v) > 0x7fff)
                v = 0x8000;
            *(p++) = v;
        }
    }

    (void) mixer;
}
