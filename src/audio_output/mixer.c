/*****************************************************************************
 * mixer.c : audio output mixing operations
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stddef.h>
#include <vlc_common.h>
#include <libvlc.h>
#include <vlc_modules.h>
#include <vlc_aout.h>
#include <vlc_aout_mixer.h>
#include "aout_internal.h"

#undef aout_MixerNew
/**
 * Creates a software amplifier.
 */
audio_mixer_t *aout_MixerNew(vlc_object_t *obj, vlc_fourcc_t format)
{
    audio_mixer_t *mixer = vlc_custom_create(obj, sizeof (*mixer), "mixer");
    if (unlikely(mixer == NULL))
        return NULL;

    mixer->format = format;
    mixer->mix = NULL;
    mixer->module = module_need(mixer, "audio mixer", NULL, false);
    if (mixer->module == NULL)
    {
        msg_Err(mixer, "no suitable audio mixer");
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
