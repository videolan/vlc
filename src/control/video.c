/*****************************************************************************
 * video.c: libvlc new API video functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * $Id: core.c 14187 2006-02-07 16:37:40Z courmisch $
 *
 * Authors: Cl�ent Stenac <zorglub@videolan.org>
 *          Filippo Carone <littlejohn@videolan.org>
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
#include <vlc_vout.h>

/*
 * Remember to release the returned input_thread_t since it is locked at
 * the end of this function.
 */
static input_thread_t *GetInputThread( libvlc_input_t *p_input,
                               libvlc_exception_t *p_exception )
{
    input_thread_t *p_input_thread;

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

    return p_input_thread;;
}

/*
 * Remember to release the returned vout_thread_t since it is locked at
 * the end of this function.
 */
static vout_thread_t *GetVout( libvlc_input_t *p_input,
                               libvlc_exception_t *p_exception )
{
    input_thread_t *p_input_thread = GetInputThread(p_input, p_exception);
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

void libvlc_set_fullscreen( libvlc_input_t *p_input, int b_fullscreen,
                            libvlc_exception_t *p_e )
{
    /* We only work on the first vout */
    vout_thread_t *p_vout1 = GetVout( p_input, p_e );
    vlc_value_t val; int i_ret;

    /* GetVout will raise the exception for us */
    if( !p_vout1 )
    {
        return;
    }

    if( b_fullscreen ) val.b_bool = VLC_TRUE;
    else               val.b_bool = VLC_FALSE;

    i_ret = var_Set( p_vout1, "fullscreen", val );
    if( i_ret )
        libvlc_exception_raise( p_e,
                        "Unexpected error while setting fullscreen value" );

    vlc_object_release( p_vout1 );
}

int libvlc_get_fullscreen( libvlc_input_t *p_input,
                            libvlc_exception_t *p_e )
{
    /* We only work on the first vout */
    vout_thread_t *p_vout1 = GetVout( p_input, p_e );
    vlc_value_t val; int i_ret;

    /* GetVout will raise the exception for us */
    if( !p_vout1 )
        return 0;

    i_ret = var_Get( p_vout1, "fullscreen", &val );
    if( i_ret )
        libvlc_exception_raise( p_e,
                        "Unexpected error while looking up fullscreen value" );

    return val.b_bool == VLC_TRUE ? 1 : 0;
}

void libvlc_toggle_fullscreen( libvlc_input_t *p_input,
                               libvlc_exception_t *p_e )
{
    /* We only work on the first vout */
    vout_thread_t *p_vout1 = GetVout( p_input, p_e );
    vlc_value_t val; int i_ret;

    /* GetVout will raise the exception for us */
    if( !p_vout1 )
        return;

    i_ret = var_Get( p_vout1, "fullscreen", &val );
    if( i_ret )
        libvlc_exception_raise( p_e,
                        "Unexpected error while looking up fullscreen value" );

    val.b_bool = !val.b_bool;
    i_ret = var_Set( p_vout1, "fullscreen", val );
    if( i_ret )
        libvlc_exception_raise( p_e,
                        "Unexpected error while setting fullscreen value" );

    vlc_object_release( p_vout1 );
}

void
libvlc_video_take_snapshot( libvlc_input_t *p_input, char *psz_filepath,
                       libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_input, p_e );
    input_thread_t *p_input_thread;

    char path[256];

    /* GetVout will raise the exception for us */
    if( !p_vout )
    {
        return;
    }

    p_input_thread = (input_thread_t*)vlc_object_get(
                                 p_input->p_instance->p_libvlc_int,
                                 p_input->i_input_id );
    if( !p_input_thread )
    {
        libvlc_exception_raise( p_e, "Input does not exist" );
        return;
    }

    snprintf( path, 255, "%s", psz_filepath );
    var_SetString( p_vout, "snapshot-path", path );
    var_SetString( p_vout, "snapshot-format", "png" );

    vout_Control( p_vout, VOUT_SNAPSHOT );
    vlc_object_release( p_vout );
    vlc_object_release( p_input_thread );
}

