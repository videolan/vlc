/*****************************************************************************
 * dbus-tracklist.c : dbus control module (mpris v1.0) - /TrackList object
 *****************************************************************************
 * Copyright © 2006-2008 Rafaël Carré
 * Copyright © 2007-2010 Mirsal Ennaime
 * Copyright © 2009-2010 The VideoLAN team
 * $Id$
 *
 * Authors:    Mirsal Ennaime <mirsal at mirsal fr>
 *             Rafaël Carré <funman at videolanorg>
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

#include <vlc_common.h>
#include <vlc_playlist.h>

#include <assert.h>

#include "dbus_tracklist.h"
#include "dbus_common.h"

DBUS_METHOD( AddTrack )
{ /* add the string to the playlist, and play it if the boolean is true */
    REPLY_INIT;

    DBusError error;
    dbus_error_init( &error );

    char *psz_mrl, *psz_aftertrack;
    dbus_bool_t b_play;

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_STRING, &psz_mrl,
            DBUS_TYPE_OBJECT_PATH, &psz_aftertrack,
            DBUS_TYPE_BOOLEAN, &b_play,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

#warning psz_aftertrack is not used
    playlist_Add( PL, psz_mrl, NULL, PLAYLIST_APPEND |
            ( ( b_play == TRUE ) ? PLAYLIST_GO : 0 ) ,
            PLAYLIST_END, true, false );

    REPLY_SEND;
}

DBUS_METHOD( GetCurrentTrack )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    playlist_t *p_playlist = PL;

    PL_LOCK;
    dbus_int32_t i_position = PL->i_current_index;
    PL_UNLOCK;

    ADD_INT32( &i_position );
    REPLY_SEND;
}

DBUS_METHOD( GetMetadata )
{
    REPLY_INIT;
    OUT_ARGUMENTS;
    DBusError error;
    dbus_error_init( &error );

    dbus_int32_t i_position;
    playlist_t *p_playlist = PL;

    dbus_message_get_args( p_from, &error,
           DBUS_TYPE_INT32, &i_position,
           DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    PL_LOCK;
    if( i_position < p_playlist->current.i_size )
    {
        GetInputMeta( p_playlist->current.p_elems[i_position]->p_input, &args );
    }

    PL_UNLOCK;
    REPLY_SEND;
}

DBUS_METHOD( GetLength )
{
    REPLY_INIT;
    OUT_ARGUMENTS;
    playlist_t *p_playlist = PL;

    PL_LOCK;
    dbus_int32_t i_elements = PL->current.i_size;
    PL_UNLOCK;

    ADD_INT32( &i_elements );
    REPLY_SEND;
}

DBUS_METHOD( DelTrack )
{
    REPLY_INIT;

    DBusError error;
    dbus_error_init( &error );

    dbus_int32_t i_position;
    playlist_t *p_playlist = PL;

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_INT32, &i_position,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    PL_LOCK;
    if( i_position < p_playlist->current.i_size )
    {
        playlist_DeleteFromInput( p_playlist,
            p_playlist->current.p_elems[i_position]->p_input,
            pl_Locked );
    }
    PL_UNLOCK;

    REPLY_SEND;
}

DBUS_METHOD( SetLoop )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusError error;
    dbus_bool_t b_loop;

    dbus_error_init( &error );
    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_BOOLEAN, &b_loop,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    var_SetBool( PL, "loop", ( b_loop == TRUE ) );

    REPLY_SEND;
}

DBUS_METHOD( SetRandom )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusError error;
    dbus_bool_t b_random;

    dbus_error_init( &error );
    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_BOOLEAN, &b_random,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    var_SetBool( PL, "random", ( b_random == TRUE ) );

    REPLY_SEND;
}

/******************************************************************************
 * TrackListChange: tracklist order / length change signal
 *****************************************************************************/
DBUS_SIGNAL( TrackListChangeSignal )
{ /* emit the new tracklist lengh */
    SIGNAL_INIT( DBUS_MPRIS_TRACKLIST_INTERFACE,
                 DBUS_MPRIS_TRACKLIST_PATH,
                 "TrackListChange");

    OUT_ARGUMENTS;

    playlist_t *p_playlist = ((intf_thread_t*)p_data)->p_sys->p_playlist;
    PL_LOCK;
    dbus_int32_t i_elements = p_playlist->current.i_size;
    PL_UNLOCK;

    ADD_INT32( &i_elements );
    SIGNAL_SEND;
}

#define METHOD_FUNC( interface, method, function ) \
    else if( dbus_message_is_method_call( p_from, interface, method ) )\
        return function( p_conn, p_from, p_this )

DBusHandlerResult
handle_tracklist ( DBusConnection *p_conn, DBusMessage *p_from, void *p_this )
{
    if(0);

/*  METHOD_FUNC( DBUS_INTERFACE_PROPERTIES, "Get",    GetProperty );
    METHOD_FUNC( DBUS_INTERFACE_PROPERTIES, "Set",    SetProperty );
    METHOD_FUNC( DBUS_INTERFACE_PROPERTIES, "GetAll", GetAllProperties ); */

    /* here D-Bus method names are associated to an handler */

    METHOD_FUNC( DBUS_MPRIS_TRACKLIST_INTERFACE, "GetMetadata",     GetMetadata );
    METHOD_FUNC( DBUS_MPRIS_TRACKLIST_INTERFACE, "GetCurrentTrack", GetCurrentTrack );
    METHOD_FUNC( DBUS_MPRIS_TRACKLIST_INTERFACE, "GetLength",       GetLength );
    METHOD_FUNC( DBUS_MPRIS_TRACKLIST_INTERFACE, "AddTrack",        AddTrack );
    METHOD_FUNC( DBUS_MPRIS_TRACKLIST_INTERFACE, "DelTrack",        DelTrack );
    METHOD_FUNC( DBUS_MPRIS_TRACKLIST_INTERFACE, "SetLoop",         SetLoop );
    METHOD_FUNC( DBUS_MPRIS_TRACKLIST_INTERFACE, "SetRandom",       SetRandom );

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/*****************************************************************************
 * TrackListChangeEmit: Emits the TrackListChange signal
 *****************************************************************************/
/* FIXME: It is not called on tracklist reordering */
int TrackListChangeEmit( intf_thread_t *p_intf, int signal, int i_node )
{
    // "playlist-item-append"
    if( signal == SIGNAL_PLAYLIST_ITEM_APPEND )
    {
        /* don't signal when items are added/removed in p_category */
        playlist_t *p_playlist = p_intf->p_sys->p_playlist;
        PL_LOCK;
        playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i_node );
        assert( p_item );
        while( p_item->p_parent )
            p_item = p_item->p_parent;
        if( p_item == p_playlist->p_root_category )
        {
            PL_UNLOCK;
            return VLC_SUCCESS;
        }
        PL_UNLOCK;
    }

    if( p_intf->p_sys->b_dead )
        return VLC_SUCCESS;

    TrackListChangeSignal( p_intf->p_sys->p_conn, p_intf );
    return VLC_SUCCESS;
}

#undef METHOD_FUNC
