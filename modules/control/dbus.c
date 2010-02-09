/*****************************************************************************
 * dbus.c : D-Bus control interface
 *****************************************************************************
 * Copyright © 2006-2008 Rafaël Carré
 * Copyright © 2007-2008 Mirsal Ennaime
 * Copyright © 2009 The VideoLAN team
 * $Id$
 *
 * Authors:    Rafaël Carré <funman at videolanorg>
 *             Mirsal Ennaime <mirsal dot ennaime at gmail dot com>
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
 *  extract:
 *   "If you use this low-level API directly, you're signing up for some pain."
 *
 * MPRIS Specification version 1.0
 *      http://wiki.xmms2.xmms.se/index.php/MPRIS
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <dbus/dbus.h>
#include "dbus.h"

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>

#include <math.h>

#include <assert.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void Run     ( intf_thread_t * );

static int StateChange( intf_thread_t *, int );
static int TrackChange( intf_thread_t * );
static int StatusChangeEmit( intf_thread_t *);
static int TrackListChangeEmit( intf_thread_t *, int, int );

static int AllCallback( vlc_object_t*, const char*, vlc_value_t, vlc_value_t, void* );

static int GetInputMeta ( input_item_t *, DBusMessageIter * );
static int MarshalStatus ( intf_thread_t *, DBusMessageIter * );
static int UpdateCaps( intf_thread_t* );

/* GetCaps() capabilities */
enum
{
     CAPS_NONE                  = 0,
     CAPS_CAN_GO_NEXT           = 1 << 0,
     CAPS_CAN_GO_PREV           = 1 << 1,
     CAPS_CAN_PAUSE             = 1 << 2,
     CAPS_CAN_PLAY              = 1 << 3,
     CAPS_CAN_SEEK              = 1 << 4,
     CAPS_CAN_PROVIDE_METADATA  = 1 << 5,
     CAPS_CAN_HAS_TRACKLIST     = 1 << 6
};

// The signal that can be get from the callbacks
enum
{
    SIGNAL_ITEM_CURRENT,
    SIGNAL_INTF_CHANGE,
    SIGNAL_PLAYLIST_ITEM_APPEND,
    SIGNAL_PLAYLIST_ITEM_DELETED,
    SIGNAL_RANDOM,
    SIGNAL_REPEAT,
    SIGNAL_LOOP,
    SIGNAL_STATE
};

struct intf_sys_t
{
    DBusConnection *p_conn;
    playlist_t     *p_playlist;
    bool            b_meta_read;
    dbus_int32_t    i_caps;
    bool            b_dead;
    vlc_array_t    *p_events;
    vlc_mutex_t     lock;
};

typedef struct
{
    int signal;
    int i_node;
    int i_input_state;
} callback_info_t;

#define INTF ((intf_thread_t *)p_this)
#define PL   (INTF->p_sys->p_playlist)


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_shortname( N_("dbus"))
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_description( N_("D-Bus control interface") )
    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Methods
 *****************************************************************************/

/* Player */

DBUS_METHOD( Quit )
{ /* exits vlc */
    REPLY_INIT;
    libvlc_Quit(INTF->p_libvlc);
    REPLY_SEND;
}

