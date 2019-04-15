/*****************************************************************************
 * dbus_player.c : dbus control module (mpris v2.2) - Player object
 *****************************************************************************
 * Copyright © 2006-2011 Rafaël Carré
 * Copyright © 2007-2011 Mirsal Ennaime
 * Copyright © 2009-2011 The VideoLAN team
 * Copyright © 2013      Alex Merry
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
#include <vlc_interface.h>

#include <math.h>

#include "dbus_player.h"
#include "dbus_common.h"

static int
MarshalPosition( intf_thread_t *p_intf, DBusMessageIter *container )
{
    /* returns time in microseconds */
    dbus_int64_t i_pos;

    vlc_player_t *player = vlc_playlist_GetPlayer(p_intf->p_sys->playlist);
    vlc_player_Lock(player);
    i_pos = vlc_player_GetTime(player);
    vlc_player_Unlock(player);
    i_pos = i_pos == VLC_TICK_INVALID ? 0 : US_FROM_VLC_TICK(i_pos);

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_INT64, &i_pos ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

DBUS_METHOD( SetPosition )
{ /* set time in microseconds */

    REPLY_INIT;
    dbus_int64_t i_pos;
    const char *psz_trackid;
    ssize_t i_item_id;
    size_t i_id;

    DBusError error;
    dbus_error_init( &error );

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_OBJECT_PATH, &psz_trackid,
            DBUS_TYPE_INT64, &i_pos,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if( sscanf( psz_trackid, MPRIS_TRACKID_FORMAT, &i_id ) < 1 )
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    vlc_playlist_t *playlist = PL;
    vlc_playlist_Lock(playlist);
    i_item_id = vlc_playlist_GetCurrentIndex( playlist );
    if (i_item_id != -1 && (size_t)i_item_id == i_id)
    {
        vlc_player_t *player = vlc_playlist_GetPlayer(playlist);
        vlc_player_SetTime(player, VLC_TICK_FROM_US(i_pos));
    }
    vlc_playlist_Unlock(playlist);

    REPLY_SEND;
}

DBUS_METHOD( Seek )
{
    REPLY_INIT;
    dbus_int64_t i_step;

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

    vlc_player_t *player = vlc_playlist_GetPlayer(PL);
    vlc_player_Lock(player);
    vlc_player_JumpTime(player, VLC_TICK_FROM_US(i_step));
    vlc_player_Unlock(player);

    REPLY_SEND;
}

