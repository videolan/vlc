/*****************************************************************************
 * mediacontrol_audio_video.c: Audio/Video management : volume, snapshot, OSD
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Aubert <olivier.aubert@liris.univ-lyon1.fr>
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
#include "mediacontrol_internal.h"
#include "libvlc_internal.h"
#include "media_player_internal.h"

#include <vlc/mediacontrol.h>
#include <vlc/libvlc.h>

#include <vlc_vout.h>
#include <vlc_input.h>
#include <vlc_osd.h>
#include <vlc_block.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <stdio.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <sys/types.h>

mediacontrol_RGBPicture *
mediacontrol_snapshot( mediacontrol_Instance *self,
                       const mediacontrol_Position * a_position,
                       mediacontrol_Exception *exception )
{
    (void)a_position;
    vout_thread_t* p_vout;
    input_thread_t *p_input;
    mediacontrol_RGBPicture *p_pic;
    libvlc_exception_t ex;

    libvlc_exception_init( &ex );
    mediacontrol_exception_init( exception );

    p_input = libvlc_get_input_thread( self->p_media_player );
    if( ! p_input )
    {
        RAISE_NULL( mediacontrol_InternalException, "No input" );
    }

    p_vout = input_GetVout( p_input );
    vlc_object_release( p_input );
    if( ! p_vout )
    {
        RAISE_NULL( mediacontrol_InternalException, "No video output" );
    }

    block_t *p_image;
    video_format_t fmt;

    if( vout_GetSnapshot( p_vout, &p_image, NULL, &fmt, "png", 500*1000 ) )
    {
        vlc_object_release( p_vout );
        RAISE_NULL( mediacontrol_InternalException, "Snapshot exception" );
        return NULL;
    }

    /* */
    char *p_data = malloc( p_image->i_buffer );
    if( p_data )
    {
        memcpy( p_data, p_image->p_buffer, p_image->i_buffer );
        p_pic = private_mediacontrol_createRGBPicture( fmt.i_width,
                                                       fmt.i_height,
                                                       fmt.i_chroma,
                                                       p_image->i_pts,
                                                       p_data,
                                                       p_image->i_buffer );
    }
    else
    {
        p_pic = NULL;
    }
    block_Release( p_image );

    if( !p_pic )
        RAISE_NULL( mediacontrol_InternalException, "Out of memory" );

    vlc_object_release( p_vout );
    return p_pic;
}

static
int mediacontrol_showtext( vout_thread_t *p_vout, int i_channel,
                           const char *psz_string, text_style_t *p_style,
                           int i_flags, int i_hmargin, int i_vmargin,
                           mtime_t i_start, mtime_t i_stop )
{
    (void)p_style; (void)i_hmargin; (void)i_vmargin;
    vout_OSDText( p_vout, i_channel, i_flags & ~SUBPICTURE_ALIGN_MASK,
                  i_stop - i_start, psz_string );
}


