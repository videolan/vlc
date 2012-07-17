/*****************************************************************************
 * mixer.c : audio output volume operations
 *****************************************************************************
 * Copyright (C) 2002-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stddef.h>
#include <math.h>

#include <vlc_common.h>
#include <libvlc.h>
#include <vlc_modules.h>
#include <vlc_aout.h>
#include <vlc_aout_mixer.h>
#include "aout_internal.h"

/* Note: Once upon a time, the audio output volume module was also responsible
 * for mixing multiple audio inputs together. Hence it was called mixer.
 * Nowadays, there is only ever a single input per module instance, so this has
 * become a misnomer. */

typedef struct aout_volume
{
    audio_mixer_t volume;
} aout_volume_t;

static inline aout_volume_t *vol_priv(audio_mixer_t *volume)
{
    return (aout_volume_t *)volume;
}

#undef aout_MixerNew
/**
 * Creates a software amplifier.
 */
audio_mixer_t *aout_MixerNew(vlc_object_t *obj, vlc_fourcc_t format)
{
    audio_mixer_t *mixer = vlc_custom_create(obj, sizeof (aout_volume_t),
                                             "volume");
    if (unlikely(mixer == NULL))
        return NULL;

    mixer->format = format;
    mixer->mix = NULL;
    mixer->module = module_need(mixer, "audio mixer", NULL, false);
    if (mixer->module == NULL)
    {
        vlc_object_release(mixer);
        mixer = NULL;
    }
    return mixer;
}

/**
 * Destroys a software amplifier.
 */
void aout_MixerDelete(audio_mixer_t *mixer)
{
    if (mixer == NULL)
        return;

    module_unneed(mixer, mixer->module);
    vlc_object_release(mixer);
}

/**
 * Applies replay gain and software volume to an audio buffer.
 */
void aout_MixerRun(audio_mixer_t *mixer, block_t *block, float amp)
{
    mixer->mix(mixer, block, amp);
}

/*** Replay gain ***/
float (aout_ReplayGainSelect)(vlc_object_t *obj, const char *str,
                              const audio_replay_gain_t *replay_gain)
{
    float gain = 0.;
    unsigned mode = AUDIO_REPLAY_GAIN_MAX;

    if (likely(str != NULL))
    {   /* Find selectrf mode */
        if (!strcmp (str, "track"))
            mode = AUDIO_REPLAY_GAIN_TRACK;
        else
        if (!strcmp (str, "album"))
            mode = AUDIO_REPLAY_GAIN_ALBUM;

        /* If the selectrf mode is not available, prefer the other one */
        if (mode != AUDIO_REPLAY_GAIN_MAX && !replay_gain->pb_gain[mode])
        {
            if (replay_gain->pb_gain[!mode])
                mode = !mode;
        }
    }

    /* */
    if (mode == AUDIO_REPLAY_GAIN_MAX)
        return 1.;

    if (replay_gain->pb_gain[mode])
        gain = replay_gain->pf_gain[mode]
             + var_InheritFloat (obj, "audio-replay-gain-preamp");
    else
        gain = var_InheritFloat (obj, "audio-replay-gain-default");

    float multiplier = pow (10., gain / 20.);

    if (replay_gain->pb_peak[mode]
     && var_InheritBool (obj, "audio-replay-gain-peak-protection")
     && replay_gain->pf_peak[mode] * multiplier > 1.0)
        multiplier = 1.0f / replay_gain->pf_peak[mode];

    return multiplier;
}