int libvlc_video_get_height( libvlc_input_t *p_input,
                             libvlc_exception_t *p_e ) 
{
    vout_thread_t *p_vout1 = GetVout( p_input, p_e );
    if( !p_vout1 )
        return 0;

    vlc_object_release( p_vout1 );

    return p_vout1->i_window_height;
}

int libvlc_video_get_width( libvlc_input_t *p_input,
                            libvlc_exception_t *p_e ) 
{
    vout_thread_t *p_vout1 = GetVout( p_input, p_e );
    if( !p_vout1 )
        return 0;

    vlc_object_release( p_vout1 );

    return p_vout1->i_window_width;
}

vlc_bool_t libvlc_input_has_vout( libvlc_input_t *p_input,
                                  libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = GetInputThread(p_input, p_e);
    vlc_bool_t has_vout = VLC_FALSE;

    if( p_input_thread )
    {
        vout_thread_t *p_vout;

        p_vout = vlc_object_find( p_input_thread, VLC_OBJECT_VOUT, FIND_CHILD );
        if( p_vout )
        {
            has_vout = VLC_TRUE;
            vlc_object_release( p_vout );
        }
        vlc_object_release( p_input_thread );
    }
    return has_vout;
}

int libvlc_video_reparent( libvlc_input_t *p_input, libvlc_drawable_t d,
                           libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_input, p_e );

    if( p_vout )
    {
        vout_Control( p_vout , VOUT_REPARENT, d);
        vlc_object_release( p_vout );
    }
    return 0;
}

void libvlc_video_resize( libvlc_input_t *p_input, int width, int height, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_input, p_e );
    if( p_vout )
    {
        vout_Control( p_vout, VOUT_SET_SIZE, width, height );
        vlc_object_release( p_vout );
    }
}

/* global video settings */

void libvlc_video_set_parent( libvlc_instance_t *p_instance, libvlc_drawable_t d,
                              libvlc_exception_t *p_e )
{
    /* set as default for future vout instances */
    var_SetInteger(p_instance->p_libvlc_int, "drawable", (int)d);

    if( libvlc_playlist_isplaying(p_instance, p_e) )
    {
        libvlc_input_t *p_input = libvlc_playlist_get_input(p_instance, p_e);
        if( p_input )
        {
            vout_thread_t *p_vout = GetVout( p_input, p_e );
            if( p_vout )
            {
                /* tell running vout to re-parent */
                vout_Control( p_vout , VOUT_REPARENT, d);
                vlc_object_release( p_vout );
            }
            libvlc_input_free(p_input);
        }
    }
}

libvlc_drawable_t libvlc_video_get_parent( libvlc_instance_t *p_instance, libvlc_exception_t *p_e )
{
    libvlc_drawable_t result;
    
    result = var_GetInteger( p_instance->p_libvlc_int, "drawable" );
    
    return result;
}


void libvlc_video_set_size( libvlc_instance_t *p_instance, int width, int height,
                           libvlc_exception_t *p_e )
{
    /* set as default for future vout instances */
    config_PutInt(p_instance->p_libvlc_int, "width", width);
    config_PutInt(p_instance->p_libvlc_int, "height", height);

    if( libvlc_playlist_isplaying(p_instance, p_e) )
    {
        libvlc_input_t *p_input = libvlc_playlist_get_input(p_instance, p_e);
        if( p_input )
        {
            vout_thread_t *p_vout = GetVout( p_input, p_e );
            if( p_vout )
            {
                /* tell running vout to re-size */
                vout_Control( p_vout , VOUT_SET_SIZE, width, height);
                vlc_object_release( p_vout );
            }
            libvlc_input_free(p_input);
        }
    }
}

