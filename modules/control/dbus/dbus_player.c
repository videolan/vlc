/*****************************************************************************
 * dbus-player.h : dbus control module (mpris v1.0) - /Player object
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
#include <vlc_interface.h>
#include <vlc_aout.h>

#include <math.h>

#include "dbus_player.h"
#include "dbus_common.h"

static int MarshalStatus ( intf_thread_t *, DBusMessageIter * );

/* XML data to answer org.freedesktop.DBus.Introspectable.Introspect requests */
static const char* psz_player_introspection_xml =
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
"<node>"
"  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"    <method name=\"Introspect\">\n"
"      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"  </interface>\n"
"  <interface name=\"org.freedesktop.MediaPlayer\">\n"
"    <method name=\"GetStatus\">\n"
"      <arg type=\"(iiii)\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"Prev\">\n"
"    </method>\n"
"    <method name=\"Next\">\n"
"    </method>\n"
"    <method name=\"Stop\">\n"
"    </method>\n"
"    <method name=\"Play\">\n"
"    </method>\n"
"    <method name=\"Pause\">\n"
"    </method>\n"
"    <method name=\"Repeat\">\n"
"      <arg type=\"b\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"VolumeSet\">\n"
"      <arg type=\"i\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"VolumeGet\">\n"
"      <arg type=\"i\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"PositionSet\">\n"
"      <arg type=\"i\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"PositionGet\">\n"
"      <arg type=\"i\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"GetMetadata\">\n"
"      <arg type=\"a{sv}\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"GetCaps\">\n"
"      <arg type=\"i\" direction=\"out\" />\n"
"    </method>\n"
"    <signal name=\"TrackChange\">\n"
"      <arg type=\"a{sv}\"/>\n"
"    </signal>\n"
"    <signal name=\"StatusChange\">\n"
"      <arg type=\"(iiii)\"/>\n"
"    </signal>\n"
"    <signal name=\"CapsChange\">\n"
"      <arg type=\"i\"/>\n"
"    </signal>\n"
"  </interface>\n"
"</node>\n"
;

DBUS_METHOD( PositionGet )
{ /* returns position in milliseconds */
    REPLY_INIT;
    OUT_ARGUMENTS;
    dbus_int32_t i_pos;

    input_thread_t *p_input = playlist_CurrentInput( PL );

    if( !p_input )
        i_pos = 0;
    else
    {
        i_pos = var_GetTime( p_input, "time" ) / 1000;
        vlc_object_release( p_input );
    }
    ADD_INT32( &i_pos );
    REPLY_SEND;
}

DBUS_METHOD( PositionSet )
{ /* set position in milliseconds */

    REPLY_INIT;
    vlc_value_t position;
    dbus_int32_t i_pos;

    DBusError error;
    dbus_error_init( &error );

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_INT32, &i_pos,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    input_thread_t *p_input = playlist_CurrentInput( PL );

    if( p_input )
    {
        position.i_time = ((mtime_t)i_pos) * 1000;
        var_Set( p_input, "time", position );
        vlc_object_release( p_input );
    }
    REPLY_SEND;
}

DBUS_METHOD( VolumeGet )
{ /* returns volume in percentage */
    REPLY_INIT;
    OUT_ARGUMENTS;
    dbus_int32_t i_dbus_vol;
    audio_volume_t i_vol;

    /* 2nd argument of aout_VolumeGet is int32 */
    aout_VolumeGet( PL, &i_vol );

    double f_vol = 100. * i_vol / AOUT_VOLUME_MAX;
    i_dbus_vol = round( f_vol );
    ADD_INT32( &i_dbus_vol );
    REPLY_SEND;
}

DBUS_METHOD( VolumeSet )
{ /* set volume in percentage */
    REPLY_INIT;

    DBusError error;
    dbus_error_init( &error );

    dbus_int32_t i_dbus_vol;
    audio_volume_t i_vol;

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_INT32, &i_dbus_vol,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    double f_vol = AOUT_VOLUME_MAX * i_dbus_vol / 100.;
    i_vol = round( f_vol );
    aout_VolumeSet( PL, i_vol );
    REPLY_SEND;
}

