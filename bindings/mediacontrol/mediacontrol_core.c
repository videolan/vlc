/*****************************************************************************
 * mediacontrol_core.c: Core functions : init, playlist, stream management
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
#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>

#include <vlc_vout.h>
#include <vlc_aout.h>
#include <vlc_input.h>
#include <vlc_osd.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <stdio.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <sys/types.h>

mediacontrol_Instance* mediacontrol_new( int argc, char** argv, mediacontrol_Exception *exception )
{
    mediacontrol_Instance* retval;
    mediacontrol_exception_init( exception );

    retval = ( mediacontrol_Instance* )malloc( sizeof( mediacontrol_Instance ) );
    if( !retval )
        RAISE_NULL( mediacontrol_InternalException, "Out of memory" );

    retval->p_instance = libvlc_new( argc, (const char**)argv );
    if( !retval->p_instance )
        RAISE_NULL( mediacontrol_InternalException, "Out of memory" );
    retval->p_media_player = libvlc_media_player_new( retval->p_instance );
    if( !retval->p_media_player )
        RAISE_NULL( mediacontrol_InternalException, "Out of memory" );
    return retval;
}

void
mediacontrol_exit( mediacontrol_Instance *self )
{
    libvlc_release( self->p_instance );
}

libvlc_instance_t*
mediacontrol_get_libvlc_instance( mediacontrol_Instance *self )
{
    return self->p_instance;
}

libvlc_media_player_t*
mediacontrol_get_media_player( mediacontrol_Instance *self )
{
    return self->p_media_player;
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
    retval->p_media_player = libvlc_media_player_new( retval->p_instance );
    if( ! retval->p_media_player )
         RAISE_NULL( mediacontrol_InternalException, "Out of memory" );
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
    int64_t pos;

    mediacontrol_exception_init( exception );

    retval = ( mediacontrol_Position* )malloc( sizeof( mediacontrol_Position ) );
    retval->origin = an_origin;
    retval->key = a_key;

    if(  an_origin != mediacontrol_AbsolutePosition )
    {
        free( retval );
        /* Relative or ModuloPosition make no sense */
        RAISE_NULL( mediacontrol_PositionOriginNotSupported,
                    "Only absolute position is valid." );
    }

    /* We are asked for an AbsolutePosition. */
    pos = libvlc_media_player_get_time( self->p_media_player );

    if( a_key == mediacontrol_MediaTime )
    {
        retval->value = pos;
    }
    else
    {
        retval->value = private_mediacontrol_unit_convert( self->p_media_player,
                                                           mediacontrol_MediaTime,
                                                           a_key,
                                                           pos );
    }
    return retval;
}

/* Sets the media position */
void
mediacontrol_set_media_position( mediacontrol_Instance *self,
                                 const mediacontrol_Position * a_position,
                                 mediacontrol_Exception *exception )
{
    int64_t i_pos;

    mediacontrol_exception_init( exception );

    i_pos = private_mediacontrol_position2microsecond( self->p_media_player, a_position );
    libvlc_media_player_set_time( self->p_media_player, i_pos / 1000 );
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
    libvlc_media_t * p_media;
    char * psz_name;
    libvlc_exception_t ex;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    p_media = libvlc_media_player_get_media( self->p_media_player );

    if ( ! p_media )
    {
        /* No media was defined. */
        RAISE( mediacontrol_PlaylistException, "No defined media." );
    }
    else
    {
        /* A media was defined. Get its mrl to reuse it, but reset the options
           (because start-time may have been set on the previous invocation */
        psz_name = libvlc_media_get_mrl( p_media );
        HANDLE_LIBVLC_EXCEPTION_VOID( &ex );

        /* Create a new media */
        p_media = libvlc_media_new_location( self->p_instance, psz_name, &ex );
        HANDLE_LIBVLC_EXCEPTION_VOID( &ex );

        if( a_position->value )
        {
            char * psz_from;
            libvlc_time_t i_from;

            /* A start position was specified. Add it to media options */
            psz_from = ( char * )malloc( 20 * sizeof( char ) );
            i_from = private_mediacontrol_position2microsecond( self->p_media_player, a_position ) / 1000000;
            snprintf( psz_from, 20, "start-time=%"PRId64, i_from );
            libvlc_media_add_option( p_media, psz_from );
            HANDLE_LIBVLC_EXCEPTION_VOID( &ex );
        }

        libvlc_media_player_set_media( self->p_media_player, p_media );

        libvlc_media_player_play( self->p_media_player );
    }
}

