/*****************************************************************************
 * audio_video.c: Audio/Video management : volume, snapshot, OSD
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

#include <vlc/mediacontrol.h>
#include <vlc/libvlc.h>

#include <vlc_playlist.h>

#include <vlc_aout.h>

#include <vlc_vout.h>
#include <vlc_osd.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>
#endif

mediacontrol_RGBPicture *
mediacontrol_snapshot( mediacontrol_Instance *self,
                       const mediacontrol_Position * a_position,
                       mediacontrol_Exception *exception )
{
    vlc_object_t* p_cache;
    vout_thread_t* p_vout;
    mediacontrol_RGBPicture *p_pic = NULL;
    char path[256];
    snapshot_t *p_snapshot;

    mediacontrol_exception_init( exception );

    p_vout = vlc_object_find( self->p_playlist, VLC_OBJECT_VOUT, FIND_CHILD );
    if( ! p_vout )
    {
        RAISE_NULL( mediacontrol_InternalException, "No video output" );
    }
    p_cache = vlc_object_create( self->p_playlist, VLC_OBJECT_GENERIC );
    if( p_cache == NULL )
    {
        vlc_object_release( p_vout );
        RAISE_NULL( mediacontrol_InternalException, "Out of memory" );
    }
    snprintf( path, 255, "object:%d", p_cache->i_object_id );
    var_SetString( p_vout, "snapshot-path", path );
    var_SetString( p_vout, "snapshot-format", "png" );

    vlc_object_lock( p_cache );
    vout_Control( p_vout, VOUT_SNAPSHOT );
    vlc_object_wait( p_cache );
    vlc_object_release( p_vout );

    p_snapshot = ( snapshot_t* ) p_cache->p_private;
    vlc_object_unlock( p_cache );
    vlc_object_release( p_cache );

    if( p_snapshot )
    {
        p_pic = private_mediacontrol_createRGBPicture( p_snapshot->i_width,
                               p_snapshot->i_height,
                               VLC_FOURCC( 'p','n','g',' ' ),
                               p_snapshot->date,
                               p_snapshot->p_data,
                               p_snapshot->i_datasize );
        if( !p_pic )
        {
            free( p_snapshot->p_data );
            free( p_snapshot );
            RAISE_NULL( mediacontrol_InternalException, "Out of memory" );
        }
    }
    else
    {
        RAISE_NULL( mediacontrol_InternalException, "Snapshot exception" );
    }
    return p_pic;
}

mediacontrol_RGBPicture **
mediacontrol_all_snapshots( mediacontrol_Instance *self,
                            mediacontrol_Exception *exception )
{
    mediacontrol_exception_init( exception );

    RAISE_NULL( mediacontrol_InternalException, "unsupported method" );
}

static
int mediacontrol_showtext( vout_thread_t *p_vout, int i_channel,
                           char *psz_string, text_style_t *p_style,
                           int i_flags, int i_hmargin, int i_vmargin,
                           mtime_t i_start, mtime_t i_stop )
{
    subpicture_t *p_spu;
    video_format_t fmt;

    if( !psz_string ) return VLC_EGENERIC;

    p_spu = spu_CreateSubpicture( p_vout->p_spu );
    if( !p_spu ) return VLC_EGENERIC;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('T','E','X','T');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_spu->p_region = p_spu->pf_create_region( VLC_OBJECT(p_vout), &fmt );
    if( !p_spu->p_region )
    {
        msg_Err( p_vout, "cannot allocate SPU region" );
        spu_DestroySubpicture( p_vout->p_spu, p_spu );
        return VLC_EGENERIC;
    }

    p_spu->p_region->psz_text = strdup( psz_string );
    p_spu->p_region->i_align = i_flags & SUBPICTURE_ALIGN_MASK;
    p_spu->i_start = i_start;
    p_spu->i_stop = i_stop;
    p_spu->b_ephemer = VLC_FALSE;
    p_spu->b_absolute = VLC_FALSE;

    p_spu->i_x = i_hmargin;
    p_spu->i_y = i_vmargin;
    p_spu->i_flags = i_flags & ~SUBPICTURE_ALIGN_MASK;
    p_spu->i_channel = i_channel;

    spu_DisplaySubpicture( p_vout->p_spu, p_spu );

    return VLC_SUCCESS;
}


void
mediacontrol_display_text( mediacontrol_Instance *self,
                           const char * message,
                           const mediacontrol_Position * begin,
                           const mediacontrol_Position * end,
                           mediacontrol_Exception *exception )
{
    input_thread_t *p_input = NULL;
    vout_thread_t *p_vout = NULL;
    char* psz_message;

    psz_message = strdup( message );
    if( !psz_message )
    {
        RAISE_VOID( mediacontrol_InternalException, "no more memory" );
    }

    p_vout = vlc_object_find( self->p_playlist, VLC_OBJECT_VOUT, FIND_CHILD );
    if( ! p_vout )
    {
        RAISE_VOID( mediacontrol_InternalException, "no video output" );
    }

    if( begin->origin == mediacontrol_RelativePosition &&
        begin->value == 0 &&
        end->origin == mediacontrol_RelativePosition )
    {
        mtime_t i_duration = 0;
        mtime_t i_now = mdate();

        i_duration = 1000 * private_mediacontrol_unit_convert(
                                                self->p_playlist->p_input,
                                                end->key,
                                                mediacontrol_MediaTime,
                                                end->value );

        mediacontrol_showtext( p_vout, DEFAULT_CHAN, psz_message, NULL,
                               OSD_ALIGN_BOTTOM | OSD_ALIGN_LEFT, 0, 0,
                               i_now, i_now + i_duration );
    }
    else
    {
        mtime_t i_debut, i_fin, i_now;

        p_input = self->p_playlist->p_input;
        if( ! p_input )
        {
            vlc_object_release( p_vout );
            RAISE_VOID( mediacontrol_InternalException, "No input" );
        }

        /* FIXME */
        /* i_now = input_ClockGetTS( p_input, NULL, 0 ); */
        i_now = mdate();

        i_debut = private_mediacontrol_position2microsecond( p_input,
                                            ( mediacontrol_Position* ) begin );
        i_debut += i_now;

        i_fin = private_mediacontrol_position2microsecond( p_input,
                                          ( mediacontrol_Position * ) end );
        i_fin += i_now;

        vout_ShowTextAbsolute( p_vout, DEFAULT_CHAN, psz_message, NULL,
                               OSD_ALIGN_BOTTOM | OSD_ALIGN_LEFT, 0, 0,
                               i_debut, i_fin );
    }

    vlc_object_release( p_vout );
}

