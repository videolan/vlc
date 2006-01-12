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

#include <mediacontrol_internal.h>
#include <vlc/mediacontrol.h>

#include <vlc/intf.h>
#include <vlc/vout.h>
#include <vlc/aout.h>
#include <vlc_demux.h>

#include <vlc_osd.h>

#define HAS_SNAPSHOT 1

#ifdef HAS_SNAPSHOT
#include <snapshot.h>
#endif

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

#define RAISE( c, m )  exception->code = c; \
                       exception->message = strdup(m);

vlc_t * vlc_current_object( int );

mediacontrol_Instance* mediacontrol_new_from_object( int vlc_object_id,
                                                     mediacontrol_Exception *exception )
{
    mediacontrol_Instance* retval;
    vlc_object_t *p_vlc;
    vlc_object_t *p_object;

    p_object = ( vlc_object_t* )vlc_current_object( vlc_object_id );
    if( ! p_object )
    {
        RAISE( mediacontrol_InternalException, "Unable to find vlc object" );
        return NULL;
    }

    p_vlc = vlc_object_find( p_object, VLC_OBJECT_ROOT, FIND_PARENT );
    if( ! p_vlc )
    {
        RAISE( mediacontrol_InternalException, "Unable to initialize VLC" );
        return NULL;
    }
    retval = ( mediacontrol_Instance* )malloc( sizeof( mediacontrol_Instance ) );
    retval->p_vlc = p_vlc;
    retval->vlc_object_id = p_vlc->i_object_id;

    /* We can keep references on these, which should not change. Is it true ? */
    retval->p_playlist = vlc_object_find( p_vlc,
                                        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    retval->p_intf = vlc_object_find( p_vlc, VLC_OBJECT_INTF, FIND_ANYWHERE );

    if( ! retval->p_playlist || ! retval->p_intf )
    {
        RAISE( mediacontrol_InternalException, "No available interface" );
        return NULL;
    }
    return retval;
};


/**************************************************************************
 * Playback management
 **************************************************************************/
mediacontrol_Position*
mediacontrol_get_media_position( mediacontrol_Instance *self,
                                 const mediacontrol_PositionOrigin an_origin,
                                 const mediacontrol_PositionKey a_key,
                                 mediacontrol_Exception *exception )
{
    mediacontrol_Position* retval;
    vlc_value_t val;
    input_thread_t * p_input = self->p_playlist->p_input;

    exception = mediacontrol_exception_init( exception );

    retval = ( mediacontrol_Position* )malloc( sizeof( mediacontrol_Position ) );
    retval->origin = an_origin;
    retval->key = a_key;

    if( ! p_input )
    {
        RAISE( mediacontrol_InternalException, "No input thread." );
        return NULL;
    }

    if(  an_origin != mediacontrol_AbsolutePosition )
    {
        /* Relative or ModuloPosition make no sense */
        RAISE( mediacontrol_PositionOriginNotSupported, "Only absolute position is valid." );
        return NULL;
    }

    /* We are asked for an AbsolutePosition. */
    val.i_time = 0;
    var_Get( p_input, "time", &val );
    /* FIXME: check val.i_time > 0 */

    retval->value = mediacontrol_unit_convert( p_input,
                                               mediacontrol_MediaTime,
                                               a_key,
                                               val.i_time / 1000 );
    return retval;
}

/* Sets the media position */
void
mediacontrol_set_media_position( mediacontrol_Instance *self,
                                 const mediacontrol_Position * a_position,
                                 mediacontrol_Exception *exception )
{
    vlc_value_t val;
    input_thread_t * p_input = self->p_playlist->p_input;

    exception=mediacontrol_exception_init( exception );
    if( ! p_input )
    {
        RAISE( mediacontrol_InternalException, "No input thread." );
        return;
    }

    if( !var_GetBool( p_input, "seekable" ) )
    {
        RAISE( mediacontrol_InvalidPosition, "Stream not seekable" );
        return;
    }

    val.i_time = mediacontrol_position2microsecond( p_input, a_position );
    var_Set( p_input, "time", val );
    return;
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

    exception = mediacontrol_exception_init( exception );
    if( ! p_playlist )
    {
        RAISE( mediacontrol_PlaylistException, "No available playlist" );
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->i_size )
    {
        int i_index;
        int i_from;
	char * psz_from = ( char * )malloc( 20 * sizeof( char ) );

        i_from = mediacontrol_position2microsecond( p_playlist->p_input, a_position ) / 1000000;
	
        i_index = p_playlist->i_index;
        if( i_index < 0 )
        { 
            /* We know that there is at least 1 element, since i_size != 0 */
            i_index = 0;
        }

        /* Set start time */
	snprintf( psz_from, 20, "start-time=%i", i_from );
	playlist_ItemAddOption( p_playlist->pp_items[i_index], psz_from );
	free( psz_from );

        vlc_mutex_unlock( &p_playlist->object_lock );

        playlist_Play( p_playlist );
    }
    else
    {
        RAISE( mediacontrol_PlaylistException, "Empty playlist." );
        vlc_mutex_unlock( &p_playlist->object_lock );
    }

    return;
}

void
mediacontrol_pause( mediacontrol_Instance *self,
                    const mediacontrol_Position * a_position,
                    mediacontrol_Exception *exception )
{
    input_thread_t *p_input = self->p_playlist->p_input;

    /* FIXME: use the a_position parameter */
    exception=mediacontrol_exception_init( exception );
    if( p_input != NULL )
    {
        var_SetInteger( p_input, "state", PAUSE_S );
    }
    else
    {
        RAISE( mediacontrol_InternalException, "No input" );
    }

    return;
}

void
mediacontrol_resume( mediacontrol_Instance *self,
                     const mediacontrol_Position * a_position,
                     mediacontrol_Exception *exception )
{
    input_thread_t *p_input = self->p_playlist->p_input;

    /* FIXME: use the a_position parameter */
    exception=mediacontrol_exception_init( exception );
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
    exception=mediacontrol_exception_init( exception );
    if( !self->p_playlist )
    {
        RAISE( mediacontrol_PlaylistException, "No playlist" );
        return;
    }

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
    exception=mediacontrol_exception_init( exception );
    if( !self->p_playlist )
    {
        RAISE( mediacontrol_InternalException, "No playlist" );
        return;
    }

    playlist_Add( self->p_playlist, psz_file, psz_file , PLAYLIST_INSERT,
                  PLAYLIST_END );
}

void
mediacontrol_playlist_next_item( mediacontrol_Instance *self,
				 mediacontrol_Exception *exception )
{
    exception=mediacontrol_exception_init( exception );
    if ( !self->p_playlist )
    {
        RAISE( mediacontrol_InternalException, "No playlist" );
        return;
    }

    playlist_Next( self->p_playlist );

    return;
}

void
mediacontrol_playlist_clear( mediacontrol_Instance *self,
                             mediacontrol_Exception *exception )
{
    exception=mediacontrol_exception_init( exception );
    if( !self->p_playlist )
    {
        RAISE( mediacontrol_PlaylistException, "No playlist" );
        return;
    }

    playlist_Clear( self->p_playlist );

    return;
}

mediacontrol_PlaylistSeq *
mediacontrol_playlist_get_list( mediacontrol_Instance *self,
                                mediacontrol_Exception *exception )
{
    mediacontrol_PlaylistSeq *retval;
    int i_index;
    playlist_t * p_playlist = self->p_playlist;
    int i_playlist_size;

    exception=mediacontrol_exception_init( exception );
    if( !p_playlist )
    {
        RAISE( mediacontrol_PlaylistException, "No playlist" );
        return NULL;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    i_playlist_size = p_playlist->i_size;

    retval = mediacontrol_PlaylistSeq__alloc( i_playlist_size );

    for( i_index = 0 ; i_index < i_playlist_size ; i_index++ )
    {
        retval->data[i_index] = strdup( p_playlist->pp_items[i_index]->input.psz_uri );
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
    mediacontrol_StreamInformation *retval;
    input_thread_t *p_input = self->p_playlist->p_input;
    vlc_value_t val;

    retval = ( mediacontrol_StreamInformation* )malloc( sizeof( mediacontrol_StreamInformation ) );
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

        retval->url = strdup( p_input->input.p_item->psz_uri );

        /* TIME and LENGTH are in microseconds. We want them in ms */
        var_Get( p_input, "time", &val);
        retval->position = val.i_time / 1000;

        var_Get( p_input, "length", &val);
        retval->length = val.i_time / 1000;

        retval->position = mediacontrol_unit_convert( p_input,
                                                      mediacontrol_MediaTime, a_key,
                                                      retval->position );
        retval->length   = mediacontrol_unit_convert( p_input,
                                                      mediacontrol_MediaTime, a_key,
                                                      retval->length );
    }
    return retval;
}
