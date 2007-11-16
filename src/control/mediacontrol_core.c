/*****************************************************************************
 * core.c: Core functions : init, playlist, stream management
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

#include <vlc/vlc.h>
#include <vlc/mediacontrol.h>

#include <vlc/libvlc.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>

#include <vlc_vout.h>
#include <vlc_aout.h>
#include <vlc_input.h>
#include <vlc_osd.h>
#include "mediacontrol_internal.h"

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

mediacontrol_Instance* mediacontrol_new( int argc, char** argv, mediacontrol_Exception *exception )
{
    mediacontrol_Instance* retval;
    libvlc_exception_t ex;

    libvlc_exception_init( &ex );
    mediacontrol_exception_init( exception );

    retval = ( mediacontrol_Instance* )malloc( sizeof( mediacontrol_Instance ) );
    if( !retval )
        RAISE_NULL( mediacontrol_InternalException, "Out of memory" );

    retval->p_instance = libvlc_new( argc, argv, &ex );
    HANDLE_LIBVLC_EXCEPTION_NULL( &ex );
    retval->p_playlist = retval->p_instance->p_libvlc_int->p_playlist;
    return retval;
}

void
mediacontrol_exit( mediacontrol_Instance *self )
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );

    libvlc_release( self->p_instance, &ex );
}

libvlc_instance_t*
mediacontrol_get_libvlc_instance( mediacontrol_Instance *self )
{
  return self->p_instance;
}

mediacontrol_Instance *
mediacontrol_new_from_instance( libvlc_instance_t* p_instance,
                mediacontrol_Exception *exception )
{
  mediacontrol_Instance* retval;

  retval = ( mediacontrol_Instance* )malloc( sizeof( mediacontrol_Instance ) );
  if( ! retval )
  {
      RAISE_NULL( mediacontrol_InternalException, "Out of memory" );
  }
  retval->p_instance = p_instance;
  retval->p_playlist = retval->p_instance->p_libvlc_int->p_playlist;
  return retval;
}

/**************************************************************************
 * Playback management
 **************************************************************************/
mediacontrol_Position*
mediacontrol_get_media_position( mediacontrol_Instance *self,
                                 const mediacontrol_PositionOrigin an_origin,
                                 const mediacontrol_PositionKey a_key,
                                 mediacontrol_Exception *exception )
{
    mediacontrol_Position* retval = NULL;
    libvlc_exception_t ex;
    vlc_int64_t pos;
    libvlc_media_instance_t * p_mi;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    retval = ( mediacontrol_Position* )malloc( sizeof( mediacontrol_Position ) );
    retval->origin = an_origin;
    retval->key = a_key;

    p_mi = libvlc_playlist_get_media_instance( self->p_instance, &ex);
    HANDLE_LIBVLC_EXCEPTION_NULL( &ex );

    if(  an_origin != mediacontrol_AbsolutePosition )
    {
        libvlc_media_instance_release( p_mi );
        /* Relative or ModuloPosition make no sense */
        RAISE_NULL( mediacontrol_PositionOriginNotSupported,
                    "Only absolute position is valid." );
    }

    /* We are asked for an AbsolutePosition. */
    pos = libvlc_media_instance_get_time( p_mi, &ex );

    if( a_key == mediacontrol_MediaTime )
    {
        retval->value = pos;
    }
    else
    {
        if( ! self->p_playlist->p_input )
        {
            libvlc_media_instance_release( p_mi );
            RAISE_NULL( mediacontrol_InternalException,
                        "No input" );
        }
        retval->value = private_mediacontrol_unit_convert( self->p_playlist->p_input,
                                                   mediacontrol_MediaTime,
                                                   a_key,
                                                   pos );
    }
    libvlc_media_instance_release( p_mi );
    return retval;
}