void libvlc_video_set_viewport( libvlc_instance_t *p_instance,
                            const libvlc_rectangle_t *view, const libvlc_rectangle_t *clip,
                           libvlc_exception_t *p_e )
{
    if( NULL == view )
    {
        libvlc_exception_raise( p_e, "viewport is NULL" );
    }

    /* if clip is NULL, then use view rectangle as clip */
    if( NULL == clip )
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

    if( libvlc_playlist_isplaying(p_instance, p_e) )
    {
        libvlc_input_t *p_input = libvlc_playlist_get_input(p_instance, p_e);
        if( p_input )
        {
            vout_thread_t *p_vout = GetVout( p_input, p_e );
            if( p_vout )
            {
                /* change viewport for running vout */
                vout_Control( p_vout , VOUT_SET_VIEWPORT,
                                   view->top, view->left, view->bottom, view->right,
                                   clip->top, clip->left, clip->bottom, clip->right );
                vlc_object_release( p_vout );
            }
            libvlc_input_free(p_input);
        }
    }
}

char *libvlc_video_get_aspect_ratio( libvlc_input_t *p_input,
                                     libvlc_exception_t *p_e )
{
    char *psz_aspect = 0;
    vout_thread_t *p_vout = GetVout( p_input, p_e );

    if( !p_vout )
        return 0;

    psz_aspect = var_GetString( p_vout, "aspect-ratio" );
    vlc_object_release( p_vout );
    return psz_aspect;
}

void libvlc_video_set_aspect_ratio( libvlc_input_t *p_input,
                                    char *psz_aspect, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_input, p_e );
    int i_ret = -1;

    if( !p_vout )
        return;

    i_ret = var_SetString( p_vout, "aspect-ratio", psz_aspect );
    if( i_ret )
        libvlc_exception_raise( p_e,
                        "Unexpected error while setting aspect-ratio value" );

    vlc_object_release( p_vout );
}

int libvlc_video_get_spu( libvlc_input_t *p_input,
                          libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = GetInputThread( p_input, p_e );
    vlc_value_t val_list;
    vlc_value_t val;
    int i_spu = -1;
    int i_ret = -1;
    int i;

    if( !p_input_thread )
        return -1;

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

void libvlc_video_set_spu( libvlc_input_t *p_input, int i_spu,
                           libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = GetInputThread( p_input, p_e );
    vlc_value_t val_list;
    int i_ret = -1;
    int i;

    if( !p_input_thread )
        return;

    var_Change( p_input_thread, "spu-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        vlc_value_t val = val_list.p_list->p_values[i];
        if( i_spu == i )
        {
            vlc_value_t new_val;

            new_val.i_int = val.i_int;
            i_ret = var_Set( p_input_thread, "spu-es", new_val );
            if( i_ret < 0 )
            {
                libvlc_exception_raise( p_e, "Setting subtitle value failed" );
            }
            vlc_object_release( p_input_thread );
            return;
        }
    }
    libvlc_exception_raise( p_e, "Subtitle value out of range" );
    vlc_object_release( p_input_thread );
}

char *libvlc_video_get_crop_geometry( libvlc_input_t *p_input,
                                   libvlc_exception_t *p_e )
{
    char *psz_geometry = 0;
    vout_thread_t *p_vout = GetVout( p_input, p_e );

    if( !p_vout )
        return 0;

    psz_geometry = var_GetString( p_vout, "crop" );
    vlc_object_release( p_vout );
    return psz_geometry;
}

void libvlc_video_set_crop_geometry( libvlc_input_t *p_input,
                                    char *psz_geometry, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_input, p_e );
    int i_ret = -1;

    if( !p_vout )
        return;

    i_ret = var_SetString( p_vout, "crop", psz_geometry );
    if( i_ret )
        libvlc_exception_raise( p_e,
                        "Unexpected error while setting crop geometry" );

    vlc_object_release( p_vout );
}

int libvlc_video_destroy( libvlc_input_t *p_input,
                          libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_input, p_e );
    vlc_object_detach( p_vout ); 
    vlc_object_release( p_vout );
    vout_Destroy( p_vout );

    return 0;
}
