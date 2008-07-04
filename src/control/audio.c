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
    VLC_UNUSED(p_e);

    aout_VolumeMute( p_instance->p_libvlc_int, NULL );
}

int libvlc_audio_get_mute( libvlc_instance_t *p_instance,
                           libvlc_exception_t *p_e )
{
    /*
     * If the volume level is 0, then the channel is muted
     */
    audio_volume_t i_volume;

    i_volume = libvlc_audio_get_volume(p_instance, p_e);
    if ( i_volume == 0 )
        return true;
    return false;
}

void libvlc_audio_set_mute( libvlc_instance_t *p_instance, int mute,
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
    VLC_UNUSED(p_e);

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
 * libvlc_audio_get_track_count : Get the number of available audio tracks
 *****************************************************************************/
int libvlc_audio_get_track_count( libvlc_media_player_t *p_mi, 
                                  libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi, p_e );
    vlc_value_t val_list;

    if( !p_input_thread )
        return -1;

    var_Change( p_input_thread, "audio-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    vlc_object_release( p_input_thread );
    return val_list.p_list->i_count;
}

/*****************************************************************************
 * libvlc_audio_get_track : Get the current audio track
 *****************************************************************************/
int libvlc_audio_get_track( libvlc_media_player_t *p_mi,
                            libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi, p_e );
    vlc_value_t val_list;
    vlc_value_t val;
    int i_track = -1;
    int i_ret = -1;
    int i;

    if( !p_input_thread )
        return -1;

    i_ret = var_Get( p_input_thread, "audio-es", &val );
    if( i_ret < 0 )
    {
        libvlc_exception_raise( p_e, "Getting Audio track information failed" );
        vlc_object_release( p_input_thread );
        return i_ret;
    }

    var_Change( p_input_thread, "audio-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        vlc_value_t track_val = val_list.p_list->p_values[i];
        if( track_val.i_int == val.i_int )
        {
            i_track = i;
            break;
       }
    }
    vlc_object_release( p_input_thread );
    return i_track;
}


/*****************************************************************************
 * libvlc_audio_set_track : Set the current audio track
 *****************************************************************************/
void libvlc_audio_set_track( libvlc_media_player_t *p_mi, int i_track,
                             libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi, p_e );
    vlc_value_t val_list;
    int i_ret = -1;
    int i;

    if( !p_input_thread )
        return;

    var_Change( p_input_thread, "audio-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        vlc_value_t val = val_list.p_list->p_values[i];
        if( i_track == val.i_int )
        {
            i_ret = var_Set( p_input_thread, "audio-es", val );
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
int libvlc_audio_get_channel( libvlc_instance_t *p_instance,
                                libvlc_exception_t *p_e )
{
    aout_instance_t *p_aout = GetAOut( p_instance, p_e );
    if( p_aout )
    {
        vlc_value_t val;

        var_Get( p_aout, "audio-channels", &val );
        vlc_object_release( p_aout );
        return val.i_int;
    }
    return -1;
}

/*****************************************************************************
 * libvlc_audio_set_channel : Set the current audio channel
 *****************************************************************************/
void libvlc_audio_set_channel( libvlc_instance_t *p_instance, int i_channel,
                               libvlc_exception_t *p_e )
{
    aout_instance_t *p_aout = GetAOut( p_instance, p_e );
    if( p_aout )
    {
        vlc_value_t val;
        int i_ret = -1;

        val.i_int = i_channel;
        switch( i_channel )
        {
            case AOUT_VAR_CHAN_RSTEREO:
            case AOUT_VAR_CHAN_STEREO:
            case AOUT_VAR_CHAN_LEFT:
            case AOUT_VAR_CHAN_RIGHT:
            case AOUT_VAR_CHAN_DOLBYS:
                i_ret = var_Set( p_aout, "audio-channels", val );
                if( i_ret < 0 )
                {
                    libvlc_exception_raise( p_e, "Failed setting audio channel" );
                    vlc_object_release( p_aout );
                    return;
                }
                vlc_object_release( p_aout );
                return; /* Found */
            default:
                libvlc_exception_raise( p_e, "Audio channel out of range" );
                break;
        }
        vlc_object_release( p_aout );
    }
}
