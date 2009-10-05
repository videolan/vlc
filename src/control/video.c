/*****************************************************************************
 * video.c: libvlc new API video functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Filippo Carone <littlejohn@videolan.org>
 *          Jean-Paul Saman <jpsaman _at_ m2x _dot_ nl>
 *          Damien Fouilleul <damienf a_t videolan dot org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_player.h>

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_vout.h>

#include "media_player_internal.h"
#include <vlc_osd.h>
#include <assert.h>

/*
 * Remember to release the returned vout_thread_t.
 */
static vout_thread_t *GetVout( libvlc_media_player_t *p_mi,
                               libvlc_exception_t *p_exception )
{
    input_thread_t *p_input = libvlc_get_input_thread( p_mi, p_exception );
    vout_thread_t *p_vout = NULL;

    if( p_input )
    {
        p_vout = input_GetVout( p_input );
        if( !p_vout )
        {
            libvlc_exception_raise( p_exception );
            libvlc_printerr( "No active video output" );
        }
        vlc_object_release( p_input );
    }
    return p_vout;
}

/**********************************************************************
 * Exported functions
 **********************************************************************/

void libvlc_set_fullscreen( libvlc_media_player_t *p_mi, int b_fullscreen,
                            libvlc_exception_t *p_e )
{
    /* We only work on the first vout */
    vout_thread_t *p_vout = GetVout( p_mi, p_e );

    /* GetVout will raise the exception for us */
    if( !p_vout ) return;

    var_SetBool( p_vout, "fullscreen", b_fullscreen );

    vlc_object_release( p_vout );
}

int libvlc_get_fullscreen( libvlc_media_player_t *p_mi,
                            libvlc_exception_t *p_e )
{
    /* We only work on the first vout */
    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    int i_ret;

    /* GetVout will raise the exception for us */
    if( !p_vout ) return 0;

    i_ret = var_GetBool( p_vout, "fullscreen" );

    vlc_object_release( p_vout );

    return i_ret;
}

void libvlc_toggle_fullscreen( libvlc_media_player_t *p_mi,
                               libvlc_exception_t *p_e )
{
    /* We only work on the first vout */
    vout_thread_t *p_vout = GetVout( p_mi, p_e );

    /* GetVout will raise the exception for us */
    if( !p_vout ) return;

    var_ToggleBool( p_vout, "fullscreen" );

    vlc_object_release( p_vout );
}

void
libvlc_video_take_snapshot( libvlc_media_player_t *p_mi, const char *psz_filepath,
        unsigned int i_width, unsigned int i_height, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout;

    assert( psz_filepath );

    /* We must have an input */
    if( !p_mi->p_input_thread )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Input does not exist" );
        return;
    }

    /* GetVout will raise the exception for us */
    p_vout = GetVout( p_mi, p_e );
    if( !p_vout ) return;

    var_SetInteger( p_vout, "snapshot-width", i_width );
    var_SetInteger( p_vout, "snapshot-height", i_height );

    var_SetString( p_vout, "snapshot-path", psz_filepath );
    var_SetString( p_vout, "snapshot-format", "png" );

    var_TriggerCallback( p_vout, "video-snapshot" );
    vlc_object_release( p_vout );
}

int libvlc_video_get_height( libvlc_media_player_t *p_mi,
                             libvlc_exception_t *p_e )
{
    int height;

    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    if( !p_vout ) return 0;

    height = p_vout->i_window_height;

    vlc_object_release( p_vout );

    return height;
}

int libvlc_video_get_width( libvlc_media_player_t *p_mi,
                            libvlc_exception_t *p_e )
{
    int width;

    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    if( !p_vout ) return 0;

    width = p_vout->i_window_width;

    vlc_object_release( p_vout );

    return width;
}

int libvlc_media_player_has_vout( libvlc_media_player_t *p_mi,
                                     libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread(p_mi, p_e);
    bool has_vout = false;

    if( p_input_thread )
    {
        vout_thread_t *p_vout;

        p_vout = input_GetVout( p_input_thread );
        if( p_vout )
        {
            has_vout = true;
            vlc_object_release( p_vout );
        }
        vlc_object_release( p_input_thread );
    }
    return has_vout;
}

