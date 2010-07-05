/*****************************************************************************
 * dbus.c : D-Bus control interface
 *****************************************************************************
 * Copyright © 2006-2008 Rafaël Carré
 * Copyright © 2007-2010 Mirsal Ennaime
 * Copyright © 2009-2010 The VideoLAN team
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
#include "dbus_common.h"
#include "dbus_root.h"
#include "dbus_player.h"
#include "dbus_tracklist.h"

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>

#include <assert.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void Run     ( intf_thread_t * );

static int StateChange( intf_thread_t *, int );
static int TrackChange( intf_thread_t * );
static int AllCallback( vlc_object_t*, const char*, vlc_value_t, vlc_value_t, void* );

typedef struct
{
    int signal;
    int i_node;
    int i_input_state;
} callback_info_t;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DBUS_UNIQUE_TEXT N_("Unique DBUS service id (org.mpris.vlc-<pid>)")
#define DBUS_UNIQUE_LONGTEXT N_( \
    "Use a unique dbus service id to identify this VLC instance on the DBUS bus. " \
    "The process identifier (PID) is added to the service name: org.mpris.vlc-<pid>" )

vlc_module_begin ()
    set_shortname( N_("dbus"))
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_description( N_("D-Bus control interface") )
    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
    add_bool( "dbus-unique-service-id", false, NULL,
              DBUS_UNIQUE_TEXT, DBUS_UNIQUE_LONGTEXT, true )
vlc_module_end ()

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
    char            *psz_service_name = NULL;

    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->b_meta_read = false;
    p_sys->i_caps = CAPS_NONE;
    p_sys->b_dead = false;
    p_sys->p_input = NULL;

    p_sys->b_unique = var_CreateGetBool( p_intf, "dbus-unique-service-id" );
    if( p_sys->b_unique )
    {
        if( asprintf( &psz_service_name, "%s-%d",
            DBUS_MPRIS_BUS_NAME, getpid() ) < 0 )
        {
            free( p_sys );
            return VLC_ENOMEM;
        }
    }
    else
    {
        psz_service_name = strdup(DBUS_MPRIS_BUS_NAME);
    }

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
    dbus_bus_request_name( p_conn, psz_service_name, 0, &error );
    if( dbus_error_is_set( &error ) )
    {
        msg_Err( p_this, "Error requesting service %s: %s",
                 psz_service_name, error.message );
        dbus_error_free( &error );
        free( psz_service_name );
        free( p_sys );
        return VLC_EGENERIC;
    }
    msg_Info( p_intf, "listening on dbus as: %s", psz_service_name );
    free( psz_service_name );

    /* we register the objects */
    dbus_connection_register_object_path( p_conn, DBUS_MPRIS_ROOT_PATH,
            &dbus_mpris_root_vtable, p_this );
    dbus_connection_register_object_path( p_conn, DBUS_MPRIS_PLAYER_PATH,
            &dbus_mpris_player_vtable, p_this );
    dbus_connection_register_object_path( p_conn, DBUS_MPRIS_TRACKLIST_PATH,
            &dbus_mpris_tracklist_vtable, p_this );

    dbus_connection_flush( p_conn );

    p_intf->pf_run = Run;
    p_intf->p_sys = p_sys;
    p_sys->p_conn = p_conn;
    p_sys->p_events = vlc_array_new();
    vlc_mutex_init( &p_sys->lock );

    p_playlist = pl_Get( p_intf );
    p_sys->p_playlist = p_playlist;

    var_AddCallback( p_playlist, "item-current", AllCallback, p_intf );
    var_AddCallback( p_playlist, "intf-change", AllCallback, p_intf );
    var_AddCallback( p_playlist, "playlist-item-append", AllCallback, p_intf );
    var_AddCallback( p_playlist, "playlist-item-deleted", AllCallback, p_intf );
    var_AddCallback( p_playlist, "random", AllCallback, p_intf );
    var_AddCallback( p_playlist, "repeat", AllCallback, p_intf );
    var_AddCallback( p_playlist, "loop", AllCallback, p_intf );

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

    var_DelCallback( p_playlist, "item-current", AllCallback, p_intf );
    var_DelCallback( p_playlist, "intf-change", AllCallback, p_intf );
    var_DelCallback( p_playlist, "playlist-item-append", AllCallback, p_intf );
    var_DelCallback( p_playlist, "playlist-item-deleted", AllCallback, p_intf );
    var_DelCallback( p_playlist, "random", AllCallback, p_intf );
    var_DelCallback( p_playlist, "repeat", AllCallback, p_intf );
    var_DelCallback( p_playlist, "loop", AllCallback, p_intf );

    if( p_sys->p_input )
    {
        var_DelCallback( p_sys->p_input, "state", AllCallback, p_intf );
        vlc_object_release( p_sys->p_input );
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

/*****************************************************************************
 * UpdateCaps: update p_sys->i_caps
 * This function have to be called with the playlist unlocked
 ****************************************************************************/
int UpdateCaps( intf_thread_t* p_intf )
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
        CapsChangeEmit( p_intf );
    }

    return VLC_SUCCESS;
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
                TrackChangeEmit( p_intf, p_item );
            }
            vlc_object_release( p_input );
        }
    }

    if( i_input_state == PLAYING_S || i_input_state == PAUSE_S ||
        i_input_state == END_S )
    {
        StatusChangeEmit( p_intf );
    }

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

    if( p_sys->p_input )
    {
        var_DelCallback( p_sys->p_input, "state", AllCallback, p_intf );
        vlc_object_release( p_sys->p_input );
        p_sys->p_input = NULL;
    }

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
        TrackChangeEmit( p_intf, p_item );
    }

    p_sys->p_input = p_input;
    var_AddCallback( p_input, "state", AllCallback, p_intf );

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

int GetInputMeta( input_item_t* p_input,
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


