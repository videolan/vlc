/*****************************************************************************
 * dbus_root.c : dbus control module (mpris v1.0) - root object
 *****************************************************************************
 * Copyright © 2006-2008 Rafaël Carré
 * Copyright © 2007-2011 Mirsal Ennaime
 * Copyright © 2009-2011 The VideoLAN team
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
#include <vlc_interface.h>

#include <unistd.h>
#include <limits.h>

#include "dbus_root.h"
#include "dbus_common.h"

static const char const ppsz_supported_uri_schemes[][9] = {
    "file", "http", "https", "rtsp", "realrtsp", "pnm", "ftp", "mtp", "smb",
    "mms", "mmsu", "mmst", "mmsh", "unsv", "itpc", "icyx", "rtmp", "rtp",
    "dccp", "dvd", "vcd", "vcdx"
};

static const char const ppsz_supported_mime_types[][26] = {
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

    dbus_message_iter_append_basic( container, DBUS_TYPE_STRING, &psz_id );
    return VLC_SUCCESS;
}

DBUS_METHOD( Identity )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusMessageIter v;
    dbus_message_iter_open_container( &args, DBUS_TYPE_VARIANT, "s", &v );

    MarshalIdentity( p_this, &v );

    if( !dbus_message_iter_close_container( &args, &v ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    REPLY_SEND;
}

static int
MarshalCanQuit( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    const dbus_bool_t b_ret = TRUE;

    dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN, &b_ret );
    return VLC_SUCCESS;
}

DBUS_METHOD( CanQuit )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusMessageIter v;
    dbus_message_iter_open_container( &args, DBUS_TYPE_VARIANT, "b", &v );

    MarshalCanQuit( p_this, &v );

    if( !dbus_message_iter_close_container( &args, &v ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    REPLY_SEND;
}

static int
MarshalCanRaise( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    const dbus_bool_t b_ret = FALSE;

    dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN, &b_ret );
    return VLC_SUCCESS;
}

DBUS_METHOD( CanRaise )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusMessageIter v;
    dbus_message_iter_open_container( &args, DBUS_TYPE_VARIANT, "b", &v );

    MarshalCanRaise( p_this, &v );

    if( !dbus_message_iter_close_container( &args, &v ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    REPLY_SEND;
}

static int
MarshalHasTrackList( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    const dbus_bool_t b_ret = FALSE;

    dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN, &b_ret );
    return VLC_SUCCESS;
}

DBUS_METHOD( HasTrackList )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusMessageIter v;
    dbus_message_iter_open_container( &args, DBUS_TYPE_VARIANT, "b", &v );

    MarshalHasTrackList( p_this, &v );

    if( !dbus_message_iter_close_container( &args, &v ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    REPLY_SEND;
}

static int
MarshalDesktopEntry( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    const char* psz_ret = PACKAGE;

    dbus_message_iter_append_basic( container, DBUS_TYPE_STRING, &psz_ret );
    return VLC_SUCCESS;
}

DBUS_METHOD( DesktopEntry )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusMessageIter v;
    dbus_message_iter_open_container( &args, DBUS_TYPE_VARIANT, "s", &v );

    MarshalDesktopEntry( p_this, &v );

    if( !dbus_message_iter_close_container( &args, &v ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    REPLY_SEND;
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

DBUS_METHOD( SupportedMimeTypes )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusMessageIter v;
    dbus_message_iter_open_container( &args, DBUS_TYPE_VARIANT, "as", &v );

    if( VLC_SUCCESS != MarshalSupportedMimeTypes( p_this, &v ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_close_container( &args, &v ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    REPLY_SEND;
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

DBUS_METHOD( SupportedUriSchemes )
{
    VLC_UNUSED( p_this );
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusMessageIter v;
    dbus_message_iter_open_container( &args, DBUS_TYPE_VARIANT, "as", &v );

    if( VLC_SUCCESS != MarshalSupportedUriSchemes( p_this, &v ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_close_container( &args, &v ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    REPLY_SEND;
}

DBUS_METHOD( Quit )
{ /* exits vlc */
    REPLY_INIT;
    libvlc_Quit(INTF->p_libvlc);
    REPLY_SEND;
}

DBUS_METHOD( Raise )
{/* shows vlc's main window */
    REPLY_INIT;
    var_ToggleBool( INTF->p_libvlc, "intf-show" );
    REPLY_SEND;
}

#define PROPERTY_MAPPING_BEGIN if( 0 ) {}
#define PROPERTY_FUNC( interface, property, function ) \
    else if( !strcmp( psz_interface_name, interface ) && \
             !strcmp( psz_property_name,  property ) ) \
        return function( p_conn, p_from, p_this );
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

    PROPERTY_MAPPING_BEGIN
    PROPERTY_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "Identity",            Identity )
    PROPERTY_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "DesktopEntry",        DesktopEntry )
    PROPERTY_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "SupportedMimeTypes",  SupportedMimeTypes )
    PROPERTY_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "SupportedUriSchemes", SupportedUriSchemes )
    PROPERTY_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "HasTrackList",        HasTrackList )
    PROPERTY_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "CanQuit",             CanQuit )
    PROPERTY_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "CanRaise",            CanRaise )
    PROPERTY_MAPPING_END
}

#undef PROPERTY_MAPPING_BEGIN
#undef PROPERTY_GET_FUNC
#undef PROPERTY_MAPPING_END

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
    METHOD_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "Quit",         Quit );
    METHOD_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "Raise",        Raise );
    METHOD_MAPPING_END
}

#undef METHOD_MAPPING_BEGIN
#undef METHOD_FUNC
#undef METHOD_MAPPING_END