float libvlc_video_get_scale( libvlc_media_player_t *p_mp,
                              libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mp, p_e );
    if( !p_vout )
        return 0.;

    float f_scale = var_GetFloat( p_vout, "scale" );
    if( var_GetBool( p_vout, "autoscale" ) )
        f_scale = 0.;
    vlc_object_release( p_vout );
    return f_scale;
}

void libvlc_video_set_scale( libvlc_media_player_t *p_mp, float f_scale,
                             libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mp, p_e );
    if( !p_vout )
        return;

    if( f_scale != 0. )
        var_SetFloat( p_vout, "scale", f_scale );
    var_SetBool( p_vout, "autoscale", f_scale != 0. );
    vlc_object_release( p_vout );
}

char *libvlc_video_get_aspect_ratio( libvlc_media_player_t *p_mi,
                                     libvlc_exception_t *p_e )
{
    char *psz_aspect = NULL;
    vout_thread_t *p_vout = GetVout( p_mi, p_e );

    if( !p_vout ) return NULL;

    psz_aspect = var_GetNonEmptyString( p_vout, "aspect-ratio" );
    vlc_object_release( p_vout );
    return psz_aspect ? psz_aspect : strdup("");
}

void libvlc_video_set_aspect_ratio( libvlc_media_player_t *p_mi,
                                    const char *psz_aspect, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    int i_ret = -1;

    if( !p_vout ) return;

    i_ret = var_SetString( p_vout, "aspect-ratio", psz_aspect );
    vlc_object_release( p_vout );
    if( i_ret )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Bad or unsupported aspect ratio" );
    }
}

int libvlc_video_get_spu( libvlc_media_player_t *p_mi,
                          libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi, p_e );
    vlc_value_t val_list;
    vlc_value_t val;
    int i_spu = -1;
    int i_ret = -1;
    int i;

    if( !p_input_thread ) return -1;

    i_ret = var_Get( p_input_thread, "spu-es", &val );
    if( i_ret < 0 )
    {
        vlc_object_release( p_input_thread );
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Subtitle informations not found" );
        return i_ret;
    }

    var_Change( p_input_thread, "spu-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        if( val.i_int == val_list.p_list->p_values[i].i_int )
        {
            i_spu = i;
            break;
        }
    }
    var_FreeList( &val_list, NULL );
    vlc_object_release( p_input_thread );
    return i_spu;
}

int libvlc_video_get_spu_count( libvlc_media_player_t *p_mi,
                                libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi, p_e );
    int i_spu_count;

    if( !p_input_thread )
        return -1;

    i_spu_count = var_CountChoices( p_input_thread, "spu-es" );

    vlc_object_release( p_input_thread );
    return i_spu_count;
}

libvlc_track_description_t *
        libvlc_video_get_spu_description( libvlc_media_player_t *p_mi,
                                          libvlc_exception_t *p_e )
{
    return libvlc_get_track_description( p_mi, "spu-es", p_e);
}

void libvlc_video_set_spu( libvlc_media_player_t *p_mi, int i_spu,
                           libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi, p_e );
    vlc_value_t val_list;
    vlc_value_t newval;
    int i_ret = -1;

    if( !p_input_thread ) return;

    var_Change( p_input_thread, "spu-es", VLC_VAR_GETCHOICES, &val_list, NULL );

    if( ( val_list.p_list->i_count == 0 )
     || (i_spu < 0) || (i_spu > val_list.p_list->i_count) )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Subtitle number out of range" );
        goto end;
    }

    newval = val_list.p_list->p_values[i_spu];
    i_ret = var_Set( p_input_thread, "spu-es", newval );
    if( i_ret < 0 )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Subtitle selection error" );
    }

end:
    var_FreeList( &val_list, NULL );
    vlc_object_release( p_input_thread );
}

int libvlc_video_set_subtitle_file( libvlc_media_player_t *p_mi,
                                    const char *psz_subtitle,
                                    libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    bool b_ret = false;

    if( p_input_thread )
    {
        if( !input_AddSubtitle( p_input_thread, psz_subtitle, true ) )
            b_ret = true;
        vlc_object_release( p_input_thread );
    }
    return b_ret;
}