static int
MarshalVolume( intf_thread_t *p_intf, DBusMessageIter *container )
{
    vlc_player_t *player = vlc_playlist_GetPlayer(p_intf->p_sys->playlist);
    double vol = vlc_player_aout_GetVolume(player);
    if( vol < .0f )
        vol = .0f;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_DOUBLE, &vol ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

DBUS_METHOD( VolumeSet )
{
    REPLY_INIT;
    double d_dbus_vol;

    if( VLC_SUCCESS != DemarshalSetPropertyValue( p_from, &d_dbus_vol ) )
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    vlc_player_t *player = vlc_playlist_GetPlayer(PL);
    vlc_player_aout_SetVolume(player, fmaxf(d_dbus_vol, .0f));

    REPLY_SEND;
}

DBUS_METHOD( Next )
{ /* next playlist item */
    REPLY_INIT;
    vlc_playlist_t *playlist = PL;
    vlc_playlist_Lock(playlist);
    vlc_playlist_Next(playlist);
    vlc_playlist_Unlock(playlist);
    REPLY_SEND;
}

DBUS_METHOD( Prev )
{ /* previous playlist item */
    REPLY_INIT;
    vlc_playlist_t *playlist = PL;
    vlc_playlist_Lock(playlist);
    vlc_playlist_Prev(playlist);
    vlc_playlist_Unlock(playlist);
    REPLY_SEND;
}

DBUS_METHOD( Stop )
{ /* stop playing */
    REPLY_INIT;
    vlc_playlist_t *playlist = PL;
    vlc_playlist_Lock(playlist);
    vlc_playlist_Stop(playlist);
    vlc_playlist_Unlock(playlist);
    REPLY_SEND;
}

DBUS_METHOD( Play )
{
    REPLY_INIT;
    vlc_player_t *player = vlc_playlist_GetPlayer(PL);
    vlc_player_Lock(player);
    if (vlc_player_IsPaused(player))
        vlc_player_Resume(player);
    else
        vlc_player_Start(player);
    vlc_player_Unlock(player);
    REPLY_SEND;
}

DBUS_METHOD( Pause )
{
    REPLY_INIT;
    vlc_playlist_t *playlist = PL;
    vlc_playlist_Lock(playlist);
    vlc_playlist_Pause(playlist);
    vlc_playlist_Unlock(playlist);
    REPLY_SEND;
}

DBUS_METHOD( PlayPause )
{
    REPLY_INIT;
    vlc_player_t *player = vlc_playlist_GetPlayer(PL);
    vlc_player_Lock(player);
    vlc_player_TogglePause(player);
    vlc_player_Unlock(player);
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

    input_item_t *item = input_item_New(psz_mrl, NULL);
    if (!item)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    vlc_playlist_t *playlist = PL;
    vlc_playlist_Lock(playlist);
    size_t count = vlc_playlist_Count(playlist);
    vlc_playlist_InsertOne(playlist, count, item);
    vlc_playlist_PlayAt(playlist, count);
    vlc_playlist_Unlock(playlist);

    input_item_Release(item);

    REPLY_SEND;
}

static int
MarshalCanGoNext( intf_thread_t *p_intf, DBusMessageIter *container )
{
    vlc_playlist_t *playlist = p_intf->p_sys->playlist;
    vlc_playlist_Lock(playlist);
    size_t count = vlc_playlist_Count(playlist);
    ssize_t index = vlc_playlist_GetCurrentIndex(playlist);
    enum vlc_playlist_playback_repeat repeat_mode =
        vlc_playlist_GetPlaybackRepeat(playlist);
    vlc_playlist_Unlock(playlist);

    dbus_bool_t b_can_go_next =
        count != 0 &&
        ((index != -1 && (size_t)index < count - 1) ||
         repeat_mode != VLC_PLAYLIST_PLAYBACK_REPEAT_NONE);

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN,
                                         &b_can_go_next ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalCanGoPrevious( intf_thread_t *p_intf, DBusMessageIter *container )
{
    vlc_playlist_t *playlist = p_intf->p_sys->playlist;
    vlc_playlist_Lock(playlist);
    size_t count = vlc_playlist_Count(playlist);
    ssize_t index = vlc_playlist_GetCurrentIndex(playlist);
    enum vlc_playlist_playback_repeat repeat_mode =
        vlc_playlist_GetPlaybackRepeat(playlist);
    vlc_playlist_Unlock(playlist);

    dbus_bool_t b_can_go_previous =
        count != 0 &&
        (index > 0 || repeat_mode != VLC_PLAYLIST_PLAYBACK_REPEAT_NONE);

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN,
                                         &b_can_go_previous ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalCanPlay( intf_thread_t *p_intf, DBusMessageIter *container )
{
    vlc_playlist_t *playlist = p_intf->p_sys->playlist;
    vlc_playlist_Lock(playlist);
    dbus_bool_t b_can_play = vlc_playlist_Count(playlist) != 0;
    vlc_playlist_Unlock(playlist);

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN,
                                         &b_can_play ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalCanPause( intf_thread_t *p_intf, DBusMessageIter *container )
{
    vlc_player_t *player = vlc_playlist_GetPlayer(p_intf->p_sys->playlist);
    vlc_player_Lock(player);
    dbus_bool_t b_can_pause = vlc_player_CanPause(player);
    vlc_player_Unlock(player);

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
    vlc_player_t *player = vlc_playlist_GetPlayer(p_intf->p_sys->playlist);
    vlc_player_Lock(player);
    dbus_bool_t b_can_seek = vlc_player_CanSeek(player);
    vlc_player_Unlock(player);

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN,
                                         &b_can_seek ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalShuffle( intf_thread_t *p_intf, DBusMessageIter *container )
{
    vlc_playlist_t *playlist = p_intf->p_sys->playlist;
    vlc_playlist_Lock(playlist);
    enum vlc_playlist_playback_order order_mode =
        vlc_playlist_GetPlaybackOrder(playlist);
    vlc_playlist_Unlock(playlist);

    dbus_bool_t b_shuffle = order_mode == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
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

    vlc_playlist_t *playlist = PL;
    vlc_playlist_Lock(playlist);
    vlc_playlist_SetPlaybackOrder(playlist, b_shuffle == TRUE
            ? VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM
            : VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL);
    vlc_playlist_Unlock(playlist);

    REPLY_SEND;
}

static int
MarshalPlaybackStatus( intf_thread_t *p_intf, DBusMessageIter *container )
{
    vlc_player_t *player = vlc_playlist_GetPlayer(p_intf->p_sys->playlist);
    vlc_player_Lock(player);
    enum vlc_player_state state = vlc_player_GetState(player);
    vlc_player_Unlock(player);

    const char *psz_playback_status;
    switch (state)
    {
        case VLC_PLAYER_STATE_STARTED:
        case VLC_PLAYER_STATE_PLAYING:
            psz_playback_status = PLAYBACK_STATUS_PLAYING;
            break;
        case VLC_PLAYER_STATE_PAUSED:
            psz_playback_status = PLAYBACK_STATUS_PAUSED;
            break;
        default:
            psz_playback_status = PLAYBACK_STATUS_STOPPED;
    }

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_STRING,
                                         &psz_playback_status ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalRate( intf_thread_t *p_intf, DBusMessageIter *container )
{
    vlc_player_t *player = vlc_playlist_GetPlayer(p_intf->p_sys->playlist);
    vlc_player_Lock(player);
    double d_rate = vlc_player_GetRate(player);
    vlc_player_Unlock(player);

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

    vlc_player_t *player = vlc_playlist_GetPlayer(PL);
    vlc_player_Lock(player);
    vlc_player_ChangeRate(player, d_rate);
    vlc_player_Unlock(player);

    REPLY_SEND;
}

static int
MarshalMinimumRate( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    double d_min_rate = INPUT_RATE_MIN;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_DOUBLE, &d_min_rate ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalMaximumRate( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    double d_max_rate = INPUT_RATE_MAX;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_DOUBLE, &d_max_rate ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalLoopStatus( intf_thread_t *p_intf, DBusMessageIter *container )
{
    vlc_playlist_t *playlist = p_intf->p_sys->playlist;
    vlc_playlist_Lock(playlist);
    enum vlc_playlist_playback_repeat repeat_mode =
        vlc_playlist_GetPlaybackRepeat(playlist);
    vlc_playlist_Unlock(playlist);

    const char *psz_loop_status;
    switch (repeat_mode)
    {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
            psz_loop_status = LOOP_STATUS_PLAYLIST;
            break;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            psz_loop_status = LOOP_STATUS_TRACK;
            break;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_NONE:
            psz_loop_status = LOOP_STATUS_NONE;
            break;
        default:
            vlc_assert_unreachable();
    }

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

    enum vlc_playlist_playback_repeat repeat_mode;
    if (!strcmp(psz_loop_status, LOOP_STATUS_NONE))
        repeat_mode = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
    else if (!strcmp(psz_loop_status, LOOP_STATUS_TRACK))
        repeat_mode = VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT;
    else if (!strcmp(psz_loop_status, LOOP_STATUS_PLAYLIST))
        repeat_mode = VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;
    else
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    vlc_playlist_t *playlist = PL;
    vlc_playlist_Lock(playlist);
    vlc_playlist_SetPlaybackRepeat(playlist, repeat_mode);
    vlc_playlist_Unlock(playlist);

    REPLY_SEND;
}

static int
MarshalMetadata( intf_thread_t *p_intf, DBusMessageIter *container )
{
    int result = VLC_SUCCESS;
    vlc_playlist_t *playlist = p_intf->p_sys->playlist;
    vlc_playlist_Lock(playlist);
    ssize_t id = vlc_playlist_GetCurrentIndex(playlist);
    if(id != -1)
    {
        vlc_playlist_item_t *plitem = vlc_playlist_Get(playlist, id);
        result = GetInputMeta(playlist, plitem, container);
    }
    else
    {   // avoid breaking the type marshalling
        DBusMessageIter a;
        if( !dbus_message_iter_open_container( container, DBUS_TYPE_ARRAY,
                                               "{sv}", &a ) ||
            !dbus_message_iter_close_container( container, &a ) )
            result = VLC_ENOMEM;
    }
    vlc_playlist_Unlock(playlist);
    return result;
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
    vlc_player_t *player = vlc_playlist_GetPlayer(p_intf->p_sys->playlist);
    vlc_player_Lock(player);
    i_pos = vlc_player_GetTime(player);
    vlc_player_Unlock(player);
    i_pos = i_pos == VLC_TICK_INVALID ? 0 : US_FROM_VLC_TICK(i_pos);

    ADD_INT64( &i_pos );
    SIGNAL_SEND;
}

#define PROPERTY_MAPPING_BEGIN
#define PROPERTY_GET_FUNC( prop, signature ) \
    if( !strcmp( psz_property_name,  #prop ) ) { \
        if( !dbus_message_iter_open_container( &args, DBUS_TYPE_VARIANT, signature, &v ) ) \
            return DBUS_HANDLER_RESULT_NEED_MEMORY; \
        if( VLC_SUCCESS != Marshal##prop( p_this, &v ) ) { \
            dbus_message_iter_abandon_container( &args, &v ); \
            return DBUS_HANDLER_RESULT_NEED_MEMORY; \
        } \
        if( !dbus_message_iter_close_container( &args, &v ) ) \
            return DBUS_HANDLER_RESULT_NEED_MEMORY; \
    } else
#define PROPERTY_SET_FUNC( prop ) \
    if( !strcmp( psz_property_name,  #prop ) ) \
        return prop##Set( p_conn, p_from, p_this ); \
    else
#define PROPERTY_MAPPING_END return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

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
            { \
                for( ; ppsz_properties[i]; ++i ) free( ppsz_properties[i] ); \
                free( ppsz_properties ); \
                dbus_message_iter_abandon_container( &args, &changed_properties ); \
                return DBUS_HANDLER_RESULT_NEED_MEMORY; \
            } \
    }
#define PROPERTY_MAPPING_END else \
    { \
        for( ; ppsz_properties[i]; ++i ) free( ppsz_properties[i] ); \
        free( ppsz_properties ); \
        dbus_message_iter_abandon_container( &args, &changed_properties ); \
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED; \
    }

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

    SIGNAL_INIT( DBUS_INTERFACE_PROPERTIES,
                 DBUS_MPRIS_OBJECT_PATH,
                 "PropertiesChanged" );

    OUT_ARGUMENTS;
    ADD_STRING( &psz_interface_name );

    if( !dbus_message_iter_open_container( &args, DBUS_TYPE_ARRAY, "{sv}",
                                           &changed_properties ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    ppsz_properties = vlc_dictionary_all_keys( p_changed_properties );

    if( unlikely(!ppsz_properties) )
    {
        dbus_message_iter_abandon_container( &args, &changed_properties );
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    for( int i = 0; ppsz_properties[i]; i++ )
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

    free( ppsz_properties );

    if( !dbus_message_iter_close_container( &args, &changed_properties ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_open_container( &args, DBUS_TYPE_ARRAY, "s",
                                           &invalidated_properties ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_close_container( &args, &invalidated_properties ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

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
