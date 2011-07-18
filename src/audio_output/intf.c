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

static aout_instance_t *findAout (vlc_object_t *obj)
{
    input_thread_t *(*pf_find_input) (vlc_object_t *);

    pf_find_input = var_GetAddress (obj, "find-input-callback");
    if (unlikely(pf_find_input == NULL))
        return NULL;

    input_thread_t *p_input = pf_find_input (obj);
    if (p_input == NULL)
       return NULL;

    aout_instance_t *p_aout = input_GetAout (p_input);
    vlc_object_release (p_input);
    return p_aout;
}
#define findAout(o) findAout(VLC_OBJECT(o))

/** Start a volume change transaction. */
static void prepareVolume (vlc_object_t *obj, aout_instance_t **aoutp,
                           audio_volume_t *volp, bool *mutep)
{
    aout_instance_t *aout = findAout (obj);

    /* FIXME: we need interlocking even if aout does not exist! */
    *aoutp = aout;
    if (aout != NULL)
        aout_lock_volume (aout);
    if (volp != NULL)
        *volp = var_GetInteger (obj, "volume");
    if (mutep != NULL)
        *mutep = var_GetBool (obj, "mute");
}

/** Commit a volume change transaction. */
static int commitVolume (vlc_object_t *obj, aout_instance_t *aout,
                         audio_volume_t volume, bool mute)
{
    int ret = 0;

    var_SetInteger (obj, "volume", volume);
    var_SetBool (obj, "mute", mute);

    if (aout != NULL)
    {
        float vol = volume / (float)AOUT_VOLUME_DEFAULT;

        aout_lock (aout);
#warning FIXME: wrong test. Need to check that aout_output is ready.
        if (aout->p_mixer != NULL)
            ret = aout->output.pf_volume_set (aout, vol, mute);
        aout_unlock (aout);

        if (ret == 0)
            var_TriggerCallback (aout, "intf-change");
        aout_unlock_volume (aout);
        vlc_object_release (aout);
    }
    return ret;
}

#if 0
/** Cancel a volume change transaction. */
static void cancelVolume (vlc_object_t *obj, aout_instance_t *aout)
{
    (void) obj;
    if (aout != NULL)
    {
        aout_unlock_volume (aout);
        vlc_object_release (aout);
    }
}
#endif

#undef aout_VolumeGet
/**
 * Gets the volume of the output device (independent of mute).
 */
audio_volume_t aout_VolumeGet (vlc_object_t *obj)
{
#if 0
    aout_instance_t *aout;
    audio_volume_t volume;

    prepareVolume (obj, &aout, &volume, NULL);
    cancelVolume (obj, aout);
    return 0;
#else
    return var_GetInteger (obj, "volume");
#endif
}

#undef aout_VolumeSet
/**
 * Sets the volume of the output device.
 * The mute status is not changed.
 */
