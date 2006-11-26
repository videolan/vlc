/*****************************************************************************
 * dbus.c : D-Bus control interface
 *****************************************************************************
 * Copyright (C) 2006 Rafaël Carré
 * $Id$
 *
 * Author:    Rafaël Carré <funman at videolanorg>
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

/*
 * D-Bus Specification:
 *      http://dbus.freedesktop.org/doc/dbus-specification.html
 * D-Bus low-level C API (libdbus)
 *      http://dbus.freedesktop.org/doc/dbus/api/html/index.html
 */

/*
 * TODO:
 *  properties ?
 *
 *  macros to read incoming arguments
 *
 *  explore different possible types (arrays..)
 *
 *  what must we do if org.videolan.vlc already exist on the bus ?
 *  ( there is more than one vlc instance )
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbus.h"

#include <vlc/vlc.h>
#include <vlc_aout.h>
#include <vlc_interface.h>
#include <vlc_meta.h>
#include <vlc_input.h>
#include <vlc_playlist.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void Run        ( intf_thread_t * );


static int ItemChange( vlc_object_t *p_this, const char *psz_var,
                    vlc_value_t oldval, vlc_value_t newval, void *p_data );

struct intf_sys_t
{
    DBusConnection *p_conn;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_shortname( _("dbus"));
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    set_description( _("D-Bus control interface") );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Methods
 *****************************************************************************/

DBUS_METHOD( Nothing )
{ /* do nothing */
    REPLY_INIT;
    REPLY_SEND;
}

DBUS_METHOD( Quit )
{ /* exits vlc */
    REPLY_INIT;
    playlist_t *p_playlist = pl_Yield( (vlc_object_t*) p_this );
    playlist_Stop( p_playlist );
    pl_Release( ((vlc_object_t*) p_this) );
    ((vlc_object_t*)p_this)->p_libvlc->b_die = VLC_TRUE;
    REPLY_SEND;
}

DBUS_METHOD( PositionGet )
{ /* returns position as an int in the range [0;1000] */
    REPLY_INIT;
    OUT_ARGUMENTS;
    vlc_value_t position;
    dbus_uint16_t i_pos;

    playlist_t *p_playlist = pl_Yield( ((vlc_object_t*) p_this) );
    input_thread_t *p_input = p_playlist->p_input;

    if( !p_input )
        i_pos = 0;
    else
    {
        var_Get( p_input, "position", &position );
        i_pos = position.f_float * 1000 ;
    }
    ADD_UINT16( &i_pos );
    pl_Release( ((vlc_object_t*) p_this) );
    REPLY_SEND;
}

DBUS_METHOD( PositionSet )
{ /* set position from an int in the range [0;1000] */

    REPLY_INIT;
    vlc_value_t position;
    dbus_uint16_t i_pos;

    DBusError error;
    dbus_error_init( &error );

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_UINT16, &i_pos,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s\n",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    playlist_t *p_playlist = pl_Yield( ((vlc_object_t*) p_this) );
    input_thread_t *p_input = p_playlist->p_input;

    if( p_input )
    {
        position.f_float = ((float)i_pos) / 1000;
        var_Set( p_input, "position", position );
    }
    pl_Release( ((vlc_object_t*) p_this) );
    REPLY_SEND;
}

DBUS_METHOD( VolumeGet )
{ /* returns volume in percentage */
    REPLY_INIT;
    OUT_ARGUMENTS;
    dbus_uint16_t i_vol;
    /* 2nd argument of aout_VolumeGet is uint16 */
    aout_VolumeGet( (vlc_object_t*) p_this, &i_vol );
    i_vol = ( 100 * i_vol ) / AOUT_VOLUME_MAX;
    ADD_UINT16( &i_vol );
    REPLY_SEND;
}

DBUS_METHOD( VolumeSet )
{ /* set volume in percentage */
    REPLY_INIT;

    DBusError error;
    dbus_error_init( &error );

    dbus_uint16_t i_vol;

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_UINT16, &i_vol,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s\n",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    aout_VolumeSet( (vlc_object_t*) p_this, ( AOUT_VOLUME_MAX / 100 ) * i_vol );

    REPLY_SEND;
}

DBUS_METHOD( Next )
{ /* next playlist item */
    REPLY_INIT;
    playlist_t *p_playlist = pl_Yield( ((vlc_object_t*) p_this) );
    playlist_Next( p_playlist );
    pl_Release( ((vlc_object_t*) p_this) );
    REPLY_SEND;
}