libvlc_track_description_t *
        libvlc_video_get_title_description( libvlc_media_player_t *p_mi,
                                            libvlc_exception_t * p_e )
{
    return libvlc_get_track_description( p_mi, "title", p_e);
}

libvlc_track_description_t *
        libvlc_video_get_chapter_description( libvlc_media_player_t *p_mi,
                                              int i_title,
                                              libvlc_exception_t *p_e )
{
    char psz_title[12];
    sprintf( psz_title,  "title %2i", i_title );
    return libvlc_get_track_description( p_mi, psz_title, p_e);
}

char *libvlc_video_get_crop_geometry( libvlc_media_player_t *p_mi,
                                   libvlc_exception_t *p_e )
{
    char *psz_geometry = 0;
    vout_thread_t *p_vout = GetVout( p_mi, p_e );

    if( !p_vout ) return 0;

    psz_geometry = var_GetNonEmptyString( p_vout, "crop" );
    vlc_object_release( p_vout );
    return psz_geometry ? psz_geometry : strdup("");
}

void libvlc_video_set_crop_geometry( libvlc_media_player_t *p_mi,
                                     const char *psz_geometry, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    int i_ret = -1;

    if( !p_vout ) return;

    i_ret = var_SetString( p_vout, "crop", psz_geometry );
    vlc_object_release( p_vout );

    if( i_ret )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Bad or unsupported cropping geometry" );
    }
}

int libvlc_video_get_teletext( libvlc_media_player_t *p_mi,
                               libvlc_exception_t *p_e )
{
#if 0
    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    vlc_object_t *p_vbi;
    int i_ret = -1;

    if( !p_vout ) return i_ret;

    p_vbi = (vlc_object_t *) vlc_object_find_name( p_vout, "zvbi",
                                                   FIND_CHILD );
    if( p_vbi )
    {
        i_ret = var_GetInteger( p_vbi, "vbi-page" );
        vlc_object_release( p_vbi );
    }

    vlc_object_release( p_vout );
    return i_ret;
#else
    VLC_UNUSED( p_mi );
    VLC_UNUSED( p_e );
    return -1;
#endif
}

void libvlc_video_set_teletext( libvlc_media_player_t *p_mi, int i_page,
                                libvlc_exception_t *p_e )
{
#if 0
    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    vlc_object_t *p_vbi;
    int i_ret = -1;

    if( !p_vout ) return;

    p_vbi = (vlc_object_t *) vlc_object_find_name( p_vout, "zvbi",
                                                   FIND_CHILD );
    if( p_vbi )
    {
        i_ret = var_SetInteger( p_vbi, "vbi-page", i_page );
        vlc_object_release( p_vbi );
        if( i_ret )
            libvlc_exception_raise( p_e,
                            "Unexpected error while setting teletext page" );
    }
    vlc_object_release( p_vout );
#else
    VLC_UNUSED( p_mi );
    VLC_UNUSED( p_e );
    VLC_UNUSED( i_page );
#endif
}

void libvlc_toggle_teletext( libvlc_media_player_t *p_mi,
                             libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread(p_mi, p_e);
    if( !p_input_thread ) return;

    if( var_CountChoices( p_input_thread, "teletext-es" ) <= 0 )
    {
        vlc_object_release( p_input_thread );
        return;
    }
    const bool b_selected = var_GetInteger( p_input_thread, "teletext-es" ) >= 0;
#if 0
    int i_ret;
    vlc_object_t *p_vbi;
    p_vbi = (vlc_object_t *)vlc_object_find_name( p_input_thread, "zvbi",
                                                  FIND_CHILD );
    if( p_vbi )
    {
        if( b_selected )
        {
            /* FIXME Gni, why that ? */
            i_ret = var_SetInteger( p_vbi, "vbi-page",
                                    var_GetInteger( p_vbi, "vbi-page" ) );
            if( i_ret )
                libvlc_exception_raise( p_e,
                                "Unexpected error while setting teletext page" );
        }
        else
        {
            /* FIXME Gni^2 */
            i_ret = var_SetBool( p_vbi, "vbi-opaque",
                                 !var_GetBool( p_vbi, "vbi-opaque" ) );
            if( i_ret )
                libvlc_exception_raise( p_e,
                                "Unexpected error while setting teletext transparency" );
        }
        vlc_object_release( p_vbi );
    }
    else
#endif
    if( b_selected )
    {
        var_SetInteger( p_input_thread, "spu-es", -1 );
    }
    else
    {
        vlc_value_t list;
        if( !var_Change( p_input_thread, "teletext-es", VLC_VAR_GETLIST, &list, NULL ) )
        {
            if( list.p_list->i_count > 0 )
                var_SetInteger( p_input_thread, "spu-es", list.p_list->p_values[0].i_int );

            var_FreeList( &list, NULL );
        }
    }
    vlc_object_release( p_input_thread );
}

