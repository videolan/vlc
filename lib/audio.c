/*****************************************************************************
 * libvlc_audio.c: New libvlc audio control API
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
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
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_picture.h>
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
            free( item->psz_name );
            free( item->psz_description );
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

    /* Forget the existing audio output */
    input_resource_ResetAout(mp->input.p_resource);

    /* Create a new audio output */
    audio_output_t *aout = input_resource_GetAout(mp->input.p_resource);
    if( aout != NULL )
        input_resource_PutAout(mp->input.p_resource, aout);

    return 0;
}

libvlc_audio_output_device_t *
libvlc_audio_output_device_enum( libvlc_media_player_t *mp )
{
    audio_output_t *aout = GetAOut( mp );
    if( aout == NULL )
        return NULL;

    libvlc_audio_output_device_t *list, **pp = &list;
    char **values, **texts;

    int n = aout_DevicesList( aout, &values, &texts );
    vlc_object_release( aout );
    if( n < 0 )
        goto err;

    for (int i = 0; i < n; i++)
    {
        libvlc_audio_output_device_t *item = malloc( sizeof(*item) );
        if( unlikely(item == NULL) )
        {
            free( texts[i] );
            free( values[i] );
            continue;
        }

        *pp = item;
        pp = &item->p_next;
        item->psz_device = values[i];
        item->psz_description = texts[i];
    }

    free( texts );
    free( values );
err:
    *pp = NULL;
    return list;
}

libvlc_audio_output_device_t *
libvlc_audio_output_device_list_get( libvlc_instance_t *p_instance,
                                     const char *aout )
{
    char varname[32];
    if( (size_t)snprintf( varname, sizeof(varname), "%s-audio-device", aout )
                                                           >= sizeof(varname) )
        return NULL;

    if( config_GetType(varname) != VLC_VAR_STRING )
        return NULL;

    libvlc_audio_output_device_t *list = NULL, **pp = &list;
    char **values, **texts;
    ssize_t count = config_GetPszChoices( varname, &values, &texts );
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

/*****************************
 * Set device for using
 *****************************/
void libvlc_audio_output_device_set( libvlc_media_player_t *mp,
                                     const char *module, const char *devid )
{
    if( devid == NULL )
        return;

    if( module != NULL )
    {
        char *cfg_name;

        if( asprintf( &cfg_name, "%s-audio-device", module ) == -1 )
            return;

        if( !var_Type( mp, cfg_name ) )
            /* Don't recreate the same variable over and over and over... */
            var_Create( mp, cfg_name, VLC_VAR_STRING );
        var_SetString( mp, cfg_name, devid );
        free( cfg_name );
        return;
    }

    audio_output_t *aout = GetAOut( mp );
    if( aout == NULL )
        return;

    aout_DeviceSet( aout, devid );
    vlc_object_release( aout );
}

char *libvlc_audio_output_device_get( libvlc_media_player_t *mp )
{
    audio_output_t *aout = GetAOut( mp );
    if( aout == NULL )
        return NULL;

    char *devid = aout_DeviceGet( aout );

    vlc_object_release( aout );

    return devid;
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
    if (!isgreaterequal(vol, 0.f))
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

    input_Release(p_input_thread);
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
    input_Release(p_input_thread);
    return id;
}

/*****************************************************************************
 * libvlc_audio_set_track : Set the current audio track
 *****************************************************************************/
int libvlc_audio_set_track( libvlc_media_player_t *p_mi, int i_track )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    vlc_value_t *val_list;
    size_t count;
    int i_ret = -1;

    if( !p_input_thread )
        return -1;

    var_Change( p_input_thread, "audio-es", VLC_VAR_GETCHOICES,
                &count, &val_list, (char ***)NULL );
    for( size_t i = 0; i < count; i++ )
    {
        if( i_track == val_list[i].i_int )
        {
            if( var_SetInteger( p_input_thread, "audio-es", i_track ) < 0 )
                break;
            i_ret = 0;
            goto end;
        }
    }
    libvlc_printerr( "Track identifier not found" );
end:
    free( val_list );
    input_Release(p_input_thread);
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
      val = US_FROM_VLC_TICK( var_GetInteger( p_input_thread, "audio-delay" ) );
      input_Release(p_input_thread);
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
      var_SetInteger( p_input_thread, "audio-delay", VLC_TICK_FROM_US( i_delay ) );
      input_Release(p_input_thread);
    }
    else
    {
      ret = -1;
    }
    return ret;
}

/*****************************************************************************
 * libvlc_audio_equalizer_get_preset_count : Get the number of equalizer presets
 *****************************************************************************/