int aout_VolumeSet (vlc_object_t *obj, audio_volume_t volume)
{
    aout_instance_t *aout;
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
    aout_instance_t *aout;
    int ret;
    audio_volume_t volume;
    bool mute;

    value *= var_InheritInteger (obj, "volume-step");

    prepareVolume (obj, &aout, &volume, &mute);
    value += volume;
    if (value < AOUT_VOLUME_MIN)
        volume = AOUT_VOLUME_MIN;
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

#undef aout_VolumeDown
/**
 * Lowers the volume. See aout_VolumeUp().
 */
int aout_VolumeDown (vlc_object_t *obj, int steps, audio_volume_t *volp)
{
    return aout_VolumeUp (obj, -steps, volp);
}

#undef aout_ToggleMute
/**
 * Toggles the mute state.
 */
int aout_ToggleMute (vlc_object_t *obj, audio_volume_t *volp)
{
    aout_instance_t *aout;
    int ret;
    audio_volume_t volume;
    bool mute;

    prepareVolume (obj, &aout, &volume, &mute);
    mute = !mute;
    ret = commitVolume (obj, aout, volume, mute);
    if (volp != NULL)
        *volp = mute ? AOUT_VOLUME_MIN : volume;
    return ret;
}

/**
 * Gets the output mute status.
 */
bool aout_IsMuted (vlc_object_t *obj)
{
#if 0
    aout_instance_t *aout;
    bool mute;

    prepareVolume (obj, &aout, NULL, &mute);
    cancelVolume (obj, aout);
    return mute;
#else
    return var_GetBool (obj, "mute");
#endif
}

/**
 * Sets mute status.
 */
int aout_SetMute (vlc_object_t *obj, audio_volume_t *volp, bool mute)
{
    aout_instance_t *aout;
    int ret;
    audio_volume_t volume;

    prepareVolume (obj, &aout, &volume, NULL);
    ret = commitVolume (obj, aout, volume, mute);
    if (volp != NULL)
        *volp = mute ? AOUT_VOLUME_MIN : volume;
    return ret;
}


/*
 * The next functions are not supposed to be called by the interface, but
 * are placeholders for software-only scaling.
 */
static int aout_VolumeSoftSet (aout_instance_t *aout, float volume, bool mute)
{
    aout->mixer_multiplier = mute ? 0. : volume;
    return 0;
}

/* Meant to be called by the output plug-in's Open(). */
void aout_VolumeSoftInit (aout_instance_t *aout)
{
    audio_volume_t volume = var_InheritInteger (aout, "volume");
    bool mute = var_InheritBool (aout, "mute");

    aout->output.pf_volume_set = aout_VolumeSoftSet;
    aout_VolumeSoftSet (aout, volume, mute);
}


/*
 * The next functions are not supposed to be called by the interface, but
 * are placeholders for unsupported scaling.
 */
static int aout_VolumeNoneSet (aout_instance_t *aout, float volume, bool mute)
{
    (void)aout; (void)volume; (void)mute;
    return -1;
}

/* Meant to be called by the output plug-in's Open(). */
void aout_VolumeNoneInit( aout_instance_t * p_aout )
{
    p_aout->output.pf_volume_set = aout_VolumeNoneSet;
}


/*
 * Pipelines management
 */

/*****************************************************************************
 * aout_Restart : re-open the output device and rebuild the input and output
 *                pipelines
 *****************************************************************************
 * This function is used whenever the parameters of the output plug-in are
 * changed (eg. selecting S/PDIF or PCM).
 *****************************************************************************/
static int aout_Restart( aout_instance_t * p_aout )
{
    aout_input_t *p_input;

    aout_lock( p_aout );
    p_input = p_aout->p_input;
    if( p_input == NULL )
    {
        aout_unlock( p_aout );
        msg_Err( p_aout, "no decoder thread" );
        return -1;
    }

    /* Reinitializes the output */
    aout_InputDelete( p_aout, p_input );
    aout_MixerDelete( p_aout );
    aout_OutputDelete( p_aout );

    /* FIXME: This function is notoriously dangerous/unsafe.
     * By the way, if OutputNew or MixerNew fails, we are totally screwed. */
    if ( aout_OutputNew( p_aout, &p_input->input ) == -1 )
    {
        /* Release all locks and report the error. */
        aout_unlock( p_aout );
        return -1;
    }

    if ( aout_MixerNew( p_aout ) == -1 )
    {
        aout_OutputDelete( p_aout );
        aout_unlock( p_aout );
        return -1;
    }

    if( aout_InputNew( p_aout, p_input, &p_input->request_vout ) )
    {
#warning FIXME: deal with errors
        aout_unlock( p_aout );
        return -1;
    }
    aout_unlock( p_aout );
    return 0;
}

/*****************************************************************************
 * aout_ChannelsRestart : change the audio device or channels and restart
 *****************************************************************************/
int aout_ChannelsRestart( vlc_object_t * p_this, const char * psz_variable,
                          vlc_value_t oldval, vlc_value_t newval,
                          void *p_data )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
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
    aout_instance_t *p_aout = findAout( p_this );

    if( aout_ChangeFilterString( p_this, p_aout, "audio-filter", psz_name, b_add ) )
    {
        if( p_aout )
            AoutInputsMarkToRestart( p_aout );
    }

    if( p_aout )
        vlc_object_release( p_aout );
}