DBUS_METHOD( Next )
{ /* next playlist item */
    REPLY_INIT;
    playlist_Next( PL );
    REPLY_SEND;
}

DBUS_METHOD( Prev )
{ /* previous playlist item */
    REPLY_INIT;
    playlist_Prev( PL );
    REPLY_SEND;
}

DBUS_METHOD( Stop )
{ /* stop playing */
    REPLY_INIT;
    playlist_Stop( PL );
    REPLY_SEND;
}

DBUS_METHOD( GetStatus )
{ /* returns the current status as a struct of 4 ints */
/*
    First   0 = Playing, 1 = Paused, 2 = Stopped.
    Second  0 = Playing linearly , 1 = Playing randomly.
    Third   0 = Go to the next element once the current has finished playing , 1 = Repeat the current element
    Fourth  0 = Stop playing once the last element has been played, 1 = Never give up playing *
 */
    REPLY_INIT;
    OUT_ARGUMENTS;

    MarshalStatus( p_this, &args );

    REPLY_SEND;
}

DBUS_METHOD( Pause )
{
    REPLY_INIT;
    playlist_Pause( PL );
    REPLY_SEND;
}

DBUS_METHOD( Play )
{
    REPLY_INIT;

    input_thread_t *p_input =  playlist_CurrentInput( PL );

    if( p_input )
    {
        double i_pos = 0;
        input_Control( p_input, INPUT_SET_POSITION, i_pos );
        vlc_object_release( p_input );
    }
    else
        playlist_Play( PL );

    REPLY_SEND;
}

DBUS_METHOD( Repeat )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusError error;
    dbus_bool_t b_repeat;

    dbus_error_init( &error );
    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_BOOLEAN, &b_repeat,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    var_SetBool( PL, "repeat", ( b_repeat == TRUE ) );

    REPLY_SEND;
}

DBUS_METHOD( GetCurrentMetadata )
{
    REPLY_INIT;
    OUT_ARGUMENTS;
    playlist_t *p_playlist = PL;

    PL_LOCK;
    playlist_item_t* p_item =  playlist_CurrentPlayingItem( p_playlist );
    if( p_item )
        GetInputMeta( p_item->p_input, &args );
    PL_UNLOCK;
    REPLY_SEND;
}

DBUS_METHOD( GetCaps )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    ADD_INT32( &INTF->p_sys->i_caps );

    REPLY_SEND;
}

/*****************************************************************************
 * StatusChange: Player status change signal
 *****************************************************************************/

DBUS_SIGNAL( StatusChangeSignal )
{ /* send the updated status info on the bus */
    SIGNAL_INIT( DBUS_MPRIS_PLAYER_INTERFACE,
                 DBUS_MPRIS_PLAYER_PATH,
                 "StatusChange" );

    OUT_ARGUMENTS;

    /* we're called from a callback of input_thread_t, so it can not be
     * destroyed before we return */
    MarshalStatus( (intf_thread_t*) p_data, &args );

    SIGNAL_SEND;
}

/*****************************************************************************
 * TrackChange: Playlist item change callback
 *****************************************************************************/

DBUS_SIGNAL( TrackChangeSignal )
{ /* emit the metadata of the new item */
    SIGNAL_INIT( DBUS_MPRIS_PLAYER_INTERFACE,
                 DBUS_MPRIS_PLAYER_PATH,
                 "TrackChange" );

    OUT_ARGUMENTS;

    input_item_t *p_item = (input_item_t*) p_data;
    GetInputMeta ( p_item, &args );

    SIGNAL_SEND;
}

/******************************************************************************
 * CapsChange: player capabilities change signal
 *****************************************************************************/
DBUS_SIGNAL( CapsChangeSignal )
{
    SIGNAL_INIT( DBUS_MPRIS_PLAYER_INTERFACE,
                 DBUS_MPRIS_PLAYER_PATH,
                 "CapsChange" );

    OUT_ARGUMENTS;

    ADD_INT32( &((intf_thread_t*)p_data)->p_sys->i_caps );
    SIGNAL_SEND;
}

