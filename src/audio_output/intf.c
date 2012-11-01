/*****************************************************************************
 * intf.c : audio output API towards the interface modules
 *****************************************************************************
 * Copyright (C) 2002-2007 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_aout_intf.h>

#include <stdio.h>
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>
#include <math.h>

#include <vlc_aout.h>
#include "aout_internal.h"

#include <vlc_playlist.h>

static audio_output_t *findAout (vlc_object_t *obj)
{
    input_thread_t *(*pf_find_input) (vlc_object_t *);

    pf_find_input = var_GetAddress (obj, "find-input-callback");
    if (unlikely(pf_find_input == NULL))
        return NULL;

    input_thread_t *p_input = pf_find_input (obj);
    if (p_input == NULL)
       return NULL;

    audio_output_t *p_aout = input_GetAout (p_input);
    vlc_object_release (p_input);
    return p_aout;
}
#define findAout(o) findAout(VLC_OBJECT(o))

#undef aout_VolumeGet
/**
 * Gets the volume of the output device (independent of mute).
 * \return Current audio volume (0 = silent, 1 = nominal),
 * or a strictly negative value if undefined.
 */
float aout_VolumeGet (vlc_object_t *obj)
{
    audio_output_t *aout = findAout (obj);
    if (aout == NULL)
        return -1.f;

    float volume = aout_OutputVolumeGet (aout);
    vlc_object_release (aout);
    return volume;
}

#undef aout_VolumeSet
/**
 * Sets the volume of the output device.
 * \note The mute status is not changed.
 */
int aout_VolumeSet (vlc_object_t *obj, float vol)
{
    int ret = -1;

    audio_output_t *aout = findAout (obj);
    if (aout != NULL)
    {
        ret = aout_OutputVolumeSet (aout, vol);
        vlc_object_release (aout);
    }
    return ret;
}

#undef aout_MuteGet
/**
 * Gets the output mute status.
 * \return 0 if not muted, 1 if muted, -1 if undefined.
 */
int aout_MuteGet (vlc_object_t *obj)
{
    audio_output_t *aout = findAout (obj);
    if (aout == NULL)
        return -1.f;

    bool mute = aout_OutputMuteGet (aout);
    vlc_object_release (aout);
    return mute;
}

#undef aout_MuteSet
/**
 * Sets mute status.
 */
int aout_MuteSet (vlc_object_t *obj, bool mute)
{
    int ret = -1;

    audio_output_t *aout = findAout (obj);
    if (aout != NULL)
    {
        ret = aout_OutputMuteSet (aout, mute);
        vlc_object_release (aout);
        if (ret == 0)
            var_SetBool (obj, "mute", mute);
    }
    return ret;
}


/*
 * Pipelines management
 */

/*****************************************************************************
 * aout_ChannelsRestart : change the audio device or channels and restart
 *****************************************************************************/
int aout_ChannelsRestart( vlc_object_t * p_this, const char * psz_variable,
                          vlc_value_t oldval, vlc_value_t newval,
                          void *p_data )
{
    audio_output_t * p_aout = (audio_output_t *)p_this;
    (void)oldval; (void)newval; (void)p_data;

    if ( !strcmp( psz_variable, "audio-device" ) )
    {
        /* This is supposed to be a significant change and supposes
         * rebuilding the channel choices. */
        var_Destroy( p_aout, "stereo-mode" );
    }
    aout_RequestRestart (p_aout);
    return 0;
}
