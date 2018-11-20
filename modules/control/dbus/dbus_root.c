/*****************************************************************************
 * dbus_root.c : dbus control module (mpris v2.2) - Root object
 *****************************************************************************
 * Copyright © 2006-2008 Rafaël Carré
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
#include <vlc_vout.h>
#include <vlc_plugin.h>

#include <unistd.h>
#include <limits.h>

#include "dbus_root.h"
#include "dbus_common.h"

static const char ppsz_supported_uri_schemes[][9] = {
    "file", "http", "https", "rtsp", "ftp", "mtp", "smb",
    "mms", "mmsu", "mmst", "mmsh", "unsv", "itpc", "icyx", "rtmp", "rtp",
    "dccp", "dvd", "vcd"
};

static const char ppsz_supported_mime_types[][26] = {
    "audio/mpeg", "audio/x-mpeg",
    "video/mpeg", "video/x-mpeg",
    "video/mpeg-system", "video/x-mpeg-system",
    "video/mp4",
    "audio/mp4",
    "video/x-msvideo",
    "video/quicktime",
    "application/ogg", "application/x-ogg",
    "video/x-ms-asf",  "video/x-ms-asf-plugin",
    "application/x-mplayer2",
    "video/x-ms-wmv",
    "video/x-google-vlc-plugin",
    "audio/wav", "audio/x-wav",
    "audio/3gpp",
    "video/3gpp",
    "audio/3gpp2",
    "video/3gpp2",
    "video/divx",
    "video/flv", "video/x-flv",
    "video/x-matroska",
    "audio/x-matroska",
    "application/xspf+xml"
};

static int
MarshalIdentity( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    const char *psz_id = _("VLC media player");

    if (!dbus_message_iter_append_basic( container, DBUS_TYPE_STRING, &psz_id ))
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalCanSetFullscreen( intf_thread_t *p_intf, DBusMessageIter *container )
{ VLC_UNUSED(p_intf);
    dbus_bool_t b_ret = TRUE;
    if (!dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN, &b_ret ))
        return VLC_ENOMEM;
    return VLC_SUCCESS;
}

static int
MarshalFullscreen( intf_thread_t *p_intf, DBusMessageIter *container )
{
    dbus_bool_t b_fullscreen;
    vlc_player_t *player = vlc_playlist_GetPlayer(p_intf->p_sys->playlist);
    b_fullscreen = vlc_player_vout_IsFullscreen(player);
    if (!dbus_message_iter_append_basic( container,
            DBUS_TYPE_BOOLEAN, &b_fullscreen ))
        return VLC_ENOMEM;
    return VLC_SUCCESS;
}

DBUS_METHOD( FullscreenSet )
{
    REPLY_INIT;
    dbus_bool_t b_fullscreen;

    if( VLC_SUCCESS != DemarshalSetPropertyValue( p_from, &b_fullscreen ) )
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    vlc_player_t *player = vlc_playlist_GetPlayer(PL);
    vlc_player_vout_SetFullscreen(player, b_fullscreen);

    REPLY_SEND;
}

static int
MarshalCanQuit( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    const dbus_bool_t b_ret = TRUE;

    if (!dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN, &b_ret ))
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalCanRaise( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    const dbus_bool_t b_ret = TRUE;

    if (!dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN, &b_ret ))
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalHasTrackList( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    const dbus_bool_t b_ret = TRUE;

    if (!dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN, &b_ret ))
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalDesktopEntry( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    const char* psz_ret = PACKAGE;

    if (!dbus_message_iter_append_basic( container, DBUS_TYPE_STRING, &psz_ret ))
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalSupportedMimeTypes( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    DBusMessageIter ret;

    size_t i_len = sizeof( ppsz_supported_mime_types ) /
        sizeof( *ppsz_supported_mime_types );

    if( !dbus_message_iter_open_container( container,
                                           DBUS_TYPE_ARRAY, "s",
                                           &ret ) )
        return VLC_ENOMEM;

    for( size_t i = 0; i < i_len; ++i )
    {
        const char* const psz_mime_type = ppsz_supported_mime_types[i];

        if( !dbus_message_iter_append_basic( &ret, DBUS_TYPE_STRING,
                                             &psz_mime_type ) )
            return VLC_ENOMEM;
    }

    if( !dbus_message_iter_close_container( container, &ret ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalSupportedUriSchemes( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    DBusMessageIter ret;

    size_t i_len = sizeof( ppsz_supported_uri_schemes ) /
        sizeof( *ppsz_supported_uri_schemes );

    if( !dbus_message_iter_open_container( container,
                                           DBUS_TYPE_ARRAY, "s",
                                           &ret ) )
        return VLC_ENOMEM;

    for( size_t i = 0; i < i_len; ++i )
    {
        const char* const psz_scheme = ppsz_supported_uri_schemes[i];

        if( !dbus_message_iter_append_basic( &ret, DBUS_TYPE_STRING,
                                             &psz_scheme ) )
            return VLC_ENOMEM;
    }

    if( !dbus_message_iter_close_container( container, &ret ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

DBUS_METHOD( Quit )
{ /* exits vlc */
    REPLY_INIT;
    libvlc_Quit(vlc_object_instance(INTF));
    REPLY_SEND;
}

