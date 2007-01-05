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

#include <vlc_input.h>
#include <vlc_aout.h>

/*
 * Remember to release the returned input_thread_t since it is locked at
 * the end of this function.
 */
static input_thread_t *GetInput( libvlc_input_t *p_input,
                                 libvlc_exception_t *p_exception )
{
    input_thread_t *p_input_thread = NULL;

    if( !p_input )
    {
        libvlc_exception_raise( p_exception, "Input is NULL" );
        return NULL;
    }

    p_input_thread = (input_thread_t*)vlc_object_get(
                                 p_input->p_instance->p_libvlc_int,
                                 p_input->i_input_id );
    if( !p_input_thread )
    {
        libvlc_exception_raise( p_exception, "Input does not exist" );
        return NULL;
    }

    return p_input_thread;
}

/*
 * Remember to release the returned aout_instance_t since it is locked at
 * the end of this function.
 */
static aout_instance_t *GetAOut( libvlc_instance_t *p_instance,
                                 libvlc_exception_t *p_exception )
{
    aout_instance_t * p_aout = NULL;

    p_aout = vlc_object_find( p_instance->p_libvlc_int, VLC_OBJECT_AOUT, FIND_CHILD );
    if( !p_aout )
    {
        libvlc_exception_raise( p_exception, "No active audio output" );
        return NULL;
    }

    return p_aout;
}


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
int libvlc_audio_get_track( libvlc_input_t *p_input,
                            libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = GetInput( p_input, p_e );
    int i_track = 0;

    i_track = var_GetInteger( p_input_thread, "audio-es" );
    vlc_object_release( p_input_thread );

    return i_track;
}

/*****************************************************************************
 * libvlc_audio_set_track : Set the current audio track
 *****************************************************************************/
void libvlc_audio_set_track( libvlc_input_t *p_input, int i_track,
                             libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = GetInput( p_input, p_e );
    vlc_value_t val_list;
    int i_ret = -1;
    int i;

    var_Change( p_input_thread, "audio-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        vlc_value_t val = val_list.p_list->p_values[i];
        if( i_track == val.i_int )
        {
            i_ret = var_SetInteger( p_input_thread, "audio-es", i_track );
            if( i_ret < 0 )
            {
                libvlc_exception_raise( p_e, "Setting audio track failed" );
            }
            vlc_object_release( p_input_thread );
            return;
        }
    }
    libvlc_exception_raise( p_e, "Audio track out of range" );
    vlc_object_release( p_input_thread );
}

/*****************************************************************************
 * libvlc_audio_get_channel : Get the current audio channel
 *****************************************************************************/
char *libvlc_audio_get_channel( libvlc_instance_t *p_instance,
                                libvlc_exception_t *p_e )
{
    aout_instance_t *p_aout = GetAOut( p_instance, p_e );
    char *psz_channel = NULL;
    vlc_value_t val;

    var_Get( p_aout, "audio-channels", &val );
    switch( val.i_int )
    {
        case AOUT_VAR_CHAN_RSTEREO:
            psz_channel = strdup("reverse stereo");
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
    vlc_object_release( p_aout );
    return psz_channel;
}

/*****************************************************************************
 * libvlc_audio_set_channel : Set the current audio channel
 *****************************************************************************/
void libvlc_audio_set_channel( libvlc_instance_t *p_instance, char *psz_channel,
                               libvlc_exception_t *p_e )
{
    aout_instance_t *p_aout = GetAOut( p_instance, p_e );
    vlc_value_t val_list, text_list;
    int i_ret = -1, i;

    i_ret = var_Change( p_aout, "audio-channels", VLC_VAR_GETCHOICES, &val_list, &text_list );
    if( (i_ret < 0) || !psz_channel )
    {
        libvlc_exception_raise( p_e, "Audio channel out of range" );
        vlc_object_release( p_aout );
        return;
    }

    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        vlc_value_t val = val_list.p_list->p_values[i];
        vlc_value_t text = text_list.p_list->p_values[i];

        if( strncasecmp( psz_channel, text.psz_string, strlen(text.psz_string) ) == 0 )
        {
            i_ret = var_Set( p_aout, "audio-channels", val );
            if( i_ret < 0 )
            {
		libvlc_exception_raise( p_e, "failed setting audio range" );
    	    	vlc_object_release( p_aout );
                return;
            }
    	    vlc_object_release( p_aout );
            return; /* Found */
        }
    }
    libvlc_exception_raise( p_e, "Audio channel out of range" );
    vlc_object_release( p_aout );
}

