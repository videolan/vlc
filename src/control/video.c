/*****************************************************************************
 * video.c: libvlc new API video functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Cl�ent Stenac <zorglub@videolan.org>
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

#include "libvlc_internal.h"

#include <vlc/libvlc.h>
#include <vlc_input.h>
#include <vlc_vout.h>

/*
 * Remember to release the returned vout_thread_t.
 */
static vout_thread_t *GetVout( libvlc_media_player_t *p_mi,
                               libvlc_exception_t *p_exception )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi, p_exception );
    vout_thread_t *p_vout = NULL;

    if( p_input_thread )
    {
        p_vout = vlc_object_find( p_input_thread, VLC_OBJECT_VOUT, FIND_CHILD );
        if( !p_vout )
        {
            libvlc_exception_raise( p_exception, "No active video output" );
        }
        vlc_object_release( p_input_thread );
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
    bool ret;

    /* GetVout will raise the exception for us */
    if( !p_vout ) return;

    ret = var_GetBool( p_vout, "fullscreen" );
    var_SetBool( p_vout, "fullscreen", !ret );

    vlc_object_release( p_vout );
}

void
libvlc_video_take_snapshot( libvlc_media_player_t *p_mi, char *psz_filepath,
        unsigned int i_width, unsigned int i_height, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    input_thread_t *p_input_thread;

    /* GetVout will raise the exception for us */
    if( !p_vout ) return;

    if( !psz_filepath )
    {
        libvlc_exception_raise( p_e, "filepath is null" );
        return;
    }

    var_SetInteger( p_vout, "snapshot-width", i_width );
    var_SetInteger( p_vout, "snapshot-height", i_height );

    p_input_thread = p_mi->p_input_thread;
    if( !p_mi->p_input_thread )
    {
        libvlc_exception_raise( p_e, "Input does not exist" );
        return;
    }

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

        p_vout = vlc_object_find( p_input_thread, VLC_OBJECT_VOUT, FIND_CHILD );
        if( p_vout )
        {
            has_vout = true;
            vlc_object_release( p_vout );
        }
        vlc_object_release( p_input_thread );
    }
    return has_vout;
}

int libvlc_video_reparent( libvlc_media_player_t *p_mi, libvlc_drawable_t d,
                           libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mi, p_e );

    if( p_vout )
    {
        vout_Control( p_vout , VOUT_REPARENT, d);
        vlc_object_release( p_vout );
    }
    return 0;
}

void libvlc_video_resize( libvlc_media_player_t *p_mi, int width, int height, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    if( p_vout )
    {
        vout_Control( p_vout, VOUT_SET_SIZE, width, height );
        vlc_object_release( p_vout );
    }
}

void libvlc_video_redraw_rectangle( libvlc_media_player_t *p_mi,
                           const libvlc_rectangle_t *area,
                           libvlc_exception_t *p_e )
{
#ifdef __APPLE__
    if( (NULL != area)
     && ((area->bottom - area->top) > 0)
     && ((area->right - area->left) > 0) )
    {
        vout_thread_t *p_vout = GetVout( p_mi, p_e );
        if( p_vout )
        {
            /* tell running vout to redraw area */
            vout_Control( p_vout , VOUT_REDRAW_RECT,
                               area->top, area->left, area->bottom, area->right );
            vlc_object_release( p_vout );
        }
    }
#else
    (void) p_mi; (void) area; (void) p_e;
#endif
}

/* global video settings */

/* Deprecated use libvlc_media_player_set_drawable() */
void libvlc_video_set_parent( libvlc_instance_t *p_instance, libvlc_drawable_t d,
                              libvlc_exception_t *p_e )
{
    /* set as default for future vout instances */
#ifdef WIN32
    vlc_value_t val;

    if( sizeof(HWND) > sizeof(libvlc_drawable_t) )
        return; /* BOOM! we told you not to use this function! */
    val.p_address = (void *)(uintptr_t)d;
    var_Set( p_instance->p_libvlc_int, "drawable-hwnd", val );
#else
    var_SetInteger( p_instance->p_libvlc_int, "drawable-xid", d );
#endif

    libvlc_media_player_t *p_mi = libvlc_playlist_get_media_player(p_instance, p_e);
    if( p_mi )
    {
        libvlc_media_player_set_drawable( p_mi, d, p_e );
        libvlc_media_player_release(p_mi);
    }
}

