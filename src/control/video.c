/*****************************************************************************
 * video.c: libvlc new API video functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * $Id: core.c 14187 2006-02-07 16:37:40Z courmisch $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Filippo Carone <littlejohn@videolan.org>
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

#include <vlc/vout.h>
#include <vlc/intf.h>

/*
 * Remember to release the returned vout_thread_t since it is locked at
 * the end of this function.
 */
static vout_thread_t *GetVout( libvlc_input_t *p_input,
                               libvlc_exception_t *p_exception )
{
    input_thread_t *p_input_thread;
    vout_thread_t *p_vout;

    if( !p_input )
    {
        libvlc_exception_raise( p_exception, "Input is NULL" );
        return NULL;
    }

    p_input_thread = (input_thread_t*)vlc_object_get(
                                 p_input->p_instance->p_vlc,
                                 p_input->i_input_id );
    if( !p_input_thread )
    {
        libvlc_exception_raise( p_exception, "Input does not exist" );
        return NULL;
    }

    p_vout = vlc_object_find( p_input_thread, VLC_OBJECT_VOUT, FIND_CHILD );
    if( !p_vout )
    {
        vlc_object_release( p_input_thread );
        libvlc_exception_raise( p_exception, "No active video output" );
        return NULL;
    }
    vlc_object_release( p_input_thread );
    
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
                                 p_input->p_instance->p_vlc,
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

    return;
    
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
    vout_thread_t *p_vout = GetVout( p_input, p_e );

    /* GetVout will raise the exception for us */
    if( !p_vout )
    {
        return VLC_FALSE;
    }

    vlc_object_release( p_vout );
    
    return VLC_TRUE;
}


int libvlc_video_reparent( libvlc_input_t *p_input, libvlc_drawable_t d,
                           libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_input, p_e );
    vout_Control( p_vout , VOUT_REPARENT, d);
    vlc_object_release( p_vout );
    
    return 0;
    
}

void libvlc_video_resize( libvlc_input_t *p_input, int width, int height, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_input, p_e );
    vout_Control( p_vout, VOUT_SET_SIZE, width, height );
    vlc_object_release( p_vout );
}

/* global video settings */

void libvlc_video_set_parent( libvlc_instance_t *p_instance, libvlc_drawable_t d,
                           libvlc_exception_t *p_e )
{
    /* set as default for future vout instances */
    var_SetInteger(p_instance->p_vlc, "drawable", (int)d);

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

void libvlc_video_set_size( libvlc_instance_t *p_instance, int width, int height,
                           libvlc_exception_t *p_e )
{
    /* set as default for future vout instances */
    config_PutInt(p_instance->p_vlc, "width", width);
    config_PutInt(p_instance->p_vlc, "height", height);

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
    var_SetInteger( p_instance->p_vlc, "drawable-view-top", view->top );
    var_SetInteger( p_instance->p_vlc, "drawable-view-left", view->left );
    var_SetInteger( p_instance->p_vlc, "drawable-view-bottom", view->bottom );
    var_SetInteger( p_instance->p_vlc, "drawable-view-right", view->right );
    var_SetInteger( p_instance->p_vlc, "drawable-clip-top", clip->top );
    var_SetInteger( p_instance->p_vlc, "drawable-clip-left", clip->left );
    var_SetInteger( p_instance->p_vlc, "drawable-clip-bottom", clip->bottom );
    var_SetInteger( p_instance->p_vlc, "drawable-clip-right", clip->right );

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

int libvlc_video_destroy( libvlc_input_t *p_input,
                          libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_input, p_e );
    vlc_object_detach( p_vout ); 
    vlc_object_release( p_vout );
    vout_Destroy( p_vout );
    
    return 0;
    
}
