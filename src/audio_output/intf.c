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

#include <stdio.h>
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <vlc_aout.h>
#include "aout_internal.h"

/*
 * Volume management
 *
 * The hardware volume cannot be set if the output module gets deleted, so
 * we must take the mixer lock. The software volume cannot be set while the
 * mixer is running, so we need the mixer lock (too).
 *
 * Here is a schematic of the i_volume range :
 *
 * |------------------------------+---------------------------------------|
 * 0                           pi_soft                                   1024
 *
 * Between 0 and pi_soft, the volume is done in hardware by the output
 * module. Above, the output module will change p_aout->mixer.i_multiplier
 * (done in software). This scaling may result * in cropping errors and
 * should be avoided as much as possible.
 *
 * It is legal to have *pi_soft == 0, and do everything in software.
 * It is also legal to have *pi_soft == 1024, and completely avoid
 * software scaling. However, some streams (esp. A/52) are encoded with
 * a very low volume and users may complain.
 */

/*****************************************************************************
 * aout_VolumeGet : get the volume of the output device
 *****************************************************************************/
int __aout_VolumeGet( vlc_object_t * p_object, audio_volume_t * pi_volume )
{
    int i_result = 0;
    aout_instance_t * p_aout = vlc_object_find( p_object, VLC_OBJECT_AOUT,
                                                FIND_ANYWHERE );

    if ( pi_volume == NULL ) return -1;

    if ( p_aout == NULL )
    {
        *pi_volume = (audio_volume_t)config_GetInt( p_object, "volume" );
        return 0;
    }

    aout_lock_mixer( p_aout );
    if ( !p_aout->mixer.b_error )
    {
        i_result = p_aout->output.pf_volume_get( p_aout, pi_volume );
    }
    else
    {
        *pi_volume = (audio_volume_t)config_GetInt( p_object, "volume" );
    }
    aout_unlock_mixer( p_aout );

    vlc_object_release( p_aout );
    return i_result;
}

/*****************************************************************************
 * aout_VolumeSet : set the volume of the output device
 *****************************************************************************/
int __aout_VolumeSet( vlc_object_t * p_object, audio_volume_t i_volume )
{
    aout_instance_t *p_aout = vlc_object_find( p_object, VLC_OBJECT_AOUT, FIND_ANYWHERE );
    int i_result = 0;

    config_PutInt( p_object, "volume", i_volume );
    var_SetBool( p_object->p_libvlc, "volume-change", true );

    if ( p_aout == NULL ) return 0;

    aout_lock_mixer( p_aout );
    if ( !p_aout->mixer.b_error )
    {
        i_result = p_aout->output.pf_volume_set( p_aout, i_volume );
    }
    aout_unlock_mixer( p_aout );

    var_SetBool( p_aout, "intf-change", true );
    vlc_object_release( p_aout );
    return i_result;
}

/*****************************************************************************
 * aout_VolumeInfos : get the boundary pi_soft
 *****************************************************************************/
int __aout_VolumeInfos( vlc_object_t * p_object, audio_volume_t * pi_soft )
{
    aout_instance_t * p_aout = vlc_object_find( p_object, VLC_OBJECT_AOUT,
                                                FIND_ANYWHERE );
    int i_result;

    if ( p_aout == NULL ) return 0;

    aout_lock_mixer( p_aout );
    if ( p_aout->mixer.b_error )
    {
        /* The output module is destroyed. */
        i_result = -1;
    }
    else
    {
        i_result = p_aout->output.pf_volume_infos( p_aout, pi_soft );
    }
    aout_unlock_mixer( p_aout );

    vlc_object_release( p_aout );
    return i_result;
}

/*****************************************************************************
 * aout_VolumeUp : raise the output volume
 *****************************************************************************
 * If pi_volume != NULL, *pi_volume will contain the volume at the end of the
 * function.
 *****************************************************************************/
