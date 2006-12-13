/*****************************************************************************
 * libvlc_audio.c: New libvlc audio control API
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *          Jean-Paul Saman <jpsaman _at_ m2x _dot_ nl>
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

#include "libvlc_internal.h"
#include <vlc/libvlc.h>

#include <vlc_aout.h>

/*****************************************************************************
 * libvlc_audio_get_mute : Get the volume state, true if muted
 *****************************************************************************/
void libvlc_audio_toggle_mute( libvlc_instance_t *p_instance,
                               libvlc_exception_t *p_e )
{
    aout_VolumeMute( p_instance->p_libvlc_int, NULL );
}

vlc_bool_t libvlc_audio_get_mute( libvlc_instance_t *p_instance,
                                  libvlc_exception_t *p_e )
{
    /*
     * If the volume level is 0, then the channel is muted
     */
    audio_volume_t i_volume;

    i_volume = libvlc_audio_get_volume(p_instance, p_e);
    if ( i_volume == 0 )
        return VLC_TRUE;
    return VLC_FALSE;
}

void libvlc_audio_set_mute( libvlc_instance_t *p_instance, vlc_bool_t mute,
                            libvlc_exception_t *p_e )
{
    if ( mute ^ libvlc_audio_get_mute( p_instance, p_e ) )
    {
        aout_VolumeMute( p_instance->p_libvlc_int, NULL );
    }
}

/*****************************************************************************
 * libvlc_audio_get_volume : Get the current volume (range 0-200 %)
 *****************************************************************************/
int libvlc_audio_get_volume( libvlc_instance_t *p_instance,
                             libvlc_exception_t *p_e )
{
    audio_volume_t i_volume;

    aout_VolumeGet( p_instance->p_libvlc_int, &i_volume );

    return (i_volume*200+AOUT_VOLUME_MAX/2)/AOUT_VOLUME_MAX;
}


/*****************************************************************************
 * libvlc_audio_set_volume : Set the current volume
 *****************************************************************************/
void libvlc_audio_set_volume( libvlc_instance_t *p_instance, int i_volume,
                              libvlc_exception_t *p_e )
{
    if( i_volume >= 0 && i_volume <= 200 )
    {
        i_volume = (i_volume * AOUT_VOLUME_MAX + 100) / 200;

        aout_VolumeSet( p_instance->p_libvlc_int, i_volume );
    }
    else
    {
        libvlc_exception_raise( p_e, "Volume out of range" );
    }
}

/*****************************************************************************
 * libvlc_audio_get_track : Get the current audio track
 *****************************************************************************/
int libvlc_audio_get_track( libvlc_instance_t *p_instance,
                            libvlc_exception_t *p_e )
{
    int i_track = 0;

    i_track = var_GetInteger( p_instance->p_libvlc_int, "audio-track" );

    return i_track;
}

/*****************************************************************************
 * libvlc_audio_set_track : Set the current audio track
 *****************************************************************************/
void libvlc_audio_set_track( libvlc_instance_t *p_instance, int i_track,
                             libvlc_exception_t *p_e )
{
    int i_ret = -1;

    i_ret = var_SetInteger( p_instance->p_libvlc_int, "audio-track", i_track );

    if( i_ret < 0 )
    {
        libvlc_exception_raise( p_e, "Setting audio track failed" );
    }
}

/*****************************************************************************
 * libvlc_audio_get_channel : Get the current audio channel
 *****************************************************************************/
char *libvlc_audio_get_channel( libvlc_instance_t *p_instance,
                                libvlc_exception_t *p_e )
{
    char *psz_channel = NULL;
    int i_channel = 0;

    i_channel = var_GetInteger( p_instance->p_libvlc_int, "audio-channel" );
    switch( i_channel )
    {
        case AOUT_VAR_CHAN_RSTEREO:
            psz_channel = strdup("reverse");
            break;
        case AOUT_VAR_CHAN_STEREO:
            psz_channel = strdup("stereo");
            break;
        case AOUT_VAR_CHAN_LEFT:
            psz_channel = strdup("left");
            break;
        case AOUT_VAR_CHAN_RIGHT:
            psz_channel = strdup("right");
            break;
        case AOUT_VAR_CHAN_DOLBYS:
            psz_channel = strdup("dolby");
            break;
        default:
            psz_channel = strdup("disabled");
            break;
    }
    return psz_channel;
}

/*****************************************************************************
 * libvlc_audio_set_channel : Set the current audio channel
 *****************************************************************************/
void libvlc_audio_set_channel( libvlc_instance_t *p_instance, char *psz_channel,
                               libvlc_exception_t *p_e )
{
    int i_ret = -1;
    int i_channel = 0;

    if( !psz_channel )
    {
        libvlc_exception_raise( p_e, "Audio track out of range" );
    }
    else
    {
        if( strncmp( psz_channel, "reverse", 7 ) == 0 )
            i_channel = AOUT_VAR_CHAN_RSTEREO;
        else if( strncmp( psz_channel, "stereo", 6 ) == 0 )
            i_channel = AOUT_VAR_CHAN_STEREO;
        else if( strncmp( psz_channel, "left", 4 ) == 0 )
            i_channel = AOUT_VAR_CHAN_LEFT;
        else if( strncmp( psz_channel, "right", 5 ) == 0 )
            i_channel = AOUT_VAR_CHAN_RIGHT;
        else if( strncmp( psz_channel, "dolby", 5 ) == 0 )
            i_channel = AOUT_VAR_CHAN_DOLBYS;

        i_ret = var_SetInteger( p_instance->p_libvlc_int, "audio-channel", i_channel );
        if( i_ret < 0 )
        {
            libvlc_exception_raise( p_e, "Audio track out of range" );
        }
    }
}
