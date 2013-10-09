/*****************************************************************************
 * dbus_player.c : dbus control module (mpris v1.0) - /Player object
 *****************************************************************************
 * Copyright © 2006-2011 Rafaël Carré
 * Copyright © 2007-2011 Mirsal Ennaime
 * Copyright © 2009-2011 The VideoLAN team
 * Copyright © 2013      Alex Merry
 * $Id$
 *
 * Authors:    Mirsal Ennaime <mirsal at mirsal fr>
 *             Rafaël Carré <funman at videolanorg>
 *             Alex Merry <dev at randomguy3 me uk>
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

#include <math.h>

#include "dbus_player.h"
#include "dbus_common.h"

static int
MarshalPosition( intf_thread_t *p_intf, DBusMessageIter *container )
{
    /* returns position in microseconds */
    dbus_int64_t i_pos;
    input_thread_t *p_input;
    p_input = playlist_CurrentInput( p_intf->p_sys->p_playlist );

    if( !p_input )
        i_pos = 0;

    else
    {
        i_pos = var_GetTime( p_input, "time" );
        vlc_object_release( p_input );
    }

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_INT64, &i_pos ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

DBUS_METHOD( SetPosition )
{ /* set position in microseconds */

    REPLY_INIT;
    dbus_int64_t i_pos;
    vlc_value_t position;
    char *psz_trackid, *psz_dbus_trackid;
    input_item_t *p_item;

    DBusError error;
    dbus_error_init( &error );

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_OBJECT_PATH, &psz_dbus_trackid,
            DBUS_TYPE_INT64, &i_pos,
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
        if( ( p_item = input_GetItem( p_input ) ) )
        {
            if( -1 == asprintf( &psz_trackid,
                                MPRIS_TRACKID_FORMAT,
                                p_item->i_id ) )
            {
                vlc_object_release( p_input );
                return DBUS_HANDLER_RESULT_NEED_MEMORY;
            }

            if( !strcmp( psz_trackid, psz_dbus_trackid ) )
            {
                position.i_time = (mtime_t) i_pos;
                var_Set( p_input, "time", position );
            }
            free( psz_trackid );
        }

        vlc_object_release( p_input );
    }


    REPLY_SEND;
}

DBUS_METHOD( Seek )
{
    REPLY_INIT;
    dbus_int64_t i_step;
    vlc_value_t  newpos;
    mtime_t      i_pos;

    DBusError error;
    dbus_error_init( &error );

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_INT64, &i_step,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    input_thread_t *p_input = playlist_CurrentInput( PL );
    if( p_input && var_GetBool( p_input, "can-seek" ) )
    {
        i_pos = var_GetTime( p_input, "time" );
        newpos.i_time = (mtime_t) i_step + i_pos;

        if( newpos.i_time < 0 )
            newpos.i_time = 0;

        var_Set( p_input, "time", newpos );
    }

    if( p_input )
        vlc_object_release( p_input );

    REPLY_SEND;
}