DBUS_METHOD( Prev )
{ /* previous playlist item */
    REPLY_INIT;
    playlist_t *p_playlist = pl_Yield( ((vlc_object_t*) p_this) );
    playlist_Prev( p_playlist );
    pl_Release( ((vlc_object_t*) p_this) );
    REPLY_SEND;
}

DBUS_METHOD( Stop )
{ /* stop playing */
    REPLY_INIT;
    playlist_t *p_playlist = pl_Yield( ((vlc_object_t*) p_this) );
    playlist_Stop( p_playlist );
    pl_Release( ((vlc_object_t*) p_this) );
    REPLY_SEND;
}

DBUS_METHOD( GetPlayingItem )
{ /* return the current item */
    REPLY_INIT;
    OUT_ARGUMENTS;
    char psz_no_input = '\0';
    char *p_psz_no_input = &psz_no_input;
    playlist_t *p_playlist = pl_Yield( ((vlc_object_t*) p_this) );
    input_thread_t *p_input = p_playlist->p_input;
    ADD_STRING( ( p_input ) ? &input_GetItem(p_input)->psz_name :
            &p_psz_no_input );
    pl_Release( ((vlc_object_t*) p_this) );
    REPLY_SEND;
}

DBUS_METHOD( GetPlayStatus )
{ /* return a string */
    REPLY_INIT;
    OUT_ARGUMENTS;

    char *psz_play;
    vlc_value_t val;
    playlist_t *p_playlist = pl_Yield( (vlc_object_t*) p_this );
    input_thread_t *p_input = p_playlist->p_input;

    if( !p_input )
        psz_play = strdup( "stopped" );
    else
    {
        var_Get( p_input, "state", &val );
        if( val.i_int == PAUSE_S )
            psz_play = strdup( "pause" );
        else if( val.i_int == PLAYING_S )
            psz_play = strdup( "playing" );
        else psz_play = strdup( "unknown" );
    }

    pl_Release( p_playlist );

    ADD_STRING( &psz_play );
    free( psz_play );
    REPLY_SEND;
}

DBUS_METHOD( TogglePause )
{ /* return a bool: true if playing */
    REPLY_INIT;
    OUT_ARGUMENTS;

    vlc_value_t val;
    playlist_t *p_playlist = pl_Yield( (vlc_object_t*) p_this );
    input_thread_t *p_input = p_playlist->p_input;
    if( p_input != NULL )
    {
        var_Get( p_input, "state", &val );
        if( val.i_int != PAUSE_S )
        {
            val.i_int = PAUSE_S;
            playlist_Pause( p_playlist );
        }
        else
        {
            val.i_int = PLAYING_S;
            playlist_Play( p_playlist );
        }
    }
    else
    {
        val.i_int = PLAYING_S;
        playlist_Play( p_playlist );
    }
    pl_Release( p_playlist );

    dbus_bool_t pause = ( val.i_int == PLAYING_S ) ? TRUE : FALSE;
    ADD_BOOL( &pause );
    REPLY_SEND;
}

DBUS_METHOD( AddMRL )
{ /* add the string to the playlist, and play it if the boolean is true */
    REPLY_INIT;

    DBusError error;
    dbus_error_init( &error );

    char *psz_mrl;
    dbus_bool_t b_play;

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_STRING, &psz_mrl,
            DBUS_TYPE_BOOLEAN, &b_play,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s\n",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    playlist_t *p_playlist = pl_Yield( (vlc_object_t*) p_this );
    playlist_Add( p_playlist, psz_mrl, NULL, PLAYLIST_APPEND |
            ( ( b_play == TRUE ) ? PLAYLIST_GO : 0 ) , PLAYLIST_END, VLC_TRUE );
    pl_Release( p_playlist );

    REPLY_SEND;
}

/*****************************************************************************
 * Introspection method
 *****************************************************************************/

DBUS_METHOD( handle_introspect )
{ /* handles introspection of /org/videolan/vlc */
    REPLY_INIT;
    OUT_ARGUMENTS;
    ADD_STRING( &psz_introspection_xml_data );
    REPLY_SEND;
}

/*****************************************************************************
 * handle_messages: answer to incoming messages
 *****************************************************************************/

#define METHOD_FUNC( method, function ) \
    else if( dbus_message_is_method_call( p_from, VLC_DBUS_INTERFACE, method ) )\
        return function( p_conn, p_from, p_this )