int libvlc_video_get_track_count( libvlc_media_player_t *p_mi,
                                  libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi, p_e );
    int i_track_count;

    if( !p_input_thread )
        return -1;

    i_track_count = var_CountChoices( p_input_thread, "video-es" );

    vlc_object_release( p_input_thread );
    return i_track_count;
}

libvlc_track_description_t *
        libvlc_video_get_track_description( libvlc_media_player_t *p_mi,
                                            libvlc_exception_t *p_e )
{
    return libvlc_get_track_description( p_mi, "video-es", p_e);
}

int libvlc_video_get_track( libvlc_media_player_t *p_mi,
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

    i_ret = var_Get( p_input_thread, "video-es", &val );
    if( i_ret < 0 )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Video track information not found" );
        vlc_object_release( p_input_thread );
        return i_ret;
    }

    var_Change( p_input_thread, "video-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        if( val_list.p_list->p_values[i].i_int == val.i_int )
        {
            i_track = i;
            break;
        }
    }
    var_FreeList( &val_list, NULL );
    vlc_object_release( p_input_thread );
    return i_track;
}

void libvlc_video_set_track( libvlc_media_player_t *p_mi, int i_track,
                             libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi, p_e );
    vlc_value_t val_list;
    int i_ret = -1;
    int i;

    if( !p_input_thread )
        return;

    var_Change( p_input_thread, "video-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        if( i_track == val_list.p_list->p_values[i].i_int )
        {
            i_ret = var_SetInteger( p_input_thread, "video-es", i_track );
            if( i_ret < 0 )
                break;
            goto end;
        }
    }
    libvlc_exception_raise( p_e );
    libvlc_printerr( "Video track number out of range" );
end:
    var_FreeList( &val_list, NULL );
    vlc_object_release( p_input_thread );
}

/******************************************************************************
 * libvlc_video_set_deinterlace : enable deinterlace
 *****************************************************************************/
void libvlc_video_set_deinterlace( libvlc_media_player_t *p_mi, int b_enable,
                                   const char *psz_mode,
                                   libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mi, p_e );

    if( !p_vout )
        return;

    if( b_enable )
    {
        /* be sure that the filter name given is supported */
        if( !strcmp(psz_mode, "blend")   || !strcmp(psz_mode, "bob")
         || !strcmp(psz_mode, "discard") || !strcmp(psz_mode, "linear")
         || !strcmp(psz_mode, "mean")    || !strcmp(psz_mode, "x") )
        {
            /* set deinterlace filter chosen */
            var_SetString( p_vout, "deinterlace-mode", psz_mode );
        }
        else
        {
            libvlc_exception_raise( p_e );
            libvlc_printerr( "Bad or unsuported deinterlacing mode" );
        }
    }
    else
    {
        /* disable deinterlace filter */
        var_SetString( p_vout, "deinterlace-mode", "" );
    }

    vlc_object_release( p_vout );
}

/*****************************************************************************
 * Marquee: FIXME: That implementation has no persistent state and requires
 * a vout
 *****************************************************************************/

static const char *get_marquee_int_option_identifier(unsigned option)
{
    static const char tab[][16] =
    {
        "marq",
        "marq-color",
        "marq-opacity",
        "marq-position",
        "marq-refresh",
        "marq-size",
        "marq-timeout",
        "marq-x",
        "marq-y",
    };
    if( option >= sizeof( tab ) / sizeof( tab[0] ) )
        return NULL;
    return tab[option];
}