unsigned libvlc_audio_equalizer_get_preset_count( void )
{
    return NB_PRESETS;
}

/*****************************************************************************
 * libvlc_audio_equalizer_get_preset_name : Get the name for a preset
 *****************************************************************************/
const char *libvlc_audio_equalizer_get_preset_name( unsigned u_index )
{
    if ( u_index >= NB_PRESETS )
        return NULL;

    return preset_list_text[ u_index ];
}

/*****************************************************************************
 * libvlc_audio_equalizer_get_band_count : Get the number of equalizer frequency bands
 *****************************************************************************/
unsigned libvlc_audio_equalizer_get_band_count( void )
{
    return EQZ_BANDS_MAX;
}

/*****************************************************************************
 * libvlc_audio_equalizer_get_band_frequency : Get the frequency for a band
 *****************************************************************************/
float libvlc_audio_equalizer_get_band_frequency( unsigned u_index )
{
    if ( u_index >= EQZ_BANDS_MAX )
        return -1.f;

    return f_iso_frequency_table_10b[ u_index ];
}

/*****************************************************************************
 * libvlc_audio_equalizer_new : Create a new audio equalizer with zeroed values
 *****************************************************************************/
libvlc_equalizer_t *libvlc_audio_equalizer_new( void )
{
    libvlc_equalizer_t *p_equalizer;
    p_equalizer = malloc( sizeof( *p_equalizer ) );
    if ( unlikely( p_equalizer == NULL ) )
        return NULL;

    p_equalizer->f_preamp = 0.f;
    for ( unsigned i = 0; i < EQZ_BANDS_MAX; i++ )
        p_equalizer->f_amp[ i ] = 0.f;

    return p_equalizer;
}

/*****************************************************************************
 * libvlc_audio_equalizer_new_from_preset : Create a new audio equalizer based on a preset
 *****************************************************************************/
libvlc_equalizer_t *libvlc_audio_equalizer_new_from_preset( unsigned u_index )
{
    libvlc_equalizer_t *p_equalizer;

    if ( u_index >= NB_PRESETS )
        return NULL;

    p_equalizer = malloc( sizeof( *p_equalizer ) );
    if ( unlikely( p_equalizer == NULL ) )
        return NULL;

    p_equalizer->f_preamp = eqz_preset_10b[ u_index ].f_preamp;

    for ( unsigned i = 0; i < EQZ_BANDS_MAX; i++ )
        p_equalizer->f_amp[ i ] = eqz_preset_10b[ u_index ].f_amp[ i ];

    return p_equalizer;
}

/*****************************************************************************
 * libvlc_audio_equalizer_release : Release a previously created equalizer
 *****************************************************************************/
void libvlc_audio_equalizer_release( libvlc_equalizer_t *p_equalizer )
{
    free( p_equalizer );
}

/*****************************************************************************
 * libvlc_audio_equalizer_set_preamp : Set the preamp value for an equalizer
 *****************************************************************************/
int libvlc_audio_equalizer_set_preamp( libvlc_equalizer_t *p_equalizer, float f_preamp )
{
    if( isnan(f_preamp) )
        return -1;
    if( f_preamp < -20.f )
        f_preamp = -20.f;
    else if( f_preamp > 20.f )
        f_preamp = 20.f;

    p_equalizer->f_preamp = f_preamp;
    return 0;
}

/*****************************************************************************
 * libvlc_audio_equalizer_get_preamp : Get the preamp value for an equalizer
 *****************************************************************************/
float libvlc_audio_equalizer_get_preamp( libvlc_equalizer_t *p_equalizer )
{
    return p_equalizer->f_preamp;
}

/*****************************************************************************
 * libvlc_audio_equalizer_set_amp_at_index : Set the amplification value for an equalizer band
 *****************************************************************************/
int libvlc_audio_equalizer_set_amp_at_index( libvlc_equalizer_t *p_equalizer, float f_amp, unsigned u_band )
{
    if( u_band >= EQZ_BANDS_MAX || isnan(f_amp) )
        return -1;


    if( f_amp < -20.f )
        f_amp = -20.f;
    else if( f_amp > 20.f )
        f_amp = 20.f;

    p_equalizer->f_amp[ u_band ] = f_amp;
    return 0;
}

/*****************************************************************************
 * libvlc_audio_equalizer_get_amp_at_index : Get the amplification value for an equalizer band
 *****************************************************************************/
float libvlc_audio_equalizer_get_amp_at_index( libvlc_equalizer_t *p_equalizer, unsigned u_band )
{
    if ( u_band >= EQZ_BANDS_MAX )
        return nanf("");

    return p_equalizer->f_amp[ u_band ];
}
