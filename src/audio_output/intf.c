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

/** Start a volume change transaction. */
static void prepareVolume (vlc_object_t *obj, audio_output_t **aoutp,
                           float *vol, bool *mute)
{
    audio_output_t *aout = findAout (obj);

    /* FIXME: we need interlocking even if aout does not exist! */
    *aoutp = aout;
    if (aout != NULL)
    {
        obj = VLC_OBJECT(aout); /* use aout volume if aout exists */
        aout_lock_volume (aout);
    }
    if (vol != NULL)
        *vol = var_InheritInteger (obj, "volume") / (float)AOUT_VOLUME_DEFAULT;
    if (mute != NULL)
        *mute = var_InheritBool (obj, "mute");
}

/** Commit a volume change transaction. */
static int commitVolume (vlc_object_t *obj, audio_output_t *aout,
                         float vol, bool mute)
{
    long volume = lroundf (vol * AOUT_VOLUME_DEFAULT);
    int ret = 0;

    if (aout != NULL)
    {
        /* apply volume to the pipeline */
        aout_lock (aout);
        if (aout->pf_volume_set != NULL)
            ret = aout->pf_volume_set (aout, vol, mute);
        aout_unlock (aout);

        if (ret == 0)
        {   /* update aout volume if it maintains its own */
            var_SetInteger (aout, "volume", volume);
            var_SetBool (aout, "mute", mute);
        }
        aout_unlock_volume (aout);

        vlc_object_release (aout);
    }
    if (ret == 0)
    {   /* update caller (input manager) volume */
        var_SetInteger (obj, "volume", volume);
        var_SetBool (obj, "mute", mute);
        if (var_InheritBool (obj, "volume-save"))
            config_PutInt (obj, "volume", volume);
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
 * \return Current audio volume (0 = silent, 1 = nominal),
 * or a strictly negative value if undefined.
 */
float aout_VolumeGet (vlc_object_t *obj)
{
    audio_output_t *aout;
    float vol;

    prepareVolume (obj, &aout, &vol, NULL);
    cancelVolume (obj, aout);
    return vol;
}

#undef aout_VolumeSet
/**
 * Sets the volume of the output device.
 * The mute status is not changed.
 */
int aout_VolumeSet (vlc_object_t *obj, float vol)
{
    audio_output_t *aout;
    bool mute;

    prepareVolume (obj, &aout, NULL, &mute);
    return commitVolume (obj, aout, vol, mute);
}

#undef aout_VolumeUp
/**
 * Raises the volume.
 * \param value how much to increase (> 0) or decrease (< 0) the volume
 * \param volp if non-NULL, will contain contain the resulting volume
 */
int aout_VolumeUp (vlc_object_t *obj, int value, float *volp)
{
    audio_output_t *aout;
    int ret;
    float vol;
    bool mute;

    value *= var_InheritInteger (obj, "volume-step");

    prepareVolume (obj, &aout, &vol, &mute);
    vol += value / (float)AOUT_VOLUME_DEFAULT;
    if (vol < 0.)
        vol = 0.;
    if (vol > (AOUT_VOLUME_MAX / AOUT_VOLUME_DEFAULT))
        vol = AOUT_VOLUME_MAX / AOUT_VOLUME_DEFAULT;
    ret = commitVolume (obj, aout, vol, mute);
    if (volp != NULL)
        *volp = vol;
    return ret;
}

#undef aout_MuteToggle
/**
 * Toggles the mute state.
 */
int aout_MuteToggle (vlc_object_t *obj)
{
    audio_output_t *aout;
    float vol;
    bool mute;

    prepareVolume (obj, &aout, &vol, &mute);
    mute = !mute;
    return commitVolume (obj, aout, vol, mute);
}

#undef aout_MuteGet
/**
 * Gets the output mute status.
 * \return 0 if not muted, 1 if muted, -1 if undefined.
 */
int aout_MuteGet (vlc_object_t *obj)
{
    audio_output_t *aout;
    bool mute;

    prepareVolume (obj, &aout, NULL, &mute);
    cancelVolume (obj, aout);
    return mute;
}

#undef aout_MuteSet
/**
 * Sets mute status.
 */
int aout_MuteSet (vlc_object_t *obj, bool mute)
{
    audio_output_t *aout;
    float vol;

    prepareVolume (obj, &aout, &vol, NULL);
    return commitVolume (obj, aout, vol, mute);
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