/* Sets the media position */
void
mediacontrol_set_media_position( mediacontrol_Instance *self,
                                 const mediacontrol_Position * a_position,
                                 mediacontrol_Exception *exception )
{
    libvlc_media_instance_t * p_mi;
    libvlc_exception_t ex;
    vlc_int64_t i_pos;

    libvlc_exception_init( &ex );
    mediacontrol_exception_init( exception );

    p_mi = libvlc_playlist_get_media_instance( self->p_instance, &ex);
    HANDLE_LIBVLC_EXCEPTION_VOID( &ex );

    i_pos = private_mediacontrol_position2microsecond( self->p_playlist->p_input, a_position );
    libvlc_media_instance_set_time( p_mi, i_pos / 1000, &ex );
    libvlc_media_instance_release( p_mi );
    HANDLE_LIBVLC_EXCEPTION_VOID( &ex );
}

/* Starts playing a stream */
/*
 * Known issues: since moving in the playlist using playlist_Next
 * or playlist_Prev implies starting to play items, the a_position
 * argument will be only honored for the 1st item in the list.
 *
 * XXX:FIXME split moving in the playlist and playing items two
 * different actions or make playlist_<Next|Prev> accept a time
 * value to start to play from.
 */
void
mediacontrol_start( mediacontrol_Instance *self,
                    const mediacontrol_Position * a_position,
                    mediacontrol_Exception *exception )
{
    playlist_t * p_playlist = self->p_playlist;

    mediacontrol_exception_init( exception );
    if( ! p_playlist )
    {
        RAISE( mediacontrol_PlaylistException, "No available playlist" );
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->items.i_size )
    {
        int i_from;
        char *psz_from = NULL;

        psz_from = ( char * )malloc( 20 * sizeof( char ) );
        if( psz_from && p_playlist->status.p_item )
        {
            i_from = private_mediacontrol_position2microsecond( p_playlist->p_input, a_position ) / 1000000;

            /* Set start time */
            snprintf( psz_from, 20, "start-time=%i", i_from );
            input_ItemAddOption( p_playlist->status.p_item->p_input, psz_from );
            free( psz_from );
        }

        vlc_mutex_unlock( &p_playlist->object_lock );
        playlist_Play( p_playlist );
    }
    else
    {
        RAISE( mediacontrol_PlaylistException, "Empty playlist." );
        vlc_mutex_unlock( &p_playlist->object_lock );
    }
}

void
mediacontrol_pause( mediacontrol_Instance *self,
                    const mediacontrol_Position * a_position,
                    mediacontrol_Exception *exception )
{
    input_thread_t *p_input = self->p_playlist->p_input;

    /* FIXME: use the a_position parameter */
    mediacontrol_exception_init( exception );
    if( p_input != NULL )
    {
        var_SetInteger( p_input, "state", PAUSE_S );
    }
    else
    {
        RAISE( mediacontrol_InternalException, "No input" );
    }
}

void
mediacontrol_resume( mediacontrol_Instance *self,
                     const mediacontrol_Position * a_position,
                     mediacontrol_Exception *exception )
{
    input_thread_t *p_input = self->p_playlist->p_input;

    /* FIXME: use the a_position parameter */
    mediacontrol_exception_init( exception );
    if( p_input != NULL )
    {
        var_SetInteger( p_input, "state", PAUSE_S );
    }
    else
    {
        RAISE( mediacontrol_InternalException, "No input" );
    }
}

void
mediacontrol_stop( mediacontrol_Instance *self,
                   const mediacontrol_Position * a_position,
                   mediacontrol_Exception *exception )
{
    /* FIXME: use the a_position parameter */
    mediacontrol_exception_init( exception );
    if( !self->p_playlist )
    {
        RAISE( mediacontrol_PlaylistException, "No playlist" );
    }
    else
        playlist_Stop( self->p_playlist );
}

/**************************************************************************
 * Playlist management
 **************************************************************************/

