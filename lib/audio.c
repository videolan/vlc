/*****************************************************************************
 * libvlc_audio.c: New libvlc audio control API
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *          Jean-Paul Saman <jpsaman _at_ m2x _dot_ nl>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <math.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_player.h>

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_aout.h>
#include <vlc_modules.h>

#include "libvlc_internal.h"
#include "media_player_internal.h"

/*
 * Remember to release the returned audio_output_t since it is locked at
 * the end of this function.
 */
static audio_output_t *GetAOut( libvlc_media_player_t *mp )
{
    assert( mp != NULL );

    audio_output_t *p_aout = input_resource_HoldAout( mp->input.p_resource );
    if( p_aout == NULL )
        libvlc_printerr( "No active audio output" );
    return p_aout;
}

/*****************************************
 * Get the list of available audio outputs
 *****************************************/
libvlc_audio_output_t *
        libvlc_audio_output_list_get( libvlc_instance_t *p_instance )
{
    size_t count;
    module_t **module_list = module_list_get( &count );
    libvlc_audio_output_t *list = NULL;

    for (size_t i = 0; i < count; i++)
    {
        module_t *module = module_list[i];

        if( !module_provides( module, "audio output" ) )
            continue;

        libvlc_audio_output_t *item = malloc( sizeof( *item ) );
        if( unlikely(item == NULL) )
        {
    error:
            libvlc_printerr( "Not enough memory" );
            libvlc_audio_output_list_release( list );
            list = NULL;
            break;
        }

        item->psz_name = strdup( module_get_object( module ) );
        item->psz_description = strdup( module_get_name( module, true ) );
        if( unlikely(item->psz_name == NULL || item->psz_description == NULL) )
        {
            free( item );
            goto error;
        }
        item->p_next = list;
        list = item;
    }
    module_list_free( module_list );

    VLC_UNUSED( p_instance );
    return list;
}

/********************************************
 * Free the list of available audio outputs
 ***********************************************/
void libvlc_audio_output_list_release( libvlc_audio_output_t *list )
{
    while( list != NULL )
    {
        libvlc_audio_output_t *next = list->p_next;

        free( list->psz_name );
        free( list->psz_description );
        free( list );
        list = next;
    }
}


/***********************
 * Set the audio output.
 ***********************/
int libvlc_audio_output_set( libvlc_media_player_t *mp, const char *psz_name )
{
    char *value;

    if( !module_exists( psz_name )
     || asprintf( &value, "%s,none", psz_name ) == -1 )
        return -1;
    var_SetString( mp, "aout", value );
    free( value );
    return 0;
}

libvlc_audio_output_device_t *
libvlc_audio_output_device_list_get( libvlc_instance_t *p_instance,
                                     const char *aout )
{
    char varname[32];
    if( (size_t)snprintf( varname, sizeof(varname), "%s-audio-device", aout )
                                                           >= sizeof(varname) )
        return NULL;

    libvlc_audio_output_device_t *list = NULL, **pp = &list;
    char **values, **texts;
    ssize_t count = config_GetPszChoices( VLC_OBJECT(p_instance->p_libvlc_int),
                                          varname, &values, &texts );
    for( ssize_t i = 0; i < count; i++ )
    {
        libvlc_audio_output_device_t *item = malloc( sizeof(*item) );
        if( unlikely(item == NULL) )
            break;

        *pp = item;
        pp = &item->p_next;
        item->psz_device = values[i];
        item->psz_description = texts[i];
    }

    *pp = NULL;
    free( texts );
    free( values );
    (void) p_instance;
    return list;
}

void libvlc_audio_output_device_list_release( libvlc_audio_output_device_t *l )
{
    while( l != NULL )
    {
        libvlc_audio_output_device_t *next = l->p_next;

        free( l->psz_description );
        free( l->psz_device );
        free( l );
        l = next;
    }
}

int libvlc_audio_output_device_count( libvlc_instance_t *p_instance,
                                      const char *psz_audio_output )
{
    (void) p_instance; (void) psz_audio_output;
    return 0;
}

char *libvlc_audio_output_device_longname( libvlc_instance_t *p_instance,
                                           const char *psz_audio_output,
                                           int i_device )
{
    (void) p_instance; (void) psz_audio_output; (void) i_device;
    return NULL;
}

char *libvlc_audio_output_device_id( libvlc_instance_t *p_instance,
                                     const char *psz_audio_output,
                                     int i_device )
{
    (void) p_instance; (void) psz_audio_output; (void) i_device;
    return NULL;
}

/*****************************
 * Set device for using
 *****************************/
void libvlc_audio_output_device_set( libvlc_media_player_t *mp,
                                     const char *psz_audio_output,
                                     const char *psz_device_id )
{
    char *psz_config_name;
    if( !psz_audio_output || !psz_device_id )
        return;
    if( asprintf( &psz_config_name, "%s-audio-device", psz_audio_output ) == -1 )
        return;
    if( !var_Type( mp, psz_config_name ) )
        /* Don't recreate the same variable over and over and over... */
        var_Create( mp, psz_config_name, VLC_VAR_STRING );
    var_SetString( mp, psz_config_name, psz_device_id );
    free( psz_config_name );
}

int libvlc_audio_output_get_device_type( libvlc_media_player_t *mp )
{
    (void) mp;
    return libvlc_AudioOutputDevice_Error;
}