int __aout_VolumeUp( vlc_object_t * p_object, int i_nb_steps,
                   audio_volume_t * pi_volume )
{
    aout_instance_t * p_aout = vlc_object_find( p_object, VLC_OBJECT_AOUT,
                                                FIND_ANYWHERE );
    int i_result = 0, i_volume = 0, i_volume_step = 0;

    i_volume_step = config_GetInt( p_object->p_libvlc, "volume-step" );
    i_volume = config_GetInt( p_object, "volume" );
    i_volume += i_volume_step * i_nb_steps;
    if ( i_volume > AOUT_VOLUME_MAX )
    {
        i_volume = AOUT_VOLUME_MAX;
    }
    config_PutInt( p_object, "volume", i_volume );
    var_Create( p_object->p_libvlc, "saved-volume", VLC_VAR_INTEGER );
    var_SetInteger( p_object->p_libvlc, "saved-volume" ,
                    (audio_volume_t) i_volume );
    if ( pi_volume != NULL ) *pi_volume = (audio_volume_t) i_volume;

    var_SetBool( p_object->p_libvlc, "volume-change", true );

    if ( p_aout == NULL ) return 0;

    aout_lock_mixer( p_aout );
    if ( !p_aout->mixer.b_error )
    {
        i_result = p_aout->output.pf_volume_set( p_aout,
                                                (audio_volume_t) i_volume );
    }
    aout_unlock_mixer( p_aout );

    vlc_object_release( p_aout );
    return i_result;
}

/*****************************************************************************
 * aout_VolumeDown : lower the output volume
 *****************************************************************************
 * If pi_volume != NULL, *pi_volume will contain the volume at the end of the
 * function.
 *****************************************************************************/
int __aout_VolumeDown( vlc_object_t * p_object, int i_nb_steps,
                     audio_volume_t * pi_volume )
{
    aout_instance_t * p_aout = vlc_object_find( p_object, VLC_OBJECT_AOUT,
                                                FIND_ANYWHERE );
    int i_result = 0, i_volume = 0, i_volume_step = 0;

    i_volume_step = config_GetInt( p_object->p_libvlc, "volume-step" );
    i_volume = config_GetInt( p_object, "volume" );
    i_volume -= i_volume_step * i_nb_steps;
    if ( i_volume < AOUT_VOLUME_MIN )
    {
        i_volume = AOUT_VOLUME_MIN;
    }
    config_PutInt( p_object, "volume", i_volume );
    var_Create( p_object->p_libvlc, "saved-volume", VLC_VAR_INTEGER );
    var_SetInteger( p_object->p_libvlc, "saved-volume", (audio_volume_t) i_volume );
    if ( pi_volume != NULL ) *pi_volume = (audio_volume_t) i_volume;

    var_SetBool( p_object->p_libvlc, "volume-change", true );

    if ( p_aout == NULL ) return 0;

    aout_lock_mixer( p_aout );
    if ( !p_aout->mixer.b_error )
    {
        i_result = p_aout->output.pf_volume_set( p_aout, (audio_volume_t) i_volume );
    }
    aout_unlock_mixer( p_aout );

    vlc_object_release( p_aout );
    return i_result;
}

/*****************************************************************************
 * aout_VolumeMute : Mute/un-mute the output volume
 *****************************************************************************
 * If pi_volume != NULL, *pi_volume will contain the volume at the end of the
 * function (muted => 0).
 *****************************************************************************/