void
mediacontrol_display_text( mediacontrol_Instance *self,
                           const char * message,
                           const mediacontrol_Position * begin,
                           const mediacontrol_Position * end,
                           mediacontrol_Exception *exception )
{
    vout_thread_t *p_vout = NULL;
    input_thread_t *p_input;
    libvlc_exception_t ex;

    libvlc_exception_init( &ex );
    mediacontrol_exception_init( exception );

    if( !message )
    {
        RAISE_VOID( mediacontrol_InternalException, "Empty text" );
    }

    p_input = libvlc_get_input_thread( self->p_media_player );
    if( ! p_input )
    {
        RAISE_VOID( mediacontrol_InternalException, "No input" );
    }
    p_vout = input_GetVout( p_input );
    /*FIXME: take care of the next fixme that can use p_input */
    vlc_object_release( p_input );

    if( ! p_vout )
    {
        RAISE_VOID( mediacontrol_InternalException, "No video output" );
    }

    if( begin->origin == mediacontrol_RelativePosition &&
        begin->value == 0 &&
        end->origin == mediacontrol_RelativePosition )
    {
        mtime_t i_duration = 0;
        mtime_t i_now = mdate();

        i_duration = 1000 * private_mediacontrol_unit_convert(
                                                              self->p_media_player,
                                                              end->key,
                                                              mediacontrol_MediaTime,
                                                              end->value );

        mediacontrol_showtext( p_vout, DEFAULT_CHAN, message, NULL,
                               OSD_ALIGN_BOTTOM | OSD_ALIGN_LEFT, 0, 0,
                               i_now, i_now + i_duration );
    }
    else
    {
        mtime_t i_debut, i_fin, i_now;

        /* FIXME */
        /* i_now = input_ClockGetTS( p_input, NULL, 0 ); */
        i_now = mdate();

        i_debut = private_mediacontrol_position2microsecond( self->p_media_player,
                                            ( mediacontrol_Position* ) begin );
        i_debut += i_now;

        i_fin = private_mediacontrol_position2microsecond( self->p_media_player,
                                          ( mediacontrol_Position * ) end );
        i_fin += i_now;

        vout_ShowTextAbsolute( p_vout, DEFAULT_CHAN, message, NULL,
                               OSD_ALIGN_BOTTOM | OSD_ALIGN_LEFT, 0, 0,
                               i_debut, i_fin );
    }

    vlc_object_release( p_vout );
}

unsigned short
mediacontrol_sound_get_volume( mediacontrol_Instance *self,
                               mediacontrol_Exception *exception )
{
    int i_ret = 0;

    mediacontrol_exception_init( exception );

    //i_ret = libvlc_audio_get_volume( self->p_instance );
#warning FIXME: unimplented
    /* FIXME: Normalize in [0..100] */
    return (unsigned short)i_ret;
}

void
mediacontrol_sound_set_volume( mediacontrol_Instance *self,
                               const unsigned short volume,
                               mediacontrol_Exception *exception )
{
    /* FIXME: Normalize in [0..100] */
    mediacontrol_exception_init( exception );

    //libvlc_audio_set_volume( self->p_instance, volume );
#warning FIXME: unimplented
}

int mediacontrol_set_visual( mediacontrol_Instance *self,
                                    WINDOWHANDLE visual_id,
                                    mediacontrol_Exception *exception )
{
    mediacontrol_exception_init( exception );
#ifdef WIN32
    libvlc_media_player_set_hwnd( self->p_media_player, visual_id );
#else
    libvlc_media_player_set_xwindow( self->p_media_player, visual_id );
#endif
    return true;
}

int
mediacontrol_get_rate( mediacontrol_Instance *self,
               mediacontrol_Exception *exception )
{
    int i_ret;

    mediacontrol_exception_init( exception );
    i_ret = libvlc_media_player_get_rate( self->p_media_player );

    return (i_ret >= 0) ? (i_ret / 10) : -1;
}

void
mediacontrol_set_rate( mediacontrol_Instance *self,
               const int rate,
               mediacontrol_Exception *exception )
{
    mediacontrol_exception_init( exception );

    libvlc_media_player_set_rate( self->p_media_player, rate * 10 );
}

int
mediacontrol_get_fullscreen( mediacontrol_Instance *self,
                 mediacontrol_Exception *exception )
{
    libvlc_exception_t ex;
    int i_ret;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    i_ret = libvlc_get_fullscreen( self->p_media_player, &ex );
    HANDLE_LIBVLC_EXCEPTION_ZERO( &ex );

    return i_ret;
}

void
mediacontrol_set_fullscreen( mediacontrol_Instance *self,
                 const int b_fullscreen,
                 mediacontrol_Exception *exception )
{
    libvlc_exception_t ex;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    libvlc_set_fullscreen( self->p_media_player, b_fullscreen, &ex );
    HANDLE_LIBVLC_EXCEPTION_VOID( &ex );
}