void libvlc_audio_output_set_device_type( libvlc_media_player_t *mp,
                                          int device_type )
{
    (void) mp; (void) device_type;
}

void libvlc_audio_toggle_mute( libvlc_media_player_t *mp )
{
    int mute = libvlc_audio_get_mute( mp );
    if( mute != -1 )
        libvlc_audio_set_mute( mp, !mute );
}

int libvlc_audio_get_mute( libvlc_media_player_t *mp )
{
    int mute = -1;

    audio_output_t *aout = GetAOut( mp );
    if( aout != NULL )
    {
        mute = aout_MuteGet( aout );
        vlc_object_release( aout );
    }
    return mute;
}

void libvlc_audio_set_mute( libvlc_media_player_t *mp, int mute )
{
    audio_output_t *aout = GetAOut( mp );
    if( aout != NULL )
    {
        mute = aout_MuteSet( aout, mute );
        vlc_object_release( aout );
    }
}

int libvlc_audio_get_volume( libvlc_media_player_t *mp )
{
    int volume = -1;

    audio_output_t *aout = GetAOut( mp );
    if( aout != NULL )
    {
        float vol = aout_VolumeGet( aout );
        vlc_object_release( aout );
        volume = lroundf( vol * 100.f );
    }
    return volume;
}

int libvlc_audio_set_volume( libvlc_media_player_t *mp, int volume )
{
    float vol = volume / 100.f;
    if (vol < 0.f)
    {
        libvlc_printerr( "Volume out of range" );
        return -1;
    }

    int ret = -1;
    audio_output_t *aout = GetAOut( mp );
    if( aout != NULL )
    {
        ret = aout_VolumeSet( aout, vol );
        vlc_object_release( aout );
    }
    return ret;
}

/*****************************************************************************
 * libvlc_audio_get_track_count : Get the number of available audio tracks
 *****************************************************************************/
int libvlc_audio_get_track_count( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    int i_track_count;

    if( !p_input_thread )
        return -1;

    i_track_count = var_CountChoices( p_input_thread, "audio-es" );

    vlc_object_release( p_input_thread );
    return i_track_count;
}

/*****************************************************************************
 * libvlc_audio_get_track_description : Get the description of available audio tracks
 *****************************************************************************/
libvlc_track_description_t *
        libvlc_audio_get_track_description( libvlc_media_player_t *p_mi )
{
    return libvlc_get_track_description( p_mi, "audio-es" );
}

/*****************************************************************************
 * libvlc_audio_get_track : Get the current audio track
 *****************************************************************************/
int libvlc_audio_get_track( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    if( !p_input_thread )
        return -1;

    int id = var_GetInteger( p_input_thread, "audio-es" );
    vlc_object_release( p_input_thread );
    return id;
}

/*****************************************************************************
 * libvlc_audio_set_track : Set the current audio track
 *****************************************************************************/
int libvlc_audio_set_track( libvlc_media_player_t *p_mi, int i_track )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    vlc_value_t val_list;
    int i_ret = -1;

    if( !p_input_thread )
        return -1;

    var_Change( p_input_thread, "audio-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( int i = 0; i < val_list.p_list->i_count; i++ )
    {
        if( i_track == val_list.p_list->p_values[i].i_int )
        {
            if( var_SetInteger( p_input_thread, "audio-es", i_track ) < 0 )
                break;
            i_ret = 0;
            goto end;
        }
    }
    libvlc_printerr( "Track identifier not found" );
end:
    var_FreeList( &val_list, NULL );
    vlc_object_release( p_input_thread );
    return i_ret;
}

/*****************************************************************************
 * libvlc_audio_get_channel : Get the current audio channel
 *****************************************************************************/
int libvlc_audio_get_channel( libvlc_media_player_t *mp )
{
    audio_output_t *p_aout = GetAOut( mp );
    if( !p_aout )
        return 0;

    int val = var_GetInteger( p_aout, "stereo-mode" );
    vlc_object_release( p_aout );
    return val;
}

/*****************************************************************************
 * libvlc_audio_set_channel : Set the current audio channel
 *****************************************************************************/
int libvlc_audio_set_channel( libvlc_media_player_t *mp, int channel )
{
    audio_output_t *p_aout = GetAOut( mp );
    int ret = 0;

    if( !p_aout )
        return -1;

    if( var_SetInteger( p_aout, "stereo-mode", channel ) < 0 )
    {
        libvlc_printerr( "Audio channel out of range" );
        ret = -1;
    }
    vlc_object_release( p_aout );
    return ret;
}

/*****************************************************************************
 * libvlc_audio_get_delay : Get the current audio delay
 *****************************************************************************/
int64_t libvlc_audio_get_delay( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread ( p_mi );
    int64_t val = 0;
    if( p_input_thread != NULL )
    {
      val = var_GetTime( p_input_thread, "audio-delay" );
      vlc_object_release( p_input_thread );
    }
    return val;
}

/*****************************************************************************
 * libvlc_audio_set_delay : Set the current audio delay
 *****************************************************************************/
int libvlc_audio_set_delay( libvlc_media_player_t *p_mi, int64_t i_delay )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread ( p_mi );
    int ret = 0;
    if( p_input_thread != NULL )
    {
      var_SetTime( p_input_thread, "audio-delay", i_delay );
      vlc_object_release( p_input_thread );
    }
    else
    {
      ret = -1;
    }
    return ret;
}