int __aout_VolumeMute( vlc_object_t * p_object, audio_volume_t * pi_volume )
{
    int i_result;
    audio_volume_t i_volume;

    i_volume = (audio_volume_t)config_GetInt( p_object, "volume" );
    if ( i_volume != 0 )
    {
        /* Mute */
        i_result = aout_VolumeSet( p_object, AOUT_VOLUME_MIN );
        var_Create( p_object->p_libvlc, "saved-volume", VLC_VAR_INTEGER );
        var_SetInteger( p_object->p_libvlc, "saved-volume", (int)i_volume );
        if ( pi_volume != NULL ) *pi_volume = AOUT_VOLUME_MIN;
    }
    else
    {
        /* Un-mute */
        var_Create( p_object->p_libvlc, "saved-volume", VLC_VAR_INTEGER );
        i_volume = (audio_volume_t)var_GetInteger( p_object->p_libvlc,
                                                   "saved-volume" );
        i_result = aout_VolumeSet( p_object, i_volume );
        if ( pi_volume != NULL ) *pi_volume = i_volume;
    }

    return i_result;
}

/*
 * The next functions are not supposed to be called by the interface, but
 * are placeholders for software-only scaling.
 */

/* Meant to be called by the output plug-in's Open(). */
void aout_VolumeSoftInit( aout_instance_t * p_aout )
{
    int i_volume;

    p_aout->output.pf_volume_infos = aout_VolumeSoftInfos;
    p_aout->output.pf_volume_get = aout_VolumeSoftGet;
    p_aout->output.pf_volume_set = aout_VolumeSoftSet;

    i_volume = config_GetInt( p_aout, "volume" );
    if ( i_volume < AOUT_VOLUME_MIN )
    {
        i_volume = AOUT_VOLUME_DEFAULT;
    }
    else if ( i_volume > AOUT_VOLUME_MAX )
    {
        i_volume = AOUT_VOLUME_MAX;
    }

    aout_VolumeSoftSet( p_aout, (audio_volume_t)i_volume );
}

/* Placeholder for pf_volume_infos(). */
int aout_VolumeSoftInfos( aout_instance_t * p_aout, audio_volume_t * pi_soft )
{
    (void)p_aout;
    *pi_soft = 0;
    return 0;
}

/* Placeholder for pf_volume_get(). */
int aout_VolumeSoftGet( aout_instance_t * p_aout, audio_volume_t * pi_volume )
{
    *pi_volume = p_aout->output.i_volume;
    return 0;
}


/* Placeholder for pf_volume_set(). */
int aout_VolumeSoftSet( aout_instance_t * p_aout, audio_volume_t i_volume )
{
    aout_MixerMultiplierSet( p_aout, (float)i_volume / AOUT_VOLUME_DEFAULT );
    p_aout->output.i_volume = i_volume;
    return 0;
}

/*
 * The next functions are not supposed to be called by the interface, but
 * are placeholders for unsupported scaling.
 */

/* Meant to be called by the output plug-in's Open(). */
void aout_VolumeNoneInit( aout_instance_t * p_aout )
{
    p_aout->output.pf_volume_infos = aout_VolumeNoneInfos;
    p_aout->output.pf_volume_get = aout_VolumeNoneGet;
    p_aout->output.pf_volume_set = aout_VolumeNoneSet;
}

/* Placeholder for pf_volume_infos(). */
int aout_VolumeNoneInfos( aout_instance_t * p_aout, audio_volume_t * pi_soft )
{
    (void)p_aout; (void)pi_soft;
    return -1;
}

/* Placeholder for pf_volume_get(). */
int aout_VolumeNoneGet( aout_instance_t * p_aout, audio_volume_t * pi_volume )
{
    (void)p_aout; (void)pi_volume;
    return -1;
}