static const char *get_marquee_string_option_identifier(unsigned option)
{
    static const char tab[][16] =
    {
        "marq-marquee",
    };
    if( option >= sizeof( tab ) / sizeof( tab[0] ) )
        return NULL;
    return tab[option];
}


static vlc_object_t *get_marquee_object( libvlc_media_player_t * p_mi )
{
    libvlc_exception_t e;
    libvlc_exception_init(&e);
    vout_thread_t * vout = GetVout( p_mi, &e );
    libvlc_exception_clear(&e);
    if( !vout )
        return NULL;
    vlc_object_t * object = vlc_object_find_name( vout, "marq", FIND_CHILD );
    vlc_object_release(vout);
    return object;
}

/*****************************************************************************
 * libvlc_video_get_marquee_option_as_int : get a marq option value
 *****************************************************************************/
int libvlc_video_get_marquee_option_as_int( libvlc_media_player_t *p_mi,
                                            libvlc_video_marquee_int_option_t option,
                                            libvlc_exception_t *p_e )
{
    const char * identifier = get_marquee_int_option_identifier(option);
    if(!identifier)
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Unknown marquee option" );
        return 0;
    }
    vlc_object_t * marquee = get_marquee_object(p_mi);

    /* Handle the libvlc_marquee_Enabled separately */
    if(option == libvlc_marquee_Enabled)
    {
        bool isEnabled = marquee != NULL;
        vlc_object_release(marquee);
        return isEnabled;
    }

    /* Generic case */
    if(!identifier)
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Marquee not enabled" );
        return 0;
    }
#warning This and the next function may crash due to type checking!
    int ret = var_GetInteger(marquee, identifier);
    vlc_object_release(marquee);
    return ret;
}

/*****************************************************************************
 * libvlc_video_get_marquee_option_as_string : get a marq option value
 *****************************************************************************/
char * libvlc_video_get_marquee_option_as_string( libvlc_media_player_t *p_mi,
                                                  libvlc_video_marquee_string_option_t option,
                                                  libvlc_exception_t *p_e )
{
    const char * identifier = get_marquee_string_option_identifier(option);
    if(!identifier)
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Unknown marquee option" );
        return NULL;
    }

    vlc_object_t * marquee = get_marquee_object(p_mi);
    if(!marquee)
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Marquee not enabled" );
        return NULL;
    }
    char *ret = var_GetString(marquee, identifier);
    vlc_object_release(marquee);
    return ret;
}

/*****************************************************************************
 * libvlc_video_set_marquee_option_as_int: enable, disable or set an int option
 *****************************************************************************/
void libvlc_video_set_marquee_option_as_int( libvlc_media_player_t *p_mi,
                                          libvlc_video_marquee_int_option_t option,
                                          int value, libvlc_exception_t *p_e )
{
    const char * identifier = get_marquee_int_option_identifier(option);
    if(!identifier)
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Unknown marquee option" );
        return;
    }

    /* Handle the libvlc_marquee_Enabled separately */
    if(option == libvlc_marquee_Enabled)
    {
        libvlc_exception_t e;
        libvlc_exception_init(&e);
        vout_thread_t * vout = GetVout( p_mi, &e );
        libvlc_exception_clear(&e);
        if (vout)
        {
            vout_EnableFilter(vout, identifier, value, false);
            vlc_object_release(vout);
        }
        return;
    }

    vlc_object_t * marquee = get_marquee_object(p_mi);
    if(!marquee)
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Marquee not enabled" );
        return;
    }
    var_SetInteger(marquee, identifier, value);
    vlc_object_release(marquee);
}

/*****************************************************************************
 * libvlc_video_set_marquee_option_as_string: set a string option
 *****************************************************************************/
void libvlc_video_set_marquee_option_as_string( libvlc_media_player_t *p_mi,
                                             libvlc_video_marquee_string_option_t option,
                                             const char * value,
                                             libvlc_exception_t *p_e )
{
    const char * identifier = get_marquee_string_option_identifier(option);
    if(!identifier)
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Unknown marquee option" );
        return;
    }
    vlc_object_t * marquee = get_marquee_object(p_mi);
    if(!marquee)
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Marquee not enabled" );
        return;
    }
    var_SetString(marquee, identifier, value);
    vlc_object_release(marquee);
}