DBUS_METHOD( handle_introspect_player )
{
    VLC_UNUSED(p_this);
    REPLY_INIT;
    OUT_ARGUMENTS;
    ADD_STRING( &psz_player_introspection_xml );
    REPLY_SEND;
}

#define METHOD_FUNC( interface, method, function ) \
    else if( dbus_message_is_method_call( p_from, interface, method ) )\
        return function( p_conn, p_from, p_this )

DBusHandlerResult
handle_player ( DBusConnection *p_conn, DBusMessage *p_from, void *p_this )
{
    if( dbus_message_is_method_call( p_from,
                DBUS_INTERFACE_INTROSPECTABLE, "Introspect" ) )
        return handle_introspect_player( p_conn, p_from, p_this );

    /* here D-Bus method names are associated to an handler */

    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "Prev",        Prev );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "Next",        Next );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "Stop",        Stop );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "Play",        Play );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "Pause",       Pause );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "Repeat",      Repeat );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "VolumeSet",   VolumeSet );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "VolumeGet",   VolumeGet );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "PositionSet", PositionSet );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "PositionGet", PositionGet );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "GetStatus",   GetStatus );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "GetMetadata", GetCurrentMetadata );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "GetCaps",     GetCaps );

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

#undef METHOD_FUNC

/*****************************************************************************
 * StatusChangeEmit: Emits the StatusChange signal
 *****************************************************************************/
int StatusChangeEmit( intf_thread_t * p_intf )
{
    if( p_intf->p_sys->b_dead )
        return VLC_SUCCESS;

    UpdateCaps( p_intf );
    StatusChangeSignal( p_intf->p_sys->p_conn, p_intf );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * CapsChangeEmit: Emits the CapsChange signal
 *****************************************************************************/
int CapsChangeEmit( intf_thread_t * p_intf )
{
    if( p_intf->p_sys->b_dead )
        return VLC_SUCCESS;

    CapsChangeSignal( p_intf->p_sys->p_conn, p_intf );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * TrackChangeEmit: Emits the TrackChange signal
 *****************************************************************************/
int TrackChangeEmit( intf_thread_t * p_intf, input_item_t* p_item )
{
    if( p_intf->p_sys->b_dead )
        return VLC_SUCCESS;

    UpdateCaps( p_intf );
    TrackChangeSignal( p_intf->p_sys->p_conn, p_item );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * MarshalStatus: Fill a DBusMessage with the current player status
 *****************************************************************************/

static int MarshalStatus( intf_thread_t* p_intf, DBusMessageIter* args )
{ /* This is NOT the right way to do that, it would be better to sore
     the status information in p_sys and update it on change, thus
     avoiding a long lock */

    DBusMessageIter status;
    dbus_int32_t i_state, i_random, i_repeat, i_loop;
    playlist_t* p_playlist = p_intf->p_sys->p_playlist;

    vlc_mutex_lock( &p_intf->p_sys->lock );
    i_state = p_intf->p_sys->i_playing_state;
    vlc_mutex_unlock( &p_intf->p_sys->lock );

    i_random = var_CreateGetBool( p_playlist, "random" );

    i_repeat = var_CreateGetBool( p_playlist, "repeat" );

    i_loop = var_CreateGetBool( p_playlist, "loop" );

    dbus_message_iter_open_container( args, DBUS_TYPE_STRUCT, NULL, &status );
    dbus_message_iter_append_basic( &status, DBUS_TYPE_INT32, &i_state );
    dbus_message_iter_append_basic( &status, DBUS_TYPE_INT32, &i_random );
    dbus_message_iter_append_basic( &status, DBUS_TYPE_INT32, &i_repeat );
    dbus_message_iter_append_basic( &status, DBUS_TYPE_INT32, &i_loop );
    dbus_message_iter_close_container( args, &status );

    return VLC_SUCCESS;
}