/* Placeholder for pf_volume_set(). */
int aout_VolumeNoneSet( aout_instance_t * p_aout, audio_volume_t i_volume )
{
    (void)p_aout; (void)i_volume;
    return -1;
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
    int i;
    bool b_error = 0;

    aout_lock_mixer( p_aout );

    if ( p_aout->i_nb_inputs == 0 )
    {
        aout_unlock_mixer( p_aout );
        msg_Err( p_aout, "no decoder thread" );
        return -1;
    }

    /* Lock all inputs. */
    aout_lock_input_fifos( p_aout );

    for ( i = 0; i < p_aout->i_nb_inputs; i++ )
    {
        aout_lock_input( p_aout, p_aout->pp_inputs[i] );
        aout_InputDelete( p_aout, p_aout->pp_inputs[i] );
    }

    aout_MixerDelete( p_aout );

    /* Re-open the output plug-in. */
    aout_OutputDelete( p_aout );

    if ( aout_OutputNew( p_aout, &p_aout->pp_inputs[0]->input ) == -1 )
    {
        /* Release all locks and report the error. */
        for ( i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            vlc_mutex_unlock( &p_aout->pp_inputs[i]->lock );
        }
        aout_unlock_input_fifos( p_aout );
        aout_unlock_mixer( p_aout );
        return -1;
    }

    if ( aout_MixerNew( p_aout ) == -1 )
    {
        aout_OutputDelete( p_aout );
        for ( i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            vlc_mutex_unlock( &p_aout->pp_inputs[i]->lock );
        }
        aout_unlock_input_fifos( p_aout );
        aout_unlock_mixer( p_aout );
        return -1;
    }

    /* Re-open all inputs. */
    for ( i = 0; i < p_aout->i_nb_inputs; i++ )
    {
        aout_input_t * p_input = p_aout->pp_inputs[i];
        b_error |= aout_InputNew( p_aout, p_input, &p_input->request_vout );
        p_input->b_changed = 1;
        aout_unlock_input( p_aout, p_input );
    }

    aout_unlock_input_fifos( p_aout );
    aout_unlock_mixer( p_aout );

    return b_error;
}

/*****************************************************************************
 * aout_FindAndRestart : find the audio output instance and restart
 *****************************************************************************
 * This is used for callbacks of the configuration variables, and we believe
 * that when those are changed, it is a significant change which implies
 * rebuilding the audio-device and audio-channels variables.
 *****************************************************************************/
int aout_FindAndRestart( vlc_object_t * p_this, const char *psz_name,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    aout_instance_t * p_aout = vlc_object_find( p_this, VLC_OBJECT_AOUT,
                                                FIND_ANYWHERE );

    (void)psz_name; (void)oldval; (void)newval; (void)p_data;
    if ( p_aout == NULL ) return VLC_SUCCESS;

    if ( var_Type( p_aout, "audio-device" ) != 0 )
    {
        var_Destroy( p_aout, "audio-device" );
    }
    if ( var_Type( p_aout, "audio-channels" ) != 0 )
    {
        var_Destroy( p_aout, "audio-channels" );
    }

    aout_Restart( p_aout );
    vlc_object_release( p_aout );

    return VLC_SUCCESS;
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
        if ( var_Type( p_aout, "audio-channels" ) >= 0 )
        {
            var_Destroy( p_aout, "audio-channels" );
        }
    }
    aout_Restart( p_aout );
    return 0;
}

/** Enable or disable an audio filter
 * \param p_this a vlc object
 * \param psz_name name of the filter
 * \param b_add are we adding or removing the filter ?
 */
void aout_EnableFilter( vlc_object_t *p_this, const char *psz_name,
                        bool b_add )
{
    aout_instance_t *p_aout = vlc_object_find( p_this, VLC_OBJECT_AOUT,
                                               FIND_ANYWHERE );

    if( AoutChangeFilterString( p_this, p_aout, "audio-filter", psz_name, b_add ) )
    {
        if( p_aout )
            AoutInputsMarkToRestart( p_aout );
    }

    if( p_aout )
        vlc_object_release( p_aout );
}

/**
 * Change audio visualization
 * -1 goes backwards, +1 goes forward
 */
char *aout_VisualChange( vlc_object_t *p_this, int i_skip )
{
    (void)p_this; (void)i_skip;
    msg_Err( p_this, "FIXME: %s (%s %d) isn't implemented.", __func__,
             __FILE__, __LINE__ );
    return strdup("foobar");
}
