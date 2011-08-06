/*****************************************************************************
 * intf.c : audio output API towards the interface modules
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
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

#include <vlc_common.h>
#include <vlc_aout_intf.h>

#include <stdio.h>
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

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

/** Start a volume change transaction. */
static void prepareVolume (vlc_object_t *obj, audio_output_t **aoutp,
                           audio_volume_t *volp, bool *mutep)
{
    audio_output_t *aout = findAout (obj);

    /* FIXME: we need interlocking even if aout does not exist! */
    *aoutp = aout;
    if (aout != NULL)
    {
        obj = VLC_OBJECT(aout); /* use aout volume if aout exists */
        aout_lock_volume (aout);
    }
    if (volp != NULL)
        *volp = var_InheritInteger (obj, "volume");
    if (mutep != NULL)
        *mutep = var_InheritBool (obj, "mute");
}

/** Commit a volume change transaction. */
static int commitVolume (vlc_object_t *obj, audio_output_t *aout,
                         audio_volume_t volume, bool mute)
{
    int ret = 0;

    /* update caller (input manager) volume */
    var_SetInteger (obj, "volume", volume);
    var_SetBool (obj, "mute", mute);

    if (aout != NULL)
    {
        aout_owner_t *owner = aout_owner (aout);
        float vol = volume / (float)AOUT_VOLUME_DEFAULT;

        /* apply volume to the pipeline */
        aout_lock (aout);
        if (owner->module != NULL)
            ret = aout->pf_volume_set (aout, vol, mute);
        aout_unlock (aout);

        /* update aout volume if it maintains its own */
        var_SetInteger (aout, "volume", volume);
        var_SetBool (aout, "mute", mute);
        aout_unlock_volume (aout);

        if (ret == 0)
            var_TriggerCallback (aout, "intf-change");
        vlc_object_release (aout);
    }
    return ret;
}

/** Cancel a volume change transaction. */
static void cancelVolume (vlc_object_t *obj, audio_output_t *aout)
{
    (void) obj;
    if (aout != NULL)
    {
        aout_unlock_volume (aout);
        vlc_object_release (aout);
    }
}

#undef aout_VolumeGet
/**
 * Gets the volume of the output device (independent of mute).
 */
audio_volume_t aout_VolumeGet (vlc_object_t *obj)
{
    audio_output_t *aout;
    audio_volume_t volume;

    prepareVolume (obj, &aout, &volume, NULL);
    cancelVolume (obj, aout);
    return volume;
}

#undef aout_VolumeSet
/**
 * Sets the volume of the output device.
 * The mute status is not changed.
 */
int aout_VolumeSet (vlc_object_t *obj, audio_volume_t volume)
{
    audio_output_t *aout;
    bool mute;

    prepareVolume (obj, &aout, NULL, &mute);
    return commitVolume (obj, aout, volume, mute);
}

#undef aout_VolumeUp
/**
 * Raises the volume.
 * \param value how much to increase (> 0) or decrease (< 0) the volume
 * \param volp if non-NULL, will contain contain the resulting volume
 */
int aout_VolumeUp (vlc_object_t *obj, int value, audio_volume_t *volp)
{
    audio_output_t *aout;
    int ret;
    audio_volume_t volume;
    bool mute;

    value *= var_InheritInteger (obj, "volume-step");

    prepareVolume (obj, &aout, &volume, &mute);
    value += volume;
    if (value < 0)
        volume = 0;
    else
    if (value > AOUT_VOLUME_MAX)
        volume = AOUT_VOLUME_MAX;
    else
        volume = value;
    ret = commitVolume (obj, aout, volume, mute);
    if (volp != NULL)
        *volp = volume;
    return ret;
}

#undef aout_ToggleMute
/**
 * Toggles the mute state.
 */
int aout_ToggleMute (vlc_object_t *obj, audio_volume_t *volp)
{
    audio_output_t *aout;
    int ret;
    audio_volume_t volume;
    bool mute;

    prepareVolume (obj, &aout, &volume, &mute);
    mute = !mute;
    ret = commitVolume (obj, aout, volume, mute);
    if (volp != NULL)
        *volp = mute ? 0 : volume;
    return ret;
}

/**
 * Gets the output mute status.
 */
bool aout_IsMuted (vlc_object_t *obj)
{
    audio_output_t *aout;
    bool mute;

    prepareVolume (obj, &aout, NULL, &mute);
    cancelVolume (obj, aout);
    return mute;
}

/**
 * Sets mute status.
 */
int aout_SetMute (vlc_object_t *obj, audio_volume_t *volp, bool mute)
{
    audio_output_t *aout;
    int ret;
    audio_volume_t volume;

    prepareVolume (obj, &aout, &volume, NULL);
    ret = commitVolume (obj, aout, volume, mute);
    if (volp != NULL)
        *volp = mute ? 0 : volume;
    return ret;
}


/*
 * Pipelines management
 */

/**
 * Marks the audio output for restart, to update any parameter of the output
 * plug-in (e.g. output device or channel mapping).
 */
static void aout_Restart (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_lock (aout);
    if (owner->input != NULL)
        owner->need_restart = true;
    aout_unlock (aout);
}

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
    aout_Restart( p_aout );
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
            AoutInputsMarkToRestart( p_aout );
    }

    if( p_aout )
        vlc_object_release( p_aout );
}