unsigned short
mediacontrol_sound_get_volume( mediacontrol_Instance *self,
                               mediacontrol_Exception *exception )
{
    libvlc_exception_t ex;
    int i_ret = 0;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    i_ret = libvlc_audio_get_volume( self->p_instance, &ex );
    HANDLE_LIBVLC_EXCEPTION_ZERO( &ex );
    /* FIXME: Normalize in [0..100] */
    return (unsigned short)i_ret;
}

void
mediacontrol_sound_set_volume( mediacontrol_Instance *self,
                               const unsigned short volume,
                               mediacontrol_Exception *exception )
{
    /* FIXME: Normalize in [0..100] */
    libvlc_exception_t ex;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    libvlc_audio_set_volume( self->p_instance, volume, &ex );
    HANDLE_LIBVLC_EXCEPTION_VOID( &ex );
}

vlc_bool_t mediacontrol_set_visual( mediacontrol_Instance *self,
                                    WINDOWHANDLE visual_id,
                                    mediacontrol_Exception *exception )
{
    libvlc_exception_t ex;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    libvlc_video_set_parent( self->p_instance, visual_id, &ex );
    HANDLE_LIBVLC_EXCEPTION_ZERO( &ex );
    return VLC_TRUE;
}

int
mediacontrol_get_rate( mediacontrol_Instance *self,
               mediacontrol_Exception *exception )
{
    libvlc_exception_t ex;
    libvlc_media_instance_t* p_mi;
    int i_ret;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    p_mi = libvlc_playlist_get_media_instance( self->p_instance, &ex );
    HANDLE_LIBVLC_EXCEPTION_ZERO( &ex );

    i_ret = libvlc_media_instance_get_rate( p_mi, &ex );
    libvlc_media_instance_release( p_mi );
    HANDLE_LIBVLC_EXCEPTION_ZERO( &ex );

    return i_ret / 10;
}

void
mediacontrol_set_rate( mediacontrol_Instance *self,
               const int rate,
               mediacontrol_Exception *exception )
{
    libvlc_exception_t ex;
    libvlc_media_instance_t* p_mi;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    p_mi = libvlc_playlist_get_media_instance( self->p_instance, &ex );
    HANDLE_LIBVLC_EXCEPTION_VOID( &ex );

    libvlc_media_instance_set_rate( p_mi, rate * 10, &ex );
    libvlc_media_instance_release( p_mi );
    HANDLE_LIBVLC_EXCEPTION_VOID( &ex );
}

int
mediacontrol_get_fullscreen( mediacontrol_Instance *self,
                 mediacontrol_Exception *exception )
{
    libvlc_exception_t ex;
    libvlc_media_instance_t* p_mi;
    int i_ret;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    p_mi = libvlc_playlist_get_media_instance( self->p_instance, &ex );
    HANDLE_LIBVLC_EXCEPTION_ZERO( &ex );

    i_ret = libvlc_get_fullscreen( p_mi, &ex );
    libvlc_media_instance_release( p_mi );
    HANDLE_LIBVLC_EXCEPTION_ZERO( &ex );

    return i_ret;
}

void
mediacontrol_set_fullscreen( mediacontrol_Instance *self,
                 const int b_fullscreen,
                 mediacontrol_Exception *exception )
{
    libvlc_exception_t ex;
    libvlc_media_instance_t* p_mi;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    p_mi = libvlc_playlist_get_media_instance( self->p_instance, &ex );
    HANDLE_LIBVLC_EXCEPTION_VOID( &ex );

    libvlc_set_fullscreen( p_mi, b_fullscreen, &ex );
    libvlc_media_instance_release( p_mi );
    HANDLE_LIBVLC_EXCEPTION_VOID( &ex );
}
