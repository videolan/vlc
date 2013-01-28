/*****************************************************************************
 * dbus_common.h : Common header for D-Bus control modules
 *****************************************************************************
 * Copyright © 2006-2008 Rafaël Carré
 * Copyright © 2007-2010 Mirsal Ennaime
 * Copyright © 2009-2010 The VideoLAN team
 * Copyright © 2013      Alex Merry
 * $Id$
 *
 * Authors:    Mirsal Ennaime <mirsal dot ennaime at gmailcom>
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

#ifndef _VLC_DBUS_COMMON_H
#define _VLC_DBUS_COMMON_H

#include <vlc_common.h>
#include <vlc_interface.h>
#include <dbus/dbus.h>

#define DBUS_MPRIS_OBJECT_PATH "/org/mpris/MediaPlayer2"

/* MACROS */

#define INTF ((intf_thread_t *)p_this)
#define PL   (INTF->p_sys->p_playlist)

#define DBUS_METHOD( method_function ) \
    static DBusHandlerResult method_function \
            ( DBusConnection *p_conn, DBusMessage *p_from, void *p_this )

#define DBUS_SIGNAL( signal_function ) \
    static DBusHandlerResult signal_function \
            ( DBusConnection *p_conn, void *p_data )

#define REPLY_INIT \
    DBusMessage* p_msg = dbus_message_new_method_return( p_from ); \
    if( !p_msg ) return DBUS_HANDLER_RESULT_NEED_MEMORY; \

#define REPLY_SEND \
    if( !dbus_connection_send( p_conn, p_msg, NULL ) ) \
        return DBUS_HANDLER_RESULT_NEED_MEMORY; \
    dbus_connection_flush( p_conn ); \
    dbus_message_unref( p_msg ); \
    return DBUS_HANDLER_RESULT_HANDLED

#define SIGNAL_INIT( interface, path, signal ) \
    DBusMessage *p_msg = dbus_message_new_signal( path, \
        interface, signal ); \
    if( !p_msg ) return DBUS_HANDLER_RESULT_NEED_MEMORY; \

#define SIGNAL_SEND \
    if( !dbus_connection_send( p_conn, p_msg, NULL ) ) \
        return DBUS_HANDLER_RESULT_NEED_MEMORY; \
    dbus_message_unref( p_msg ); \
    dbus_connection_flush( p_conn ); \
    return DBUS_HANDLER_RESULT_HANDLED

#define OUT_ARGUMENTS \
    DBusMessageIter args; \
    dbus_message_iter_init_append( p_msg, &args )

#define DBUS_ADD( dbus_type, value ) \
    if( !dbus_message_iter_append_basic( &args, dbus_type, value ) ) \
        return DBUS_HANDLER_RESULT_NEED_MEMORY

#define ADD_STRING( s ) DBUS_ADD( DBUS_TYPE_STRING, s )
#define ADD_DOUBLE( d ) DBUS_ADD( DBUS_TYPE_DOUBLE, d )
#define ADD_BOOL( b ) DBUS_ADD( DBUS_TYPE_BOOLEAN, b )
#define ADD_INT32( i ) DBUS_ADD( DBUS_TYPE_INT32, i )
#define ADD_INT64( i ) DBUS_ADD( DBUS_TYPE_INT64, i )
#define ADD_BYTE( b ) DBUS_ADD( DBUS_TYPE_BYTE, b )

#define MPRIS_TRACKID_FORMAT "/org/videolan/vlc/playlist/%d"

struct intf_sys_t
{
    DBusConnection *p_conn;
    playlist_t     *p_playlist;
    bool            b_meta_read;
    dbus_int32_t    i_player_caps;
    dbus_int32_t    i_playing_state;
    bool            b_can_play;
    bool            b_dead;
    vlc_array_t    *p_events;
    vlc_array_t    *p_timeouts;
    vlc_array_t    *p_watches;
    int             p_pipe_fds[2];
    vlc_mutex_t     lock;
    vlc_thread_t    thread;
    input_thread_t *p_input;

    mtime_t         i_last_input_pos; /* Only access from input thread */
    mtime_t         i_last_input_pos_event; /* Same as above */
};

enum
{
    SIGNAL_NONE=0,
    SIGNAL_ITEM_CURRENT,
    SIGNAL_INTF_CHANGE,
    SIGNAL_PLAYLIST_ITEM_APPEND,
    SIGNAL_PLAYLIST_ITEM_DELETED,
    SIGNAL_INPUT_METADATA,
    SIGNAL_RANDOM,
    SIGNAL_REPEAT,
    SIGNAL_LOOP,
    SIGNAL_STATE,
    SIGNAL_RATE,
    SIGNAL_SEEK,
    SIGNAL_CAN_SEEK,
    SIGNAL_CAN_PAUSE,
    SIGNAL_VOLUME_CHANGE,
    SIGNAL_VOLUME_MUTED,
    SIGNAL_FULLSCREEN
};

enum
{
    PLAYBACK_STATE_INVALID = -1,
    PLAYBACK_STATE_PLAYING = 0,
    PLAYBACK_STATE_PAUSED  = 1,
    PLAYBACK_STATE_STOPPED = 2
};

int DemarshalSetPropertyValue( DBusMessage *p_msg, void *p_arg );
int GetInputMeta  ( input_item_t* p_input, DBusMessageIter *args );
int AddProperty ( intf_thread_t *p_intf,
                  DBusMessageIter *p_container,
                  const char* psz_property_name,
                  const char* psz_signature,
                  int (*pf_marshaller) (intf_thread_t*, DBusMessageIter*) );

#endif //dbus-common.h