void
mediacontrol_pause( mediacontrol_Instance *self,
                    mediacontrol_Exception *exception )
{
    mediacontrol_exception_init( exception );
    libvlc_media_player_pause( self->p_media_player );
}

void
mediacontrol_resume( mediacontrol_Instance *self,
                     mediacontrol_Exception *exception )
{
    mediacontrol_exception_init( exception );
    libvlc_media_player_pause( self->p_media_player );
}

void
mediacontrol_stop( mediacontrol_Instance *self,
                   mediacontrol_Exception *exception )
{
    mediacontrol_exception_init( exception );
    libvlc_media_player_stop( self->p_media_player );
}

/**************************************************************************
 * File management
 **************************************************************************/

void
mediacontrol_set_mrl( mediacontrol_Instance *self,
                      const char * psz_file,
                      mediacontrol_Exception *exception )
{
    libvlc_media_t * p_media;
    libvlc_exception_t ex;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    p_media = libvlc_media_new_location( self->p_instance, psz_file, &ex );
    HANDLE_LIBVLC_EXCEPTION_VOID( &ex );

    libvlc_media_player_set_media( self->p_media_player, p_media );
}

char *
mediacontrol_get_mrl( mediacontrol_Instance *self,
                      mediacontrol_Exception *exception )
{
    libvlc_media_t * p_media;
    libvlc_exception_t ex;

    mediacontrol_exception_init( exception );
    libvlc_exception_init( &ex );

    p_media = libvlc_media_player_get_media( self->p_media_player );

    if ( ! p_media )
    {
        return strdup( "" );
    }
    else
    {
        char * psz_mrl;

        psz_mrl = libvlc_media_get_mrl( p_media );
        HANDLE_LIBVLC_EXCEPTION_NULL( &ex );
        return psz_mrl;
    }
}

/***************************************************************************
 * Status feedback
 ***************************************************************************/

mediacontrol_StreamInformation *
mediacontrol_get_stream_information( mediacontrol_Instance *self,
                                     mediacontrol_PositionKey a_key,
                                     mediacontrol_Exception *exception )
{
    (void)a_key;
    mediacontrol_StreamInformation *retval = NULL;
    libvlc_media_t * p_media;
    libvlc_exception_t ex;

    libvlc_exception_init( &ex );

    retval = ( mediacontrol_StreamInformation* )
                            malloc( sizeof( mediacontrol_StreamInformation ) );
    if( ! retval )
    {
        RAISE( mediacontrol_InternalException, "Out of memory" );
        return NULL;
    }

    p_media = libvlc_media_player_get_media( self->p_media_player );
    if( ! p_media )
    {
        /* No p_media defined */
        retval->streamstatus = mediacontrol_UndefinedStatus;
        retval->url          = strdup( "" );
        retval->position     = 0;
        retval->length       = 0;
    }
    else
    {
        libvlc_state_t state;

        state = libvlc_media_player_get_state( self->p_media_player );
        switch( state )
        {
        case libvlc_NothingSpecial:
            retval->streamstatus = mediacontrol_UndefinedStatus;
            break;
        case libvlc_Opening :
            retval->streamstatus = mediacontrol_InitStatus;
            break;
        case libvlc_Buffering:
            retval->streamstatus = mediacontrol_BufferingStatus;
            break;
        case libvlc_Playing:
            retval->streamstatus = mediacontrol_PlayingStatus;
            break;
        case libvlc_Paused:
            retval->streamstatus = mediacontrol_PauseStatus;
            break;
        case libvlc_Stopped:
            retval->streamstatus = mediacontrol_StopStatus;
            break;
        case libvlc_Ended:
            retval->streamstatus = mediacontrol_EndStatus;
            break;
        case libvlc_Error:
            retval->streamstatus = mediacontrol_ErrorStatus;
            break;
        default :
            retval->streamstatus = mediacontrol_UndefinedStatus;
            break;
        }

        retval->url = libvlc_media_get_mrl( p_media );

        retval->position = libvlc_media_player_get_time( self->p_media_player );
        retval->length = libvlc_media_player_get_length( self->p_media_player );
    }
    return retval;
}
