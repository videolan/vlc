/*****************************************************************************
 * telepathy.c : changes Telepathy Presence information using MissionControl
 *****************************************************************************
 * Copyright © 2007-2009 the VideoLAN team
 * $Id$
 *
 * Author: Rafaël Carré <funman@videoanorg>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_strings.h>                                /* str_format_meta */

#include <dbus/dbus.h>

#define FORMAT_DEFAULT "$a - $t"
/*****************************************************************************
 * intf_sys_t: description and status of log interface
 *****************************************************************************/
struct intf_sys_t
{
    char            *psz_format;
    DBusConnection  *p_conn;
    int             i_id;
    int             i_item_changes;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static int ItemChange( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );
static int StateChange( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int SendToTelepathy( intf_thread_t *, const char * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_shortname( "Telepathy" )
    set_description( N_("Telepathy \"Now Playing\" (MissionControl)") )

    add_obsolete_string( "telepathy-format")

    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    playlist_t *p_playlist;
    DBusError       error;

    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( !p_intf->p_sys )
        return VLC_ENOMEM;

    /* connect to the session bus */
    dbus_error_init( &error );
    p_intf->p_sys->p_conn = dbus_bus_get( DBUS_BUS_SESSION, &error );
    if( !p_intf->p_sys->p_conn )
    {
        msg_Err( p_this, "Failed to connect to the DBus session daemon: %s",
                error.message );
        dbus_error_free( &error );
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }

    p_intf->p_sys->psz_format = var_InheritString( p_intf, "input-title-format" );
    if( !p_intf->p_sys->psz_format )
    {
        p_intf->p_sys->psz_format = strdup( FORMAT_DEFAULT );
    }
    msg_Dbg( p_intf, "using format: %s", p_intf->p_sys->psz_format );

    p_intf->p_sys->i_id = -1;

    p_playlist = pl_Get( p_intf );
    var_AddCallback( p_playlist, "item-change", ItemChange, p_intf );
    var_AddCallback( p_playlist, "item-current", ItemChange, p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    playlist_t *p_playlist = pl_Get( p_this );
    input_thread_t *p_input = NULL;

    var_DelCallback( p_playlist, "item-change", ItemChange, p_intf );
    var_DelCallback( p_playlist, "item-current", ItemChange, p_intf );
    if( (p_input = playlist_CurrentInput( p_playlist )) )
    {
        var_DelCallback( p_input, "state", StateChange, p_intf );
        vlc_object_release( p_input );
    }

    /* Clears the Presence message ... else it looks like we're still playing
     * something although VLC (or the Telepathy plugin) is closed */

    /* Do not check for VLC_ENOMEM as we're closing */
    SendToTelepathy( p_intf, "" );

    /* we won't use the DBus connection anymore */
    dbus_connection_unref( p_intf->p_sys->p_conn );

    /* Destroy structure */
    free( p_intf->p_sys->psz_format );
    free( p_intf->p_sys );
}

/*****************************************************************************
 * ItemChange: Playlist item change callback
 *****************************************************************************/
static int ItemChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *param )
{
    VLC_UNUSED(oldval);
    intf_thread_t *p_intf = (intf_thread_t *)param;
    playlist_t* p_playlist = (playlist_t*) p_this;
    char *psz_buf = NULL;
    input_thread_t *p_input;
    input_item_t *p_item = newval.p_address;
    bool b_is_item_current = !strcmp( "item-current", psz_var );

    /* Don't update Telepathy presence each time an item has been preparsed */
    if( b_is_item_current )
    { /* stores the current input item id */
        p_intf->p_sys->i_id = p_item->i_id;
        p_intf->p_sys->i_item_changes = 0;
    }
    else
    {

        if( p_item->i_id != p_intf->p_sys->i_id ) /* "item-change" */
            return VLC_SUCCESS;
        /* Some variable bitrate inputs call "item-change callbacks each time
         * their length is updated, that is several times per second.
         * We'll limit the number of changes to 10 per input. */
        if( p_intf->p_sys->i_item_changes > 10 )
            return VLC_SUCCESS;
        p_intf->p_sys->i_item_changes++;
    }

    p_input = playlist_CurrentInput( p_playlist );

    if( !p_input ) return VLC_SUCCESS;

    if( p_input->b_dead || !input_GetItem(p_input)->psz_name )
    {
        vlc_object_release( p_input );
        /* Not playing anything ... */
        switch( SendToTelepathy( p_intf, "" ) )
        {
            case VLC_ENOMEM:
                return VLC_ENOMEM;
            default:
                return VLC_SUCCESS;
        }
    }

    if( b_is_item_current )
        var_AddCallback( p_input, "state", StateChange, p_intf );

    /* We format the string to be displayed */
    psz_buf = str_format_meta( (vlc_object_t*) p_intf,
            p_intf->p_sys->psz_format );

    /* We don't need the input anymore */
    vlc_object_release( p_input );

    if( SendToTelepathy( p_intf, psz_buf ) == VLC_ENOMEM )
    {
        free( psz_buf );
        return VLC_ENOMEM;
    }
    free( psz_buf );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * StateChange: State change callback
 *****************************************************************************/
static int StateChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *param )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_var); VLC_UNUSED(oldval);
    intf_thread_t *p_intf = (intf_thread_t *)param;
    if( newval.i_int >= END_S )
        return SendToTelepathy( p_intf, "" );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendToTelepathy
 *****************************************************************************/
static int SendToTelepathy( intf_thread_t *p_intf, const char *psz_msg )
{
    DBusConnection *p_conn;
    DBusMessage *p_msg;
    DBusMessage *p_reply;
    DBusMessageIter args;
    DBusError error;
    dbus_error_init( &error );
    dbus_uint32_t i_status;

    p_conn = p_intf->p_sys->p_conn;

    /* first we need to get the actual status */
    p_msg = dbus_message_new_method_call(
            "org.freedesktop.Telepathy.MissionControl",
           "/org/freedesktop/Telepathy/MissionControl",
            "org.freedesktop.Telepathy.MissionControl",
            "GetPresence" );
    if( !p_msg )
        return VLC_ENOMEM;

    p_reply = dbus_connection_send_with_reply_and_block( p_conn, p_msg,
        50, &error ); /* blocks 50ms maximum */

    if( dbus_error_is_set( &error ) )
        dbus_error_free( &error );

    dbus_message_unref( p_msg );
    if( p_reply == NULL )
    {   /* MC is not active, or too slow. Better luck next time? */
        return VLC_SUCCESS;
    }

    /* extract the status from the reply */
    if( dbus_message_get_args( p_reply, &error,
            DBUS_TYPE_UINT32, &i_status,
            DBUS_TYPE_INVALID ) == FALSE )
    {
        return VLC_ENOMEM;
    }

    p_msg = dbus_message_new_method_call(
            "org.freedesktop.Telepathy.MissionControl",
           "/org/freedesktop/Telepathy/MissionControl",
            "org.freedesktop.Telepathy.MissionControl",
            "SetPresence" );
    if( !p_msg )
        return VLC_ENOMEM;

    dbus_message_iter_init_append( p_msg, &args );

    /* first argument is the status */
    if( !dbus_message_iter_append_basic( &args, DBUS_TYPE_UINT32, &i_status ) )
    {
        dbus_message_unref( p_msg );
        return VLC_ENOMEM;
    }
    /* second argument is the message */
    if( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &psz_msg ) )
    {
        dbus_message_unref( p_msg );
        return VLC_ENOMEM;
    }


    if( !dbus_connection_send( p_conn, p_msg, NULL ) )
        return VLC_ENOMEM;

    dbus_connection_flush( p_conn );
    dbus_message_unref( p_msg );

    return VLC_SUCCESS;
}