DBUS_METHOD( MprisVersion )
{ /*implemented version of the mpris spec */
    REPLY_INIT;
    OUT_ARGUMENTS;
    VLC_UNUSED( p_this );
    dbus_uint16_t i_major = VLC_MPRIS_VERSION_MAJOR;
    dbus_uint16_t i_minor = VLC_MPRIS_VERSION_MINOR;
    DBusMessageIter version;

    if( !dbus_message_iter_open_container( &args, DBUS_TYPE_STRUCT, NULL,
            &version ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_append_basic( &version, DBUS_TYPE_UINT16,
            &i_major ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_append_basic( &version, DBUS_TYPE_UINT16,
            &i_minor ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_close_container( &args, &version ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    REPLY_SEND;
}

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

/* Media Player information */

DBUS_METHOD( Identity )
{
    VLC_UNUSED(p_this);
    REPLY_INIT;
    OUT_ARGUMENTS;
    char *psz_identity;

    if( asprintf( &psz_identity, "%s %s", PACKAGE, VERSION ) != -1 )
    {
        ADD_STRING( &psz_identity );
        free( psz_identity );
    }
    else
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    REPLY_SEND;
}

/* TrackList */

DBUS_METHOD( AddTrack )
{ /* add the string to the playlist, and play it if the boolean is true */
    REPLY_INIT;
    OUT_ARGUMENTS;

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
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    playlist_Add( PL, psz_mrl, NULL, PLAYLIST_APPEND |
            ( ( b_play == TRUE ) ? PLAYLIST_GO : 0 ) ,
            PLAYLIST_END, true, false );

    dbus_int32_t i_success = 0;
    ADD_INT32( &i_success );

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
/*****************************************************************************
 * Introspection method
 *****************************************************************************/

DBUS_METHOD( handle_introspect_root )
{ /* handles introspection of root object */
    VLC_UNUSED(p_this);
    REPLY_INIT;
    OUT_ARGUMENTS;
    ADD_STRING( &psz_introspection_xml_data_root );
    REPLY_SEND;
}

DBUS_METHOD( handle_introspect_player )
{
    VLC_UNUSED(p_this);
    REPLY_INIT;
    OUT_ARGUMENTS;
    ADD_STRING( &psz_introspection_xml_data_player );
    REPLY_SEND;
}

DBUS_METHOD( handle_introspect_tracklist )
{
    VLC_UNUSED(p_this);
    REPLY_INIT;
    OUT_ARGUMENTS;
    ADD_STRING( &psz_introspection_xml_data_tracklist );
    REPLY_SEND;
}

/*****************************************************************************
 * handle_*: answer to incoming messages
 *****************************************************************************/

#define METHOD_FUNC( method, function ) \
    else if( dbus_message_is_method_call( p_from, MPRIS_DBUS_INTERFACE, method ) )\
        return function( p_conn, p_from, p_this )

DBUS_METHOD( handle_root )
{

    if( dbus_message_is_method_call( p_from,
                DBUS_INTERFACE_INTROSPECTABLE, "Introspect" ) )
        return handle_introspect_root( p_conn, p_from, p_this );

    /* here D-Bus method's names are associated to an handler */

    METHOD_FUNC( "Identity",                Identity );
    METHOD_FUNC( "MprisVersion",            MprisVersion );
    METHOD_FUNC( "Quit",                    Quit );

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


DBUS_METHOD( handle_player )
{
    if( dbus_message_is_method_call( p_from,
                DBUS_INTERFACE_INTROSPECTABLE, "Introspect" ) )
        return handle_introspect_player( p_conn, p_from, p_this );

    /* here D-Bus method's names are associated to an handler */

    METHOD_FUNC( "Prev",                    Prev );
    METHOD_FUNC( "Next",                    Next );
    METHOD_FUNC( "Stop",                    Stop );
    METHOD_FUNC( "Play",                    Play );
    METHOD_FUNC( "Pause",                   Pause );
    METHOD_FUNC( "Repeat",                  Repeat );
    METHOD_FUNC( "VolumeSet",               VolumeSet );
    METHOD_FUNC( "VolumeGet",               VolumeGet );
    METHOD_FUNC( "PositionSet",             PositionSet );
    METHOD_FUNC( "PositionGet",             PositionGet );
    METHOD_FUNC( "GetStatus",               GetStatus );
    METHOD_FUNC( "GetMetadata",             GetCurrentMetadata );
    METHOD_FUNC( "GetCaps",                 GetCaps );

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBUS_METHOD( handle_tracklist )
{
    if( dbus_message_is_method_call( p_from,
                DBUS_INTERFACE_INTROSPECTABLE, "Introspect" ) )
    return handle_introspect_tracklist( p_conn, p_from, p_this );

    /* here D-Bus method's names are associated to an handler */

    METHOD_FUNC( "GetMetadata",             GetMetadata );
    METHOD_FUNC( "GetCurrentTrack",         GetCurrentTrack );
    METHOD_FUNC( "GetLength",               GetLength );
    METHOD_FUNC( "AddTrack",                AddTrack );
    METHOD_FUNC( "DelTrack",                DelTrack );
    METHOD_FUNC( "SetLoop",                 SetLoop );
    METHOD_FUNC( "SetRandom",               SetRandom );

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

    p_sys->b_meta_read = false;
    p_sys->i_caps = CAPS_NONE;
    p_sys->b_dead = false;

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

    /* register a well-known name on the bus */
    dbus_bus_request_name( p_conn, VLC_MPRIS_DBUS_SERVICE, 0, &error );
    if( dbus_error_is_set( &error ) )
    {
        msg_Err( p_this, "Error requesting service " VLC_MPRIS_DBUS_SERVICE
                 ": %s", error.message );
        dbus_error_free( &error );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* we register the objects */
    dbus_connection_register_object_path( p_conn, MPRIS_DBUS_ROOT_PATH,
            &vlc_dbus_root_vtable, p_this );
    dbus_connection_register_object_path( p_conn, MPRIS_DBUS_PLAYER_PATH,
            &vlc_dbus_player_vtable, p_this );
    dbus_connection_register_object_path( p_conn, MPRIS_DBUS_TRACKLIST_PATH,
            &vlc_dbus_tracklist_vtable, p_this );

    dbus_connection_flush( p_conn );

    p_intf->pf_run = Run;
    p_intf->p_sys = p_sys;
    p_sys->p_conn = p_conn;
    p_sys->p_events = vlc_array_new();
    vlc_mutex_init( &p_sys->lock );

    p_playlist = pl_Get( p_intf );
    p_sys->p_playlist = p_playlist;

    PL_LOCK;
    var_AddCallback( p_playlist, "item-current", AllCallback, p_intf );
    var_AddCallback( p_playlist, "intf-change", AllCallback, p_intf );
    var_AddCallback( p_playlist, "playlist-item-append", AllCallback, p_intf );
    var_AddCallback( p_playlist, "playlist-item-deleted", AllCallback, p_intf );
    var_AddCallback( p_playlist, "random", AllCallback, p_intf );
    var_AddCallback( p_playlist, "repeat", AllCallback, p_intf );
    var_AddCallback( p_playlist, "loop", AllCallback, p_intf );
    PL_UNLOCK;

    UpdateCaps( p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/

static void Close   ( vlc_object_t *p_this )
{
    intf_thread_t   *p_intf     = (intf_thread_t*) p_this;
    intf_sys_t      *p_sys      = p_intf->p_sys;
    playlist_t      *p_playlist = p_sys->p_playlist;
    input_thread_t  *p_input;

    var_DelCallback( p_playlist, "item-current", AllCallback, p_intf );
    var_DelCallback( p_playlist, "intf-change", AllCallback, p_intf );
    var_DelCallback( p_playlist, "playlist-item-append", AllCallback, p_intf );
    var_DelCallback( p_playlist, "playlist-item-deleted", AllCallback, p_intf );
    var_DelCallback( p_playlist, "random", AllCallback, p_intf );
    var_DelCallback( p_playlist, "repeat", AllCallback, p_intf );
    var_DelCallback( p_playlist, "loop", AllCallback, p_intf );

    p_input = playlist_CurrentInput( p_playlist );
    if ( p_input )
    {
        var_DelCallback( p_input, "state", AllCallback, p_intf );
        vlc_object_release( p_input );
    }

    dbus_connection_unref( p_sys->p_conn );

    // Free the events array
    for( int i = 0; i < vlc_array_count( p_sys->p_events ); i++ )
    {
        callback_info_t* info = vlc_array_item_at_index( p_sys->p_events, i );
        free( info );
    }
    vlc_mutex_destroy( &p_sys->lock );
    vlc_array_destroy( p_sys->p_events );
    free( p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/

static void Run          ( intf_thread_t *p_intf )
{
    for( ;; )
    {
        if( dbus_connection_get_dispatch_status(p_intf->p_sys->p_conn)
                                             == DBUS_DISPATCH_COMPLETE )
            msleep( INTF_IDLE_SLEEP );
        int canc = vlc_savecancel();
        dbus_connection_read_write_dispatch( p_intf->p_sys->p_conn, 0 );

        /* Get the list of events to process
         *
         * We can't keep the lock on p_intf->p_sys->p_events, else we risk a
         * deadlock:
         * The signal functions could lock mutex X while p_events is locked;
         * While some other function in vlc (playlist) might lock mutex X
         * and then set a variable which would call AllCallback(), which itself
         * needs to lock p_events to add a new event.
         */
        vlc_mutex_lock( &p_intf->p_sys->lock );
        int i_events = vlc_array_count( p_intf->p_sys->p_events );
        callback_info_t* info[i_events];
        for( int i = i_events - 1; i >= 0; i-- )
        {
            info[i] = vlc_array_item_at_index( p_intf->p_sys->p_events, i );
            vlc_array_remove( p_intf->p_sys->p_events, i );
        }
        vlc_mutex_unlock( &p_intf->p_sys->lock );

        for( int i = 0; i < i_events; i++ )
        {
            switch( info[i]->signal )
            {
            case SIGNAL_ITEM_CURRENT:
                TrackChange( p_intf );
                break;
            case SIGNAL_INTF_CHANGE:
            case SIGNAL_PLAYLIST_ITEM_APPEND:
            case SIGNAL_PLAYLIST_ITEM_DELETED:
                TrackListChangeEmit( p_intf, info[i]->signal, info[i]->i_node );
                break;
            case SIGNAL_RANDOM:
            case SIGNAL_REPEAT:
            case SIGNAL_LOOP:
                StatusChangeEmit( p_intf );
                break;
            case SIGNAL_STATE:
                StateChange( p_intf, info[i]->i_input_state );
                break;
            default:
                assert(0);
            }
            free( info[i] );
        }
        vlc_restorecancel( canc );
    }
}


// Get all the callbacks
static int AllCallback( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void)p_this;
    (void)oldval;
    intf_thread_t *p_intf = (intf_thread_t*)p_data;

    callback_info_t *info = malloc( sizeof( callback_info_t ) );
    if( !info )
        return VLC_ENOMEM;

    // Wich event is it ?
    if( !strcmp( "item-current", psz_var ) )
        info->signal = SIGNAL_ITEM_CURRENT;
    else if( !strcmp( "intf-change", psz_var ) )
        info->signal = SIGNAL_INTF_CHANGE;
    else if( !strcmp( "playlist-item-append", psz_var ) )
    {
        info->signal = SIGNAL_PLAYLIST_ITEM_APPEND;
        info->i_node = ((playlist_add_t*)newval.p_address)->i_node;
    }
    else if( !strcmp( "playlist-item-deleted", psz_var ) )
        info->signal = SIGNAL_PLAYLIST_ITEM_DELETED;
    else if( !strcmp( "random", psz_var ) )
        info->signal = SIGNAL_RANDOM;
    else if( !strcmp( "repeat", psz_var ) )
        info->signal = SIGNAL_REPEAT;
    else if( !strcmp( "loop", psz_var ) )
        info->signal = SIGNAL_LOOP;
    else if( !strcmp( "state", psz_var ) )
    {
        info->signal = SIGNAL_STATE;
        info->i_input_state = newval.i_int;
    }
    else
        assert(0);

    // Append the event
    vlc_mutex_lock( &p_intf->p_sys->lock );
    vlc_array_append( p_intf->p_sys->p_events, info );
    vlc_mutex_unlock( &p_intf->p_sys->lock );
    return VLC_SUCCESS;
}

/******************************************************************************
 * CapsChange: player capabilities change signal
 *****************************************************************************/
DBUS_SIGNAL( CapsChangeSignal )
{
    SIGNAL_INIT( MPRIS_DBUS_PLAYER_PATH, "CapsChange" );
    OUT_ARGUMENTS;

    ADD_INT32( &((intf_thread_t*)p_data)->p_sys->i_caps );
    SIGNAL_SEND;
}

/******************************************************************************
 * TrackListChange: tracklist order / length change signal
 *****************************************************************************/
DBUS_SIGNAL( TrackListChangeSignal )
{ /* emit the new tracklist lengh */
    SIGNAL_INIT( MPRIS_DBUS_TRACKLIST_PATH, "TrackListChange");
    OUT_ARGUMENTS;

    /* XXX: locking */
    dbus_int32_t i_elements = ((intf_thread_t*)p_data)->p_sys->p_playlist->current.i_size;

    ADD_INT32( &i_elements );
    SIGNAL_SEND;
}

/*****************************************************************************
 * TrackListChangeEmit: Emits the TrackListChange signal
 *****************************************************************************/
/* FIXME: It is not called on tracklist reordering */
static int TrackListChangeEmit( intf_thread_t *p_intf, int signal, int i_node )
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

    UpdateCaps( p_intf );
    TrackListChangeSignal( p_intf->p_sys->p_conn, p_intf );
    return VLC_SUCCESS;
}
/*****************************************************************************
 * TrackChange: Playlist item change callback
 *****************************************************************************/

DBUS_SIGNAL( TrackChangeSignal )
{ /* emit the metadata of the new item */
    SIGNAL_INIT( MPRIS_DBUS_PLAYER_PATH, "TrackChange" );
    OUT_ARGUMENTS;

    input_item_t *p_item = (input_item_t*) p_data;
    GetInputMeta ( p_item, &args );

    SIGNAL_SEND;
}

/*****************************************************************************
 * StatusChange: Player status change signal
 *****************************************************************************/

DBUS_SIGNAL( StatusChangeSignal )
{ /* send the updated status info on the bus */
    SIGNAL_INIT( MPRIS_DBUS_PLAYER_PATH, "StatusChange" );
    OUT_ARGUMENTS;

    /* we're called from a callback of input_thread_t, so it can not be
     * destroyed before we return */
    MarshalStatus( (intf_thread_t*) p_data, &args );

    SIGNAL_SEND;
}

/*****************************************************************************
 * StateChange: callback on input "state"
 *****************************************************************************/
//static int StateChange( vlc_object_t *p_this, const char* psz_var,
//            vlc_value_t oldval, vlc_value_t newval, void *p_data )
static int StateChange( intf_thread_t *p_intf, int i_input_state )
{
    intf_sys_t          *p_sys      = p_intf->p_sys;
    playlist_t          *p_playlist = p_sys->p_playlist;
    input_thread_t      *p_input;
    input_item_t        *p_item;

    if( p_intf->p_sys->b_dead )
        return VLC_SUCCESS;

    UpdateCaps( p_intf );

    if( !p_sys->b_meta_read && i_input_state == PLAYING_S )
    {
        p_input = playlist_CurrentInput( p_playlist );
        if( p_input )
        {
            p_item = input_GetItem( p_input );
            if( p_item )
            {
                p_sys->b_meta_read = true;
                TrackChangeSignal( p_sys->p_conn, p_item );
            }
            vlc_object_release( p_input );
        }
    }

    if( i_input_state == PLAYING_S || i_input_state == PAUSE_S ||
        i_input_state == END_S )
    {
        StatusChangeSignal( p_sys->p_conn, p_intf );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * StatusChangeEmit: Emits the StatusChange signal
 *****************************************************************************/
static int StatusChangeEmit( intf_thread_t * p_intf )
{
    if( p_intf->p_sys->b_dead )
        return VLC_SUCCESS;

    UpdateCaps( p_intf );
    StatusChangeSignal( p_intf->p_sys->p_conn, p_intf );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * TrackChange: callback on playlist "item-current"
 *****************************************************************************/
static int TrackChange( intf_thread_t *p_intf )
{
    intf_sys_t          *p_sys      = p_intf->p_sys;
    playlist_t          *p_playlist = p_sys->p_playlist;
    input_thread_t      *p_input    = NULL;
    input_item_t        *p_item     = NULL;

    if( p_intf->p_sys->b_dead )
        return VLC_SUCCESS;

    p_sys->b_meta_read = false;

    p_input = playlist_CurrentInput( p_playlist );
    if( !p_input )
    {
        return VLC_SUCCESS;
    }

    p_item = input_GetItem( p_input );
    if( !p_item )
    {
        vlc_object_release( p_input );
        return VLC_EGENERIC;
    }

    if( input_item_IsPreparsed( p_item ) )
    {
        p_sys->b_meta_read = true;
        TrackChangeSignal( p_sys->p_conn, p_item );
    }

    var_AddCallback( p_input, "state", AllCallback, p_intf );

    vlc_object_release( p_input );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * UpdateCaps: update p_sys->i_caps
 * This function have to be called with the playlist unlocked
 ****************************************************************************/
static int UpdateCaps( intf_thread_t* p_intf )
{
    intf_sys_t* p_sys = p_intf->p_sys;
    dbus_int32_t i_caps = CAPS_CAN_HAS_TRACKLIST;
    playlist_t* p_playlist = p_sys->p_playlist;

    PL_LOCK;
    if( p_playlist->current.i_size > 0 )
        i_caps |= CAPS_CAN_PLAY | CAPS_CAN_GO_PREV | CAPS_CAN_GO_NEXT;
    PL_UNLOCK;

    input_thread_t* p_input = playlist_CurrentInput( p_playlist );
    if( p_input )
    {
        /* XXX: if UpdateCaps() is called too early, these are
         * unconditionnaly true */
        if( var_GetBool( p_input, "can-pause" ) )
            i_caps |= CAPS_CAN_PAUSE;
        if( var_GetBool( p_input, "can-seek" ) )
            i_caps |= CAPS_CAN_SEEK;
        vlc_object_release( p_input );
    }

    if( p_sys->b_meta_read )
        i_caps |= CAPS_CAN_PROVIDE_METADATA;

    if( i_caps != p_intf->p_sys->i_caps )
    {
        p_sys->i_caps = i_caps;
        CapsChangeSignal( p_intf->p_sys->p_conn, (vlc_object_t*)p_intf );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * GetInputMeta: Fill a DBusMessage with the given input item metadata
 *****************************************************************************/

#define ADD_META( entry, type, data ) \
    if( data ) { \
        dbus_message_iter_open_container( &dict, DBUS_TYPE_DICT_ENTRY, \
                NULL, &dict_entry ); \
        dbus_message_iter_append_basic( &dict_entry, DBUS_TYPE_STRING, \
                &ppsz_meta_items[entry] ); \
        dbus_message_iter_open_container( &dict_entry, DBUS_TYPE_VARIANT, \
                type##_AS_STRING, &variant ); \
        dbus_message_iter_append_basic( &variant, \
                type, \
                & data ); \
        dbus_message_iter_close_container( &dict_entry, &variant ); \
        dbus_message_iter_close_container( &dict, &dict_entry ); }

#define ADD_VLC_META_STRING( entry, item ) \
    { \
        char * psz = input_item_Get##item( p_input );\
        ADD_META( entry, DBUS_TYPE_STRING, \
                  psz ); \
        free( psz ); \
    }

static int GetInputMeta( input_item_t* p_input,
                        DBusMessageIter *args )
{
    DBusMessageIter dict, dict_entry, variant;
    /** The duration of the track can be expressed in second, milli-seconds and
        µ-seconds */
    dbus_int64_t i_mtime = input_item_GetDuration( p_input );
    dbus_uint32_t i_time = i_mtime / 1000000;
    dbus_int64_t i_length = i_mtime / 1000;

    const char* ppsz_meta_items[] =
    {
    /* Official MPRIS metas */
    "location", "title", "artist", "album", "tracknumber", "time", "mtime",
    "genre", "rating", "date", "arturl",
    "audio-bitrate", "audio-samplerate", "video-bitrate",
    /* VLC specifics metas */
    "audio-codec", "copyright", "description", "encodedby", "language", "length",
    "nowplaying", "publisher", "setting", "status", "trackid", "url",
    "video-codec"
    };

    dbus_message_iter_open_container( args, DBUS_TYPE_ARRAY, "{sv}", &dict );

    ADD_VLC_META_STRING( 0,  URI );
    ADD_VLC_META_STRING( 1,  Title );
    ADD_VLC_META_STRING( 2,  Artist );
    ADD_VLC_META_STRING( 3,  Album );
    ADD_VLC_META_STRING( 4,  TrackNum );
    ADD_META( 5, DBUS_TYPE_UINT32, i_time );
    ADD_META( 6, DBUS_TYPE_UINT32, i_mtime );
    ADD_VLC_META_STRING( 7,  Genre );
    ADD_VLC_META_STRING( 8,  Rating );
    ADD_VLC_META_STRING( 9,  Date );
    ADD_VLC_META_STRING( 10, ArtURL );

    ADD_VLC_META_STRING( 15, Copyright );
    ADD_VLC_META_STRING( 16, Description );
    ADD_VLC_META_STRING( 17, EncodedBy );
    ADD_VLC_META_STRING( 18, Language );
    ADD_META( 19, DBUS_TYPE_INT64, i_length );
    ADD_VLC_META_STRING( 20, NowPlaying );
    ADD_VLC_META_STRING( 21, Publisher );
    ADD_VLC_META_STRING( 22, Setting );
    ADD_VLC_META_STRING( 24, TrackID );
    ADD_VLC_META_STRING( 25, URL );

    vlc_mutex_lock( &p_input->lock );
    if( p_input->p_meta )
    {
        int i_status = vlc_meta_GetStatus( p_input->p_meta );
        ADD_META( 23, DBUS_TYPE_INT32, i_status );
    }
    vlc_mutex_unlock( &p_input->lock );

    dbus_message_iter_close_container( args, &dict );
    return VLC_SUCCESS;
}

#undef ADD_META
#undef ADD_VLC_META_STRING

/*****************************************************************************
 * MarshalStatus: Fill a DBusMessage with the current player status
 *****************************************************************************/

static int MarshalStatus( intf_thread_t* p_intf, DBusMessageIter* args )
{ /* This is NOT the right way to do that, it would be better to sore
     the status information in p_sys and update it on change, thus
     avoiding a long lock */

    DBusMessageIter status;
    dbus_int32_t i_state, i_random, i_repeat, i_loop;
    int i_val;
    playlist_t* p_playlist = p_intf->p_sys->p_playlist;
    input_thread_t* p_input = NULL;

    i_state = 2;

    p_input = playlist_CurrentInput( p_playlist );
    if( p_input )
    {
        i_val = var_GetInteger( p_input, "state" );
        if( i_val >= END_S )
            i_state = 2;
        else if( i_val == PAUSE_S )
            i_state = 1;
        else if( i_val <= PLAYING_S )
            i_state = 0;
        vlc_object_release( p_input );
    }

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
