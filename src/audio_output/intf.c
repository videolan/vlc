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

    float volume = var_GetFloat (aout, "volume");
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
    long volume = lroundf (vol * AOUT_VOLUME_DEFAULT);
    int ret = -1;

    audio_output_t *aout = findAout (obj);
    if (aout != NULL)
    {
        aout_lock (aout);
        if (aout->volume_set != NULL)
            ret = aout->volume_set (aout, vol);
        aout_unlock (aout);
        vlc_object_release (aout);
    }
    return ret;
}

#undef aout_VolumeUp
/**
 * Raises the volume.
 * \param value how much to increase (> 0) or decrease (< 0) the volume
 * \param volp if non-NULL, will contain contain the resulting volume
 */
int aout_VolumeUp (vlc_object_t *obj, int value, float *volp)
{
    value *= var_InheritInteger (obj, "volume-step");

    float vol = aout_VolumeGet (obj);
    if (vol < 0.)
        return -1;

    vol += value / (float)AOUT_VOLUME_DEFAULT;
    if (vol < 0.)
        vol = 0.;
    if (vol > 2.)
        vol = 2.;
    if (volp != NULL)
        *volp = vol;

    return aout_VolumeSet (obj, vol);
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

    bool mute = var_InheritBool (aout, "mute");
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
        aout_lock (aout);
        if (aout->mute_set != NULL)
            ret = aout->mute_set (aout, mute);
        aout_unlock (aout);
        vlc_object_release (aout);
    }

    if (ret == 0)
        var_SetBool (obj, "mute", mute);
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
        var_Destroy( p_aout, "audio-channels" );
    }
    aout_RequestRestart (p_aout);
    return 0;
}

#undef aout_EnableFilter
/** Enable or disable an audio filter
 * \param p_this a vlc object
 * \param psz_name name of the filter
 * \param b_add are we adding or removing the filter ?
 */
void aout_EnableFilter( vlc_object_t *p_this, const char *psz_name,
                        bool b_add )
{
    audio_output_t *p_aout = findAout( p_this );

    if( aout_ChangeFilterString( p_this, VLC_OBJECT(p_aout), "audio-filter", psz_name, b_add ) )
    {
        if( p_aout )
            aout_InputRequestRestart( p_aout );
    }

    if( p_aout )
        vlc_object_release( p_aout );
}