/* Deprecated use libvlc_media_player_get_drawable() */
libvlc_drawable_t libvlc_video_get_parent( libvlc_instance_t *p_instance, libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

#ifdef WIN32
    vlc_value_t val;

    if( sizeof(HWND) > sizeof(libvlc_drawable_t) )
        return 0;
    var_Get( p_instance->p_libvlc_int, "drawable-hwnd", &val );
    return (uintptr_t)val.p_address;
#else
    return var_GetInteger( p_instance->p_libvlc_int, "drawable-xid" );
#endif
}


void libvlc_video_set_size( libvlc_instance_t *p_instance, int width, int height,
                           libvlc_exception_t *p_e )
{
    /* set as default for future vout instances */
    config_PutInt(p_instance->p_libvlc_int, "width", width);
    config_PutInt(p_instance->p_libvlc_int, "height", height);

    libvlc_media_player_t *p_mi = libvlc_playlist_get_media_player(p_instance, p_e);
    if( p_mi )
    {
        vout_thread_t *p_vout = GetVout( p_mi, p_e );
        if( p_vout )
        {
            /* tell running vout to re-size */
            vout_Control( p_vout , VOUT_SET_SIZE, width, height);
            vlc_object_release( p_vout );
        }
        libvlc_media_player_release(p_mi);
    }
}

void libvlc_video_set_viewport( libvlc_instance_t *p_instance,
                            const libvlc_rectangle_t *view, const libvlc_rectangle_t *clip,
                           libvlc_exception_t *p_e )
{
#ifdef __APPLE__
    if( !view )
    {
        libvlc_exception_raise( p_e, "viewport is NULL" );
        return;
    }

    /* if clip is NULL, then use view rectangle as clip */
    if( !clip )
        clip = view;

    /* set as default for future vout instances */
    var_SetInteger( p_instance->p_libvlc_int, "drawable-view-top", view->top );
    var_SetInteger( p_instance->p_libvlc_int, "drawable-view-left", view->left );
    var_SetInteger( p_instance->p_libvlc_int, "drawable-view-bottom", view->bottom );
    var_SetInteger( p_instance->p_libvlc_int, "drawable-view-right", view->right );
    var_SetInteger( p_instance->p_libvlc_int, "drawable-clip-top", clip->top );
    var_SetInteger( p_instance->p_libvlc_int, "drawable-clip-left", clip->left );
    var_SetInteger( p_instance->p_libvlc_int, "drawable-clip-bottom", clip->bottom );
    var_SetInteger( p_instance->p_libvlc_int, "drawable-clip-right", clip->right );

    libvlc_media_player_t *p_mi = libvlc_playlist_get_media_player(p_instance, p_e);
    if( p_mi )
    {
        vout_thread_t *p_vout = GetVout( p_mi, p_e );
        if( p_vout )
        {
            /* change viewport for running vout */
            vout_Control( p_vout , VOUT_SET_VIEWPORT,
                               view->top, view->left, view->bottom, view->right,
                               clip->top, clip->left, clip->bottom, clip->right );
            vlc_object_release( p_vout );
        }
        libvlc_media_player_release(p_mi);
    }
#else
    (void) p_instance; (void) view; (void) clip; (void) p_e;
#endif
}

char *libvlc_video_get_aspect_ratio( libvlc_media_player_t *p_mi,
                                     libvlc_exception_t *p_e )
{
    char *psz_aspect = 0;
    vout_thread_t *p_vout = GetVout( p_mi, p_e );

    if( !p_vout ) return 0;

    psz_aspect = var_GetNonEmptyString( p_vout, "aspect-ratio" );
    vlc_object_release( p_vout );
    return psz_aspect ? psz_aspect : strdup("");
}