static int
MarshalVolume( intf_thread_t *p_intf, DBusMessageIter *container )
{
    float f_vol = playlist_VolumeGet( p_intf->p_sys->p_playlist );
    if( f_vol < 0.f )
        f_vol = 1.f; /* ? */

    double d_vol = f_vol;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_DOUBLE, &d_vol ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

DBUS_METHOD( VolumeSet )
{
    REPLY_INIT;
    double d_dbus_vol;

    if( VLC_SUCCESS != DemarshalSetPropertyValue( p_from, &d_dbus_vol ) )
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    playlist_VolumeSet( PL, fmaxf( d_dbus_vol, 0.f ) );

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

DBUS_METHOD( Play )
{
    REPLY_INIT;
    input_thread_t *p_input =  playlist_CurrentInput( PL );

    if( !p_input || var_GetInteger( p_input, "state" ) != PLAYING_S )
        playlist_Play( PL );

    if( p_input )
        vlc_object_release( p_input );

    REPLY_SEND;
}

DBUS_METHOD( Pause )
{
    REPLY_INIT;
    input_thread_t *p_input = playlist_CurrentInput( PL );

    if( p_input && var_GetInteger(p_input, "state") == PLAYING_S )
        playlist_Pause( PL );

    if( p_input )
        vlc_object_release( p_input );

    REPLY_SEND;
}

DBUS_METHOD( PlayPause )
{
    REPLY_INIT;
    input_thread_t *p_input = playlist_CurrentInput( PL );

    if( p_input && var_GetInteger(p_input, "state") == PLAYING_S )
        playlist_Pause( PL );
    else
        playlist_Play( PL );

    if( p_input )
        vlc_object_release( p_input );

    REPLY_SEND;
}

DBUS_METHOD( OpenUri )
{
    REPLY_INIT;

    char *psz_mrl;
    DBusError error;
    dbus_error_init( &error );

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_STRING, &psz_mrl,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    playlist_Add( PL, psz_mrl, NULL,
                  PLAYLIST_APPEND | PLAYLIST_GO,
                  PLAYLIST_END, true, false );

    REPLY_SEND;
}

static int
MarshalCanGoNext( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );

    dbus_bool_t b_can_go_next = TRUE;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN,
                                         &b_can_go_next ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalCanGoPrevious( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );

    dbus_bool_t b_can_go_previous = TRUE;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN,
                                         &b_can_go_previous ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalCanPlay( intf_thread_t *p_intf, DBusMessageIter *container )
{
    playlist_t *p_playlist = p_intf->p_sys->p_playlist;

    PL_LOCK;
    dbus_bool_t b_can_play = playlist_CurrentSize( p_playlist ) > 0;
    PL_UNLOCK;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN,
                                         &b_can_play ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalCanPause( intf_thread_t *p_intf, DBusMessageIter *container )
{
    dbus_bool_t b_can_pause = FALSE;
    input_thread_t *p_input;
    p_input = playlist_CurrentInput( p_intf->p_sys->p_playlist );

    if( p_input )
    {
        b_can_pause = var_GetBool( p_input, "can-pause" );
        vlc_object_release( p_input );
    }

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN,
                                         &b_can_pause ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalCanControl( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    dbus_bool_t b_can_control = TRUE;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN,
                                         &b_can_control ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalCanSeek( intf_thread_t *p_intf, DBusMessageIter *container )
{
    dbus_bool_t b_can_seek = FALSE;
    input_thread_t *p_input;
    p_input = playlist_CurrentInput( p_intf->p_sys->p_playlist );

    if( p_input )
    {
        b_can_seek = var_GetBool( p_input, "can-seek" );
        vlc_object_release( p_input );
    }

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN,
                                         &b_can_seek ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalShuffle( intf_thread_t *p_intf, DBusMessageIter *container )
{
    dbus_bool_t b_shuffle = var_GetBool( p_intf->p_sys->p_playlist, "random" );

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN,
                                         &b_shuffle ))
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

DBUS_METHOD( ShuffleSet )
{
    REPLY_INIT;
    dbus_bool_t b_shuffle;

    if( VLC_SUCCESS != DemarshalSetPropertyValue( p_from, &b_shuffle ) )
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    var_SetBool( PL, "random", ( b_shuffle == TRUE ) );

    REPLY_SEND;
}

static int
MarshalPlaybackStatus( intf_thread_t *p_intf, DBusMessageIter *container )
{
    input_thread_t *p_input;
    const char *psz_playback_status;

    if( ( p_input = playlist_CurrentInput( p_intf->p_sys->p_playlist ) ) )
    {
        switch( var_GetInteger( p_input, "state" ) )
        {
            case OPENING_S:
            case PLAYING_S:
                psz_playback_status = PLAYBACK_STATUS_PLAYING;
                break;
            case PAUSE_S:
                psz_playback_status = PLAYBACK_STATUS_PAUSED;
                break;
            default:
                psz_playback_status = PLAYBACK_STATUS_STOPPED;
        }

        vlc_object_release( (vlc_object_t*) p_input );
    }
    else
        psz_playback_status = PLAYBACK_STATUS_STOPPED;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_STRING,
                                         &psz_playback_status ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalRate( intf_thread_t *p_intf, DBusMessageIter *container )
{
    double d_rate;
    input_thread_t *p_input;

    if( ( p_input = playlist_CurrentInput( p_intf->p_sys->p_playlist ) ) )
    {
        d_rate = var_GetFloat( p_input, "rate" );
        vlc_object_release( (vlc_object_t*) p_input );
    }
    else
        d_rate = 1.0;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_DOUBLE,
                                         &d_rate ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

DBUS_METHOD( RateSet )
{
    REPLY_INIT;

    double d_rate;

    if( VLC_SUCCESS != DemarshalSetPropertyValue( p_from, &d_rate ) )
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    input_thread_t *p_input;
    if( ( p_input = playlist_CurrentInput( PL ) ) )
    {
        var_SetFloat( p_input, "rate", (float) d_rate );
        vlc_object_release( (vlc_object_t*) p_input );
    }
    else
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    REPLY_SEND;
}

static int
MarshalMinimumRate( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    double d_min_rate = (double) INPUT_RATE_MIN / INPUT_RATE_DEFAULT;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_DOUBLE, &d_min_rate ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalMaximumRate( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    double d_max_rate = (double) INPUT_RATE_MAX / INPUT_RATE_DEFAULT;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_DOUBLE, &d_max_rate ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalLoopStatus( intf_thread_t *p_intf, DBusMessageIter *container )
{
    const char *psz_loop_status;

    if( var_GetBool( p_intf->p_sys->p_playlist, "repeat" ) )
        psz_loop_status = LOOP_STATUS_TRACK;

    else if( var_GetBool( p_intf->p_sys->p_playlist, "loop" ) )
        psz_loop_status = LOOP_STATUS_PLAYLIST;

    else
        psz_loop_status = LOOP_STATUS_NONE;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_STRING,
                                         &psz_loop_status ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

DBUS_METHOD( LoopStatusSet )
{
    REPLY_INIT;
    char *psz_loop_status;

    if( VLC_SUCCESS != DemarshalSetPropertyValue( p_from, &psz_loop_status ) )
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if( !strcmp( psz_loop_status, LOOP_STATUS_NONE ) )
    {
        var_SetBool( PL, "loop",   FALSE );
        var_SetBool( PL, "repeat", FALSE );
    }
    else if( !strcmp( psz_loop_status, LOOP_STATUS_TRACK ) )
    {
        var_SetBool( PL, "loop",   FALSE );
        var_SetBool( PL, "repeat", TRUE  );
    }
    else if( !strcmp( psz_loop_status, LOOP_STATUS_PLAYLIST ) )
    {
        var_SetBool( PL, "loop",   TRUE );
        var_SetBool( PL, "repeat", FALSE  );
    }
    else
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    REPLY_SEND;
}

static int
MarshalMetadata( intf_thread_t *p_intf, DBusMessageIter *container )
{
    DBusMessageIter a;
    input_thread_t *p_input = NULL;
    input_item_t   *p_item  = NULL;

    if( ( p_input = playlist_CurrentInput( p_intf->p_sys->p_playlist ) ) )
    {
        p_item = input_GetItem( p_input );

        if( p_item )
        {
            int result = GetInputMeta( p_item, container );

            if (result != VLC_SUCCESS)
            {
                vlc_object_release( (vlc_object_t*) p_input );
                return result;
            }
        }

        vlc_object_release( (vlc_object_t*) p_input );
    }

    if (!p_item)
    {
        // avoid breaking the type marshalling
        if( !dbus_message_iter_open_container( container, DBUS_TYPE_ARRAY, "{sv}", &a ) ||
              !dbus_message_iter_close_container( container, &a ) )
            return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}


/******************************************************************************
 * Seeked: non-linear playback signal
 *****************************************************************************/
DBUS_SIGNAL( SeekedSignal )
{
    SIGNAL_INIT( DBUS_MPRIS_PLAYER_INTERFACE,
                 DBUS_MPRIS_OBJECT_PATH,
                 "Seeked" );

    OUT_ARGUMENTS;

    dbus_int64_t i_pos = 0;
    intf_thread_t *p_intf = (intf_thread_t*) p_data;
    input_thread_t *p_input = playlist_CurrentInput( p_intf->p_sys->p_playlist );

    if( p_input )
    {
        i_pos = var_GetTime( p_input, "time" );
        vlc_object_release( p_input );
    }

    ADD_INT64( &i_pos );
    SIGNAL_SEND;
}

#define PROPERTY_MAPPING_BEGIN if( 0 ) {}
#define PROPERTY_GET_FUNC( prop, signature ) \
    else if( !strcmp( psz_property_name,  #prop ) ) { \
        if( !dbus_message_iter_open_container( &args, DBUS_TYPE_VARIANT, signature, &v ) ) \
            return DBUS_HANDLER_RESULT_NEED_MEMORY; \
        if( VLC_SUCCESS != Marshal##prop( p_this, &v ) ) { \
            dbus_message_iter_abandon_container( &args, &v ); \
            return DBUS_HANDLER_RESULT_NEED_MEMORY; \
        } \
        if( !dbus_message_iter_close_container( &args, &v ) ) \
            return DBUS_HANDLER_RESULT_NEED_MEMORY; \
    }
#define PROPERTY_SET_FUNC( prop ) \
    else if( !strcmp( psz_property_name,  #prop ) ) { \
        return prop##Set( p_conn, p_from, p_this ); \
    }
#define PROPERTY_MAPPING_END else { return DBUS_HANDLER_RESULT_NOT_YET_HANDLED; }

DBUS_METHOD( GetProperty )
{
    DBusError error;

    char *psz_interface_name = NULL;
    char *psz_property_name  = NULL;

    dbus_error_init( &error );
    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_STRING, &psz_interface_name,
            DBUS_TYPE_STRING, &psz_property_name,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                                         error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    msg_Dbg( (vlc_object_t*) p_this, "Getting property %s",
                                     psz_property_name );

    if( strcmp( psz_interface_name, DBUS_MPRIS_PLAYER_INTERFACE ) ) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    REPLY_INIT;
    OUT_ARGUMENTS;
    DBusMessageIter v;

    PROPERTY_MAPPING_BEGIN
    PROPERTY_GET_FUNC( Metadata,       "a{sv}" )
    PROPERTY_GET_FUNC( Position,       "x" )
    PROPERTY_GET_FUNC( PlaybackStatus, "s" )
    PROPERTY_GET_FUNC( LoopStatus,     "s" )
    PROPERTY_GET_FUNC( Shuffle,        "b" )
    PROPERTY_GET_FUNC( Volume,         "d" )
    PROPERTY_GET_FUNC( Rate,           "d" )
    PROPERTY_GET_FUNC( MinimumRate,    "d" )
    PROPERTY_GET_FUNC( MaximumRate,    "d" )
    PROPERTY_GET_FUNC( CanControl,     "b" )
    PROPERTY_GET_FUNC( CanPlay,        "b" )
    PROPERTY_GET_FUNC( CanGoNext,      "b" )
    PROPERTY_GET_FUNC( CanGoPrevious,  "b" )
    PROPERTY_GET_FUNC( CanPause,       "b" )
    PROPERTY_GET_FUNC( CanSeek,        "b" )
    PROPERTY_MAPPING_END

    REPLY_SEND;
}

DBUS_METHOD( SetProperty )
{
    DBusError error;

    char *psz_interface_name = NULL;
    char *psz_property_name  = NULL;

    dbus_error_init( &error );
    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_STRING, &psz_interface_name,
            DBUS_TYPE_STRING, &psz_property_name,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                                         error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    PROPERTY_MAPPING_BEGIN
    PROPERTY_SET_FUNC( LoopStatus )
    PROPERTY_SET_FUNC( Shuffle )
    PROPERTY_SET_FUNC( Volume )
    PROPERTY_SET_FUNC( Rate )
    PROPERTY_MAPPING_END
}

#undef PROPERTY_MAPPING_BEGIN
#undef PROPERTY_GET_FUNC
#undef PROPERTY_SET_FUNC
#undef PROPERTY_MAPPING_END

#define ADD_PROPERTY( prop, signature ) \
    if( VLC_SUCCESS != AddProperty( (intf_thread_t*) p_this, \
                &dict, #prop, signature, Marshal##prop ) ) { \
        dbus_message_iter_abandon_container( &args, &dict ); \
        return VLC_ENOMEM; \
    }

DBUS_METHOD( GetAllProperties )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusError error;
    DBusMessageIter dict;

    char *const psz_interface_name = NULL;

    dbus_error_init( &error );
    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_STRING, &psz_interface_name,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                                         error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    msg_Dbg( (vlc_object_t*) p_this, "Getting All properties" );

    if( !dbus_message_iter_open_container( &args, DBUS_TYPE_ARRAY, "{sv}", &dict ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    ADD_PROPERTY ( Metadata,       "a{sv}" );
    ADD_PROPERTY ( Position,       "x"     );
    ADD_PROPERTY ( PlaybackStatus, "s"     );
    ADD_PROPERTY ( LoopStatus,     "s"     );
    ADD_PROPERTY ( Shuffle,        "b"     );
    ADD_PROPERTY ( Volume,         "d"     );
    ADD_PROPERTY ( Rate,           "d"     );
    ADD_PROPERTY ( MinimumRate,    "d"     );
    ADD_PROPERTY ( MaximumRate,    "d"     );
    ADD_PROPERTY ( CanControl,     "b"     );
    ADD_PROPERTY ( CanPlay,        "b"     );
    ADD_PROPERTY ( CanGoNext,      "b"     );
    ADD_PROPERTY ( CanGoPrevious,  "b"     );
    ADD_PROPERTY ( CanPause,       "b"     );
    ADD_PROPERTY ( CanSeek,        "b"     );

    if( !dbus_message_iter_close_container( &args, &dict ))
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    REPLY_SEND;
}

#undef ADD_PROPERTY

#define METHOD_FUNC( interface, method, function ) \
    else if( dbus_message_is_method_call( p_from, interface, method ) )\
        return function( p_conn, p_from, p_this )

DBusHandlerResult
handle_player ( DBusConnection *p_conn, DBusMessage *p_from, void *p_this )
{
    if(0);
    METHOD_FUNC( DBUS_INTERFACE_PROPERTIES,   "Get",        GetProperty );
    METHOD_FUNC( DBUS_INTERFACE_PROPERTIES,   "Set",        SetProperty );
    METHOD_FUNC( DBUS_INTERFACE_PROPERTIES,   "GetAll",     GetAllProperties );

    /* here D-Bus method names are associated to an handler */

    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "Previous",     Prev );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "Next",         Next );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "Stop",         Stop );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "Seek",         Seek );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "Play",         Play );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "Pause",        Pause );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "PlayPause",    PlayPause );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "OpenUri",      OpenUri );
    METHOD_FUNC( DBUS_MPRIS_PLAYER_INTERFACE, "SetPosition",  SetPosition );

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

#undef METHOD_FUNC

/*****************************************************************************
 * SeekedEmit: Emits the Seeked signal
 *****************************************************************************/
int SeekedEmit( intf_thread_t * p_intf )
{
    if( p_intf->p_sys->b_dead )
        return VLC_SUCCESS;

    SeekedSignal( p_intf->p_sys->p_conn, p_intf );
    return VLC_SUCCESS;
}

#define PROPERTY_MAPPING_BEGIN if( 0 ) {}
#define PROPERTY_ENTRY( prop, signature ) \
    else if( !strcmp( ppsz_properties[i], #prop ) ) \
    { \
        if( VLC_SUCCESS != AddProperty( (intf_thread_t*) p_intf, \
                    &changed_properties, #prop, signature, Marshal##prop ) ) \
            return DBUS_HANDLER_RESULT_NEED_MEMORY; \
    }
#define PROPERTY_MAPPING_END else { return DBUS_HANDLER_RESULT_NOT_YET_HANDLED; }

/**
 * PropertiesChangedSignal() synthetizes and sends the
 * org.freedesktop.DBus.Properties.PropertiesChanged signal
 */
static DBusHandlerResult
PropertiesChangedSignal( intf_thread_t    *p_intf,
                         vlc_dictionary_t *p_changed_properties )
{
    DBusConnection  *p_conn = p_intf->p_sys->p_conn;
    DBusMessageIter changed_properties, invalidated_properties;
    const char *psz_interface_name = DBUS_MPRIS_PLAYER_INTERFACE;
    char **ppsz_properties = NULL;
    int i_properties = 0;

    SIGNAL_INIT( DBUS_INTERFACE_PROPERTIES,
                 DBUS_MPRIS_OBJECT_PATH,
                 "PropertiesChanged" );

    OUT_ARGUMENTS;
    ADD_STRING( &psz_interface_name );

    if( !dbus_message_iter_open_container( &args, DBUS_TYPE_ARRAY, "{sv}",
                                           &changed_properties ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    i_properties = vlc_dictionary_keys_count( p_changed_properties );
    ppsz_properties = vlc_dictionary_all_keys( p_changed_properties );

    if( unlikely(!ppsz_properties) )
    {
        dbus_message_iter_abandon_container( &args, &changed_properties );
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    for( int i = 0; i < i_properties; i++ )
    {
        PROPERTY_MAPPING_BEGIN
        PROPERTY_ENTRY( Metadata,       "a{sv}" )
        PROPERTY_ENTRY( PlaybackStatus, "s"     )
        PROPERTY_ENTRY( LoopStatus,     "s"     )
        PROPERTY_ENTRY( Rate,           "d"     )
        PROPERTY_ENTRY( Shuffle,        "b"     )
        PROPERTY_ENTRY( Volume,         "d"     )
        PROPERTY_ENTRY( CanSeek,        "b"     )
        PROPERTY_ENTRY( CanPlay,        "b"     )
        PROPERTY_ENTRY( CanPause,       "b"     )
        PROPERTY_MAPPING_END

        free( ppsz_properties[i] );
    }

    if( !dbus_message_iter_close_container( &args, &changed_properties ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_open_container( &args, DBUS_TYPE_ARRAY, "s",
                                           &invalidated_properties ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_close_container( &args, &invalidated_properties ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    free( ppsz_properties );

    SIGNAL_SEND;
}

#undef PROPERTY_MAPPING_BEGIN
#undef PROPERTY_ADD
#undef PROPERTY_MAPPING_END

/*****************************************************************************
 * PropertiesChangedEmit: Emits the Seeked signal
 *****************************************************************************/
int PlayerPropertiesChangedEmit( intf_thread_t    * p_intf,
                                 vlc_dictionary_t * p_changed_properties )
{
    if( p_intf->p_sys->b_dead )
        return VLC_SUCCESS;

    PropertiesChangedSignal( p_intf, p_changed_properties );
    return VLC_SUCCESS;
}