void
mediacontrol_playlist_add_item( mediacontrol_Instance *self,
                                const char * psz_file,
                                mediacontrol_Exception *exception )
{
    libvlc_exception_t ex;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    libvlc_playlist_add( self->p_instance, psz_file, psz_file, &ex );
    HANDLE_LIBVLC_EXCEPTION_VOID( &ex );
}

void
mediacontrol_playlist_next_item( mediacontrol_Instance *self,
                                 mediacontrol_Exception *exception )
{
    libvlc_exception_t ex;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    libvlc_playlist_next( self->p_instance, &ex );
    HANDLE_LIBVLC_EXCEPTION_VOID( &ex );
}

void
mediacontrol_playlist_clear( mediacontrol_Instance *self,
                             mediacontrol_Exception *exception )
{
    libvlc_exception_t ex;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    libvlc_playlist_clear( self->p_instance, &ex );
    HANDLE_LIBVLC_EXCEPTION_VOID( &ex );
}

mediacontrol_PlaylistSeq *
mediacontrol_playlist_get_list( mediacontrol_Instance *self,
                                mediacontrol_Exception *exception )
{
    mediacontrol_PlaylistSeq *retval = NULL;
    int i_index;
    playlist_t * p_playlist = self->p_playlist;
    int i_playlist_size;

    mediacontrol_exception_init( exception );
    if( !p_playlist )
    {
        RAISE( mediacontrol_PlaylistException, "No playlist" );
        return NULL;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    i_playlist_size = p_playlist->current.i_size;

    retval = private_mediacontrol_PlaylistSeq__alloc( i_playlist_size );

    for( i_index = 0 ; i_index < i_playlist_size ; i_index++ )
    {
        retval->data[i_index] = input_item_GetURI( ARRAY_VAL(p_playlist->current, i_index)->p_input );
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    return retval;
}

/***************************************************************************
 * Status feedback
 ***************************************************************************/

mediacontrol_StreamInformation *
mediacontrol_get_stream_information( mediacontrol_Instance *self,
                                     mediacontrol_PositionKey a_key,
                                     mediacontrol_Exception *exception )
{
    mediacontrol_StreamInformation *retval = NULL;
    input_thread_t *p_input = self->p_playlist->p_input;
    vlc_value_t val;

    retval = ( mediacontrol_StreamInformation* )
                            malloc( sizeof( mediacontrol_StreamInformation ) );
    if( ! retval )
    {
        RAISE( mediacontrol_InternalException, "Out of memory" );
        return NULL;
    }

    if( ! p_input )
    {
        /* No p_input defined */
        retval->streamstatus = mediacontrol_UndefinedStatus;
        retval->url          = strdup( "None" );
        retval->position     = 0;
        retval->length       = 0;
    }
    else
    {
        switch( var_GetInteger( p_input, "state" ) )
        {
        case PLAYING_S     :
            retval->streamstatus = mediacontrol_PlayingStatus;
            break;
        case PAUSE_S       :
            retval->streamstatus = mediacontrol_PauseStatus;
            break;
        case INIT_S        :
            retval->streamstatus = mediacontrol_InitStatus;
            break;
        case END_S         :
            retval->streamstatus = mediacontrol_EndStatus;
            break;
        default :
            retval->streamstatus = mediacontrol_UndefinedStatus;
            break;
        }

        retval->url = input_item_GetURI( input_GetItem( p_input ) );

        /* TIME and LENGTH are in microseconds. We want them in ms */
        var_Get( p_input, "time", &val);
        retval->position = val.i_time / 1000;

        var_Get( p_input, "length", &val);
        retval->length = val.i_time / 1000;

        retval->position = private_mediacontrol_unit_convert( p_input,
                                         mediacontrol_MediaTime, a_key,
                                         retval->position );
        retval->length   = private_mediacontrol_unit_convert( p_input,
                                         mediacontrol_MediaTime, a_key,
                                         retval->length );
    }
    return retval;
}