void libvlc_video_set_aspect_ratio( libvlc_media_player_t *p_mi,
                                    char *psz_aspect, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    int i_ret = -1;

    if( !p_vout ) return;

    i_ret = var_SetString( p_vout, "aspect-ratio", psz_aspect );
    if( i_ret )
        libvlc_exception_raise( p_e,
                        "Unexpected error while setting aspect-ratio value" );

    vlc_object_release( p_vout );
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
        libvlc_exception_raise( p_e, "Getting subtitle information failed" );
        vlc_object_release( p_input_thread );
        return i_ret;
    }

    var_Change( p_input_thread, "spu-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        vlc_value_t spu_val = val_list.p_list->p_values[i];
        if( val.i_int == spu_val.i_int )
        {
            i_spu = i;
            break;
        }
    }
    vlc_object_release( p_input_thread );
    return i_spu;
}

int libvlc_video_get_spu_count( libvlc_media_player_t *p_mi,
                                libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi, p_e );
    vlc_value_t val_list;

    if( !p_input_thread )
        return -1;

    var_Change( p_input_thread, "spu-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    vlc_object_release( p_input_thread );
    return val_list.p_list->i_count;
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
    if( (i_spu < 0) && (i_spu > val_list.p_list->i_count) )
    {
        libvlc_exception_raise( p_e, "Subtitle value out of range" );
        vlc_object_release( p_input_thread );
        return;
    }

    newval = val_list.p_list->p_values[i_spu];
    i_ret = var_Set( p_input_thread, "spu-es", newval );
    if( i_ret < 0 )
    {
        libvlc_exception_raise( p_e, "Setting subtitle value failed" );
    }
    vlc_object_release( p_input_thread );
}

int libvlc_video_set_subtitle_file( libvlc_media_player_t *p_mi,
                                    char *psz_subtitle,
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
                                    char *psz_geometry, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    int i_ret = -1;

    if( !p_vout ) return;

    i_ret = var_SetString( p_vout, "crop", psz_geometry );
    if( i_ret )
        libvlc_exception_raise( p_e,
                        "Unexpected error while setting crop geometry" );

    vlc_object_release( p_vout );
}

int libvlc_video_get_teletext( libvlc_media_player_t *p_mi,
                               libvlc_exception_t *p_e )
{
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
}

void libvlc_video_set_teletext( libvlc_media_player_t *p_mi, int i_page,
                                libvlc_exception_t *p_e )
{
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
}

void libvlc_toggle_teletext( libvlc_media_player_t *p_mi,
                             libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_object_t *p_vbi;
    int i_ret;

    p_input_thread = libvlc_get_input_thread(p_mi, p_e);
    if( !p_input_thread ) return;

    if( var_CountChoices( p_input_thread, "teletext-es" ) <= 0 )
    {
        vlc_object_release( p_input_thread );
        return;
    }
    const bool b_selected = var_GetInteger( p_input_thread, "teletext-es" ) >= 0;

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
    else if( b_selected )
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

            var_Change( p_input_thread, "teletext-es", VLC_VAR_FREELIST, &list, NULL );
        }
    }
    vlc_object_release( p_input_thread );
}

int libvlc_video_get_track_count( libvlc_media_player_t *p_mi,
                                  libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi, p_e );
    vlc_value_t val_list;

    if( !p_input_thread )
        return -1;

    var_Change( p_input_thread, "video-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    vlc_object_release( p_input_thread );
    return val_list.p_list->i_count;
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
        libvlc_exception_raise( p_e, "Getting Video track information failed" );
        vlc_object_release( p_input_thread );
        return i_ret;
    }

    var_Change( p_input_thread, "video-es", VLC_VAR_GETCHOICES, &val_list, NULL );
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
        vlc_value_t val = val_list.p_list->p_values[i];
        if( i_track == val.i_int )
        {
            i_ret = var_Set( p_input_thread, "audio-es", val );
            if( i_ret < 0 )
                libvlc_exception_raise( p_e, "Setting video track failed" );
            vlc_object_release( p_input_thread );
            return;
        }
    }
    libvlc_exception_raise( p_e, "Video track out of range" );
    vlc_object_release( p_input_thread );
}

int libvlc_video_destroy( libvlc_media_player_t *p_mi,
                          libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    vlc_object_detach( p_vout );
    vlc_object_release( p_vout );
    vlc_object_release( p_vout );

    return 0;
}