DBUS_METHOD( Raise )
{/* shows vlc's main window */
    REPLY_INIT;
    var_TriggerCallback(vlc_object_instance(INTF), "intf-show" );
    REPLY_SEND;
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

    if( strcmp( psz_interface_name, DBUS_MPRIS_ROOT_INTERFACE ) ) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    REPLY_INIT;
    OUT_ARGUMENTS;
    DBusMessageIter v;

    PROPERTY_MAPPING_BEGIN
    PROPERTY_GET_FUNC( Identity,            "s" )
    PROPERTY_GET_FUNC( CanSetFullscreen,    "b" )
    PROPERTY_GET_FUNC( Fullscreen,          "b" )
    PROPERTY_GET_FUNC( CanQuit,             "b" )
    PROPERTY_GET_FUNC( CanRaise,            "b" )
    PROPERTY_GET_FUNC( HasTrackList,        "b" )
    PROPERTY_GET_FUNC( DesktopEntry,        "s" )
    PROPERTY_GET_FUNC( SupportedMimeTypes,  "as" )
    PROPERTY_GET_FUNC( SupportedUriSchemes, "as" )
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
    PROPERTY_SET_FUNC( Fullscreen )
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

    ADD_PROPERTY( Identity,            "s"  );
    ADD_PROPERTY( DesktopEntry,        "s"  );
    ADD_PROPERTY( SupportedMimeTypes,  "as" );
    ADD_PROPERTY( SupportedUriSchemes, "as" );
    ADD_PROPERTY( HasTrackList,        "b"  );
    ADD_PROPERTY( CanQuit,             "b"  );
    ADD_PROPERTY( CanSetFullscreen,    "b"  );
    ADD_PROPERTY( Fullscreen,          "b"  );
    ADD_PROPERTY( CanRaise,            "b"  );

    if( !dbus_message_iter_close_container( &args, &dict ))
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    REPLY_SEND;
}

#undef ADD_PROPERTY

#define METHOD_MAPPING_BEGIN if( 0 ) {}
#define METHOD_FUNC( interface, method, function ) \
    else if( dbus_message_is_method_call( p_from, interface, method ) )\
        return function( p_conn, p_from, p_this )
#define METHOD_MAPPING_END return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

DBusHandlerResult
handle_root ( DBusConnection *p_conn, DBusMessage *p_from, void *p_this )
{
    METHOD_MAPPING_BEGIN
    METHOD_FUNC( DBUS_INTERFACE_PROPERTIES, "Get",          GetProperty );
    METHOD_FUNC( DBUS_INTERFACE_PROPERTIES, "Set",          SetProperty );
    METHOD_FUNC( DBUS_INTERFACE_PROPERTIES, "GetAll",       GetAllProperties );
    METHOD_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "Quit",         Quit );
    METHOD_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "Raise",        Raise );
    METHOD_MAPPING_END
}

#undef METHOD_MAPPING_BEGIN
#undef METHOD_FUNC
#undef METHOD_MAPPING_END
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
    const char *psz_interface_name = DBUS_MPRIS_ROOT_INTERFACE;

    SIGNAL_INIT( DBUS_INTERFACE_PROPERTIES,
                 DBUS_MPRIS_OBJECT_PATH,
                 "PropertiesChanged" );

    OUT_ARGUMENTS;
    ADD_STRING( &psz_interface_name );

    if( !dbus_message_iter_open_container( &args, DBUS_TYPE_ARRAY, "{sv}",
                                           &changed_properties ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( vlc_dictionary_has_key( p_changed_properties, "Fullscreen" ) )
    {
        if( AddProperty( p_intf, &changed_properties, "Fullscreen", "b",
                     MarshalFullscreen ) != VLC_SUCCESS )
        {
            dbus_message_iter_abandon_container( &args, &changed_properties );
            return DBUS_HANDLER_RESULT_NEED_MEMORY;
        }
    }

    if( !dbus_message_iter_close_container( &args, &changed_properties ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_open_container( &args, DBUS_TYPE_ARRAY, "s",
                                           &invalidated_properties ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_close_container( &args, &invalidated_properties ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    SIGNAL_SEND;
}

/*****************************************************************************
 * RootPropertiesChangedEmit: Emits the Seeked signal
 *****************************************************************************/
int RootPropertiesChangedEmit( intf_thread_t    *p_intf,
                               vlc_dictionary_t *p_changed_properties )
{
    if( p_intf->p_sys->b_dead )
        return VLC_SUCCESS;

    PropertiesChangedSignal( p_intf, p_changed_properties );
    return VLC_SUCCESS;
}