DBUS_METHOD( handle_messages )
{ /* the main handler, that call methods */

    if( dbus_message_is_method_call( p_from,
                DBUS_INTERFACE_INTROSPECTABLE, "Introspect" ) )
        return handle_introspect( p_conn, p_from, p_this );

    /* here D-Bus method's names are associated to an handler */

    METHOD_FUNC( "GetPlayStatus",   GetPlayStatus );
    METHOD_FUNC( "GetPlayingItem",  GetPlayingItem );
    METHOD_FUNC( "AddMRL",          AddMRL );
    METHOD_FUNC( "TogglePause",     TogglePause );
    METHOD_FUNC( "Nothing",         Nothing );
    METHOD_FUNC( "Prev",            Prev );
    METHOD_FUNC( "Next",            Next );
    METHOD_FUNC( "Quit",            Quit );
    METHOD_FUNC( "Stop",            Stop );
    METHOD_FUNC( "VolumeSet",       VolumeSet );
    METHOD_FUNC( "VolumeGet",       VolumeGet );
    METHOD_FUNC( "PositionSet",     PositionSet );
    METHOD_FUNC( "PositionGet",     PositionGet );

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/

static int Open( vlc_object_t *p_this )
{ /* initialisation of the connection */
    intf_thread_t   *p_intf = (intf_thread_t*)p_this;
    intf_sys_t      *p_sys  = malloc( sizeof( intf_sys_t ) );
    playlist_t      *p_playlist;
    DBusConnection  *p_conn;
    DBusError       error;

    if( !p_sys )
        return VLC_ENOMEM;

    dbus_threads_init_default();

    dbus_error_init( &error );

    /* connect to the session bus */
    p_conn = dbus_bus_get( DBUS_BUS_SESSION, &error );
    if( !p_conn )
    {
        msg_Err( p_this, "Failed to connect to the D-Bus session daemon: %s",
                error.message );
        dbus_error_free( &error );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* we unregister the object /, registered by libvlc */
    dbus_connection_unregister_object_path( p_conn, "/" );

    /* we register the object /org/videolan/vlc */
    dbus_connection_register_object_path( p_conn, VLC_DBUS_OBJECT_PATH,
            &vlc_dbus_vtable, p_this );

    dbus_connection_flush( p_conn );

    p_playlist = pl_Yield( p_intf );
    PL_LOCK;
    var_AddCallback( p_playlist, "playlist-current", ItemChange, p_intf );
    PL_UNLOCK;
    pl_Release( p_playlist );

    p_intf->pf_run = Run;
    p_intf->p_sys = p_sys;
    p_sys->p_conn = p_conn;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/

static void Close   ( vlc_object_t *p_this )
{
    intf_thread_t   *p_intf     = (intf_thread_t*) p_this;
    playlist_t      *p_playlist = pl_Yield( p_intf );;

    PL_LOCK;
    var_DelCallback( p_playlist, "playlist-current", ItemChange, p_intf );
    PL_UNLOCK;
    pl_Release( p_playlist );

    dbus_connection_unref( p_intf->p_sys->p_conn );

    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/

static void Run          ( intf_thread_t *p_intf )
{
    while( !p_intf->b_die )
    {
        msleep( INTF_IDLE_SLEEP );
        dbus_connection_read_write_dispatch( p_intf->p_sys->p_conn, 0 );
    }
}

/*****************************************************************************
 * ItemChange: Playlist item change callback
 *****************************************************************************/

DBUS_SIGNAL( ItemChangeSignal )
{ /* emit the name of the new item */
    SIGNAL_INIT( "ItemChange" );
    OUT_ARGUMENTS;

    input_thread_t *p_input = (input_thread_t*) p_data;
    ADD_STRING( &input_GetItem(p_input)->psz_name );

    SIGNAL_SEND;
}

static int ItemChange( vlc_object_t *p_this, const char *psz_var,
            vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t       *p_intf     = ( intf_thread_t* ) p_data;
    intf_sys_t          *p_sys      = p_intf->p_sys;
    playlist_t          *p_playlist;
    input_thread_t      *p_input    = NULL;
    (void)p_this; (void)psz_var; (void)oldval; (void)newval;

    p_playlist = pl_Yield( p_intf );
    PL_LOCK;
    p_input = p_playlist->p_input;

    if( !p_input )
    {
        PL_UNLOCK;
        pl_Release( p_playlist );
        return VLC_SUCCESS;
    }

    vlc_object_yield( p_input );
    PL_UNLOCK;
    pl_Release( p_playlist );

    ItemChangeSignal( p_sys->p_conn, p_input );

    vlc_object_release( p_input );
    return VLC_SUCCESS;
}

