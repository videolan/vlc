/*****************************************************************************
 * volume.c : audio output volume operations
 *****************************************************************************
 * Copyright (C) 2002-2004 VLC authors and VideoLAN
 * Copyright (C) 2011-2012 Rémi Denis-Courmont
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

#include <stdatomic.h>
#include <stddef.h>
#include <math.h>

#include <vlc_common.h>
#include "../libvlc.h"
#include <vlc_modules.h>
#include <vlc_aout.h>
#include <vlc_aout_volume.h>
#include <vlc_replay_gain.h>
#include "aout_internal.h"

struct aout_volume
{
    audio_volume_t object;
    audio_replay_gain_t replay_gain;
    _Atomic float gain_factor;
    _Atomic float output_factor;
    module_t *module;
};

static int ReplayGainCallback (vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void *);

#undef aout_volume_New
/**
 * Creates a software amplifier.
 */
aout_volume_t *aout_volume_New(vlc_object_t *parent,
                               const audio_replay_gain_t *gain)
{
    aout_volume_t *vol = vlc_custom_create(parent, sizeof (aout_volume_t),
                                           "volume");
    if (unlikely(vol == NULL))
        return NULL;
    vol->module = NULL;
    atomic_init(&vol->gain_factor, 1.f);
    atomic_init(&vol->output_factor, 1.f);

    //audio_volume_t *obj = &vol->object;

    /* Gain */
    if (gain != NULL)
        memcpy(&vol->replay_gain, gain, sizeof (vol->replay_gain));
    else
        memset(&vol->replay_gain, 0, sizeof (vol->replay_gain));

    var_AddCallback(parent, "audio-replay-gain-mode",
                    ReplayGainCallback, vol);
    var_TriggerCallback(parent, "audio-replay-gain-mode");

    return vol;
}

/**
 * Selects the current sample format for software amplification.
 */
int aout_volume_SetFormat(aout_volume_t *vol, vlc_fourcc_t format)
{
    audio_volume_t *obj = &vol->object;
    if (vol->module != NULL)
    {
        if (obj->format == format)
        {
            msg_Dbg (obj, "retaining sample format");
            return 0;
        }
        msg_Dbg (obj, "changing sample format");
        module_unneed(obj, vol->module);
    }

    obj->format = format;
    vol->module = module_need(obj, "audio volume", NULL, false);
    if (vol->module == NULL)
        return -1;
    return 0;
}

/**
 * Destroys a software amplifier.
 */
void aout_volume_Delete(aout_volume_t *vol)
{
    audio_volume_t *obj = &vol->object;

    if (vol->module != NULL)
        module_unneed(obj, vol->module);
    var_DelCallback(vlc_object_parent(obj), "audio-replay-gain-mode",
                    ReplayGainCallback, vol);
    vlc_object_delete(obj);
}

void aout_volume_SetVolume(aout_volume_t *vol, float factor)
{
    atomic_store_explicit(&vol->output_factor, factor, memory_order_relaxed);
}

/**
 * Applies replay gain and software volume to an audio buffer.
 */
int aout_volume_Amplify(aout_volume_t *vol, block_t *block)
{
    if (vol->module == NULL)
        return -1;

    float amp = atomic_load_explicit(&vol->output_factor, memory_order_relaxed)
              * atomic_load_explicit(&vol->gain_factor, memory_order_relaxed);

    vol->object.amplify(&vol->object, block, amp);
    return 0;
}

static int ReplayGainCallback (vlc_object_t *obj, char const *var,
                               vlc_value_t oldval, vlc_value_t val, void *data)
{
    aout_volume_t *vol = data;
    float multiplier = replay_gain_CalcMultiplier(obj, &vol->replay_gain);
    atomic_store_explicit(&vol->gain_factor, multiplier, memory_order_relaxed);
    VLC_UNUSED(var); VLC_UNUSED(oldval); VLC_UNUSED(val);
    return VLC_SUCCESS;
}
