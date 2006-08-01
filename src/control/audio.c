/*****************************************************************************
 * libvlc_audio.c: New libvlc audio control API
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Filippo Carone <filippo@carone.org>
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

#include <libvlc_internal.h>
#include <vlc/libvlc.h>

#include <audio_output.h> /* for audio_volume_t, AOUT_VOLUME_MAX */
#include <vlc/intf.h>

/*****************************************************************************
 * libvlc_audio_get_mute : Get the volume state, true if muted
 *****************************************************************************/
void libvlc_audio_toggle_mute( libvlc_instance_t *p_instance,
                               libvlc_exception_t *p_e )
{
    aout_VolumeMute( p_instance->p_vlc, NULL );
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

void libvlc_audio_set_mute( libvlc_instance_t *p_instance, vlc_bool_t status,
                            libvlc_exception_t *p_e )
{
    if ( status )
    {
        /* Check if the volume is already muted */
        if (! libvlc_audio_get_volume( p_instance, p_e ) )
        {
            return;
        }
        aout_VolumeMute( p_instance->p_vlc, NULL );
    }
    else
    {
        /* the aout_VolumeMute is a toggle function, so this is enough. */
        aout_VolumeMute( p_instance->p_vlc, NULL );
    }
}

/*****************************************************************************
 * libvlc_audio_get_volume : Get the current volume (range 0-200 %)
 *****************************************************************************/
int libvlc_audio_get_volume( libvlc_instance_t *p_instance,
                             libvlc_exception_t *p_e )
{
    audio_volume_t i_volume;

    aout_VolumeGet( p_instance->p_vlc, &i_volume );

    return i_volume*200/AOUT_VOLUME_MAX;
}


/*****************************************************************************
 * libvlc_audio_set_volume : Set the current volume
 *****************************************************************************/
void libvlc_audio_set_volume( libvlc_instance_t *p_instance, int i_volume,
                              libvlc_exception_t *p_e )
{
    if( i_volume >= 0 && i_volume <= 200 )
    {
        i_volume = i_volume * AOUT_VOLUME_MAX / 200;
        aout_VolumeSet( p_instance->p_vlc, i_volume );
    }
    else
    {
        libvlc_exception_raise( p_e, "Volume out of range" );
    }
}

