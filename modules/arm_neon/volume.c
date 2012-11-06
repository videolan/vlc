/*****************************************************************************
 * volume.c : ARM NEON audio volume
 *****************************************************************************
 * Copyright (C) 2012 Rémi Denis-Courmont
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_cpu.h>
#include <vlc_aout.h>
#include <vlc_aout_volume.h>

static int Probe(vlc_object_t *);

vlc_module_begin()
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_MISC)
    set_description(N_("ARM NEON audio volume"))
    set_capability("audio volume", 10)
    set_callbacks(Probe, NULL)
vlc_module_end()

static void AmplifyFloat(audio_volume_t *, block_t *, float);

static int Probe(vlc_object_t *obj)
{
    audio_volume_t *volume = (audio_volume_t *)obj;

    if (!vlc_CPU_ARM_NEON())
        return VLC_EGENERIC;
    if (volume->format == VLC_CODEC_FL32)
        volume->amplify = AmplifyFloat;
    else
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

void amplify_float_arm_neon(float *, const float *, size_t, float);

static void AmplifyFloat(audio_volume_t *volume, block_t *block, float amp)
{
    float *buf = (float *)block->p_buffer;
    size_t length = block->i_buffer;

    if (amp == 1.0)
        return;

    /* Unaligned header */
    assert(((uintptr_t)buf & 3) == 0);
    while (unlikely((uintptr_t)buf & 12))
    {
        *(buf++) *= amp;
        length -= 4;
    }
    /* Unaligned footer */
    assert((length & 3) == 0);
    while (unlikely(length & 12))
    {
        length -= 4;
        buf[length / 4] *= amp;
    }

    amplify_float_arm_neon(buf, buf, length, amp);
    (void) volume;
}
