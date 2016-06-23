/*****************************************************************************
 * kwallet.c: KWallet keystore module
 *****************************************************************************
 * Copyright © 2015-2016 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_keystore.h>
#include <vlc_url.h>
#include <vlc_plugin.h>
#include <vlc_strings.h>

#include <dbus/dbus.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

static int Open( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_shortname( N_("KWallet keystore") )
    set_description( N_("secrets are stored via KWallet") )
    set_category( CAT_ADVANCED )
    set_subcategory( SUBCAT_ADVANCED_MISC )
    set_capability( "keystore", 100 )
    set_callbacks( Open, Close )
vlc_module_end()

/* kwallet is the kde keyring. *
 * There are several entry categories, *
 * but we only use the "Password" category. *
 * It is juste a simple Entry name ( or key ) *
 * associated with a secret. *
 * Keys are urls formated with : *
 *     _ Protocol *
 *     _ User ( optional ) *
 *     _ Server *
 *     _ Port ( optional ) *
 *     _ Path ( optional ) *
 *     _ Realm ( binary encrypted ) ( optional ) *
 *     _ Authtype ( binary encrypted ) ( optional ) *
 * Secrets are binary encrypted strings */

static const char* psz_folder = VLC_KEYSTORE_NAME;
static const char* psz_kwallet_interface = "org.kde.KWallet";

#define DBUS_INSTANCE_PREFIX "instance"
#define KWALLET_APP_ID "org.videolan.kwallet"

/*
 * There are two kwallet services :
 * kwallet and kwallet5 */

/* These services have the same interfaces and methods *
 * but not the same addresses and paths */

enum serviceId
{
    KWALLET5 = 0,
    KWALLET,
    SERVICE_MAX
};

static const char *ppsz_sAddr[SERVICE_MAX] = {
    "org.kde.kwalletd5",
    "org.kde.kwalletd"
};

static const char *ppsz_sPath[SERVICE_MAX] = {
    "/modules/kwalletd5",
    "/modules/kwalletd"
};

typedef struct vlc_keystore_sys
{
    DBusConnection* connection;
    int i_sid; /* service ID */
    int i_handle;
    char* psz_app_id;
    char* psz_wallet;
}  vlc_keystore_sys;

/* takes all values in the values of vlc_keystore_entry *
 * and formats them in a url key */
static char*
values2key( const char* const* ppsz_values, bool b_search )
{
    FILE* stream;
    size_t size = 0;
    char* psz_b64_realm = NULL;
    char* psz_b64_auth = NULL;
    char* psz_key = NULL;
    bool b_state = false;

    if ( ( !ppsz_values[KEY_PROTOCOL] || !ppsz_values[KEY_SERVER] )
         && !b_search )
        return NULL;

    stream = open_memstream( &psz_key, &size );
    if ( !stream )
        return NULL;

    /* Protocol section */
    if ( ppsz_values[KEY_PROTOCOL] )
        fprintf( stream, "%s://", ppsz_values[KEY_PROTOCOL] );
    else if ( b_search )
        fprintf( stream, "*://" );

    /* User section */
    if ( ppsz_values[KEY_USER] )
        fprintf( stream, "%s@", ppsz_values[KEY_USER] );
    else if ( b_search )
        fprintf( stream, "*" );

    /* Server section */
    if ( ppsz_values[KEY_SERVER] )
        fprintf( stream, "%s", ppsz_values[KEY_SERVER] );
    else if ( b_search )
        fprintf( stream, "*" );

    /* Port section */
    if ( ppsz_values[KEY_PORT] )
        fprintf( stream, ":%s", ppsz_values[KEY_PORT] );
    else if ( b_search )
        fprintf( stream, "*" );

    /* Path section */
    if ( ppsz_values[KEY_PATH] && ppsz_values[KEY_PATH][0] == '/')
        fprintf( stream, "%s", ppsz_values[KEY_PATH] );
    else if ( ppsz_values[KEY_PATH] && ppsz_values[KEY_PATH][0] != '/' )
        fprintf( stream, "/%s", ppsz_values[KEY_PATH] );
    else if ( b_search )
        fprintf( stream, "*" );

    /* Realm and authtype section */
    if ( ppsz_values[KEY_REALM] || ppsz_values[KEY_AUTHTYPE] || b_search )
    {
        fprintf( stream, "?" );

        /* Realm section */
        if ( ppsz_values[KEY_REALM] || b_search )
        {
            if ( ppsz_values[KEY_REALM] )
            {
                psz_b64_realm = vlc_b64_encode_binary( ( uint8_t* )ppsz_values[KEY_REALM],
                                                       strlen(ppsz_values[KEY_REALM] ) );
                if ( !psz_b64_realm )
                    goto end;
                fprintf( stream, "realm=%s", psz_b64_realm );
            }
            else
                fprintf( stream, "*" );

            if ( ppsz_values[KEY_AUTHTYPE] )
                fprintf( stream, "&" );
        }

        /* Authtype section */
        if ( ppsz_values[KEY_AUTHTYPE] || b_search )
        {

            if ( ppsz_values[KEY_AUTHTYPE] )
            {
                psz_b64_auth = vlc_b64_encode_binary( ( uint8_t* )ppsz_values[KEY_AUTHTYPE],
                                                      strlen(ppsz_values[KEY_AUTHTYPE] ) );
                if ( !psz_b64_auth )
                    goto end;
                fprintf( stream, "authtype=%s", psz_b64_auth );
            }
            else
                fprintf( stream, "*" );
        }

    }

    b_state = true;

end:
    if ( !b_state )
    {
        free( psz_key );
        psz_key = NULL;
    }
    fflush( stream );
    fclose( stream );
    free( psz_b64_realm );
    free( psz_b64_auth );
    return psz_key;
}

/* Take an url key and splits it into vlc_keystore_entry values */
static int
key2values( char* psz_key, vlc_keystore_entry* p_entry )
{
    vlc_url_t url;
    int i_ret = VLC_ENOMEM;

    for ( int inc = 0 ; inc < KEY_MAX ; ++inc )
        p_entry->ppsz_values[inc] = NULL;

    vlc_UrlParse( &url, psz_key );

    if ( url.psz_protocol && !( p_entry->ppsz_values[KEY_PROTOCOL] =
                                strdup( url.psz_protocol ) ) )
        goto end;
    if ( url.psz_username && !( p_entry->ppsz_values[KEY_USER] =
                                strdup( url.psz_username ) ) )
        goto end;
    if ( url.psz_host && !( p_entry->ppsz_values[KEY_SERVER] =
                            strdup( url.psz_host ) ) )
        goto end;
    if ( url.i_port && asprintf( &p_entry->ppsz_values[KEY_PORT],
                                 "%d", url.i_port) == -1 )
        goto end;
    if ( url.psz_path && !( p_entry->ppsz_values[KEY_PATH] =
                            strdup( url.psz_path ) ) )
        goto end;
    if ( url.psz_option )
    {
        char *p_savetpr;

        for ( const char *psz_option = strtok_r( url.psz_option, "&", &p_savetpr );
              psz_option != NULL;
              psz_option = strtok_r( NULL, "&", &p_savetpr ) )
        {
            enum vlc_keystore_key key;
            const char *psz_value;

            if ( !strncmp( psz_option, "realm=", strlen( "realm=" ) ) )
            {
                key = KEY_REALM;
                psz_value = psz_option + strlen( "realm=" );
            }
            else if ( !strncmp( psz_option, "authtype=", strlen( "authtype=" ) ) )
            {
                key = KEY_AUTHTYPE;
                psz_value = psz_option + strlen( "authtype=" );
            }
            else
                psz_value = NULL;

            if ( psz_value != NULL )
            {
                p_entry->ppsz_values[key] = vlc_b64_decode( psz_value );
                if ( !p_entry->ppsz_values[key] )
                    goto end;
            }
        }
    }

    i_ret = VLC_SUCCESS;

end:
    vlc_UrlClean( &url );
    if ( i_ret )
    {
        free( p_entry->ppsz_values[KEY_PROTOCOL] );
        free( p_entry->ppsz_values[KEY_USER] );
        free( p_entry->ppsz_values[KEY_SERVER] );
        free( p_entry->ppsz_values[KEY_PORT] );
        free( p_entry->ppsz_values[KEY_PATH] );
        free( p_entry->ppsz_values[KEY_REALM] );
        free ( p_entry->ppsz_values[KEY_AUTHTYPE] );
    }
    return i_ret;
}

static DBusMessage*
vlc_dbus_new_method( vlc_keystore* p_keystore, const char* psz_method )
{
    vlc_keystore_sys* p_sys = p_keystore->p_sys;
    DBusMessage* msg;

    msg = dbus_message_new_method_call( ppsz_sAddr[p_sys->i_sid],
                                        ppsz_sPath[p_sys->i_sid],
                                        psz_kwallet_interface,
                                        psz_method );
    if ( !msg )
    {
        msg_Err( p_keystore, "vlc_dbus_new_method : Failed to create message" );
        return NULL;
    }

    return msg;
}

static DBusMessage*
vlc_dbus_send_message( vlc_keystore* p_keystore, DBusMessage* msg )
{
    vlc_keystore_sys* p_sys = p_keystore->p_sys;
    DBusMessage* repmsg;
    DBusError error;

    dbus_error_init( &error );

    repmsg = dbus_connection_send_with_reply_and_block( p_sys->connection,
                                                        msg, -1,
                                                        &error);
    if ( !repmsg )
    {
        msg_Err( p_keystore, "vlc_dbus_send_message : "
                 "Failed dbus_connection_send_with_reply_and_block" );
        return NULL;
    }
    if ( dbus_error_is_set( &error ) )
    {
        msg_Err( p_keystore, "vlc_dbus_send_message : "
                 "dbus_connection_send_with_reply_and_block has error set" );
        return NULL;
    }

    return repmsg;
}

static int
kwallet_network_wallet( vlc_keystore* p_keystore )
{
    vlc_keystore_sys* p_sys = p_keystore->p_sys;
    DBusMessage* msg = NULL;
    DBusMessage* repmsg = NULL;
    DBusError error;
    char* psz_reply;
    int i_ret = VLC_EGENERIC;

    /* init */
    msg = vlc_dbus_new_method( p_keystore, "networkWallet" );
    if ( !msg )
    {
        msg_Err( p_keystore, "kwallet_network_wallet : vlc_dbus_new_method failed" );
        return VLC_EGENERIC;
    }

    /* sending message */
    repmsg = vlc_dbus_send_message( p_keystore, msg );
    if ( !repmsg )
    {
        msg_Err( p_keystore, "kwallet_network_wallet : vlc_dbus_send_message failed" );
        goto end;
    }

    /* handling reply */
    dbus_error_init( &error );
    if ( !dbus_message_get_args( repmsg, &error, DBUS_TYPE_STRING,
                                 &psz_reply, DBUS_TYPE_INVALID ) )
    {
        msg_Err( p_keystore, "kwallet_network_wallet : "
                 "dbus_message_get_args failed\n%s", error.message );
        goto end;
    }

    p_sys->psz_wallet = strdup( psz_reply );
    if ( !p_sys->psz_wallet )
    {
        i_ret = VLC_ENOMEM;
        goto end;
    }

    i_ret = VLC_SUCCESS;

end:

    if ( msg )
        dbus_message_unref( msg );
    if ( repmsg )
        dbus_message_unref( repmsg );

    return i_ret;
}

static int
kwallet_is_enabled( vlc_keystore* p_keystore, int i_sid, bool* b_is_enabled )
{
    VLC_UNUSED( p_keystore );
    DBusMessage* msg = NULL;
    DBusMessage* repmsg = NULL;
    DBusMessageIter args;
    DBusError error;
    dbus_bool_t b_reply;
    int i_ret = VLC_EGENERIC;

    /* init */
    msg = dbus_message_new_method_call( "org.freedesktop.DBus",
                                        "/",
                                        "org.freedesktop.DBus",
                                        "NameHasOwner" );
    if ( !msg )
    {
        msg_Err( p_keystore, "vlc_dbus_new_method : Failed to create message" );
        goto end;
    }

    /* argument init */
    dbus_message_iter_init_append( msg, &args );
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &ppsz_sAddr[i_sid] ) )
        goto end;

    /* sending message */
    repmsg = vlc_dbus_send_message( p_keystore, msg );
    if ( !repmsg )
    {
        msg_Err( p_keystore, "kwallet_is_enabled : vlc_dbus_send_message failed");
        goto end;
    }

    /* handling reply */
    dbus_error_init( &error );
    if ( !dbus_message_get_args( repmsg, &error, DBUS_TYPE_BOOLEAN,
                                 &b_reply, DBUS_TYPE_INVALID ) )
    {
        msg_Err( p_keystore, "kwallet_is_enabled : "
                 "dbus_message_get_args failed\n%s", error.message );
        goto end;
    }

    *b_is_enabled = b_reply;

    i_ret = VLC_SUCCESS;

end:

    if ( msg )
        dbus_message_unref( msg );
    if ( repmsg )
        dbus_message_unref( repmsg );

    return i_ret;
}

static int
vlc_dbus_init( vlc_keystore* p_keystore )
{
    vlc_keystore_sys* p_sys = p_keystore->p_sys;
    int i_ret;
    DBusError error;

    dbus_error_init( &error );

    /* DBus Connection */
    p_sys->connection = dbus_bus_get_private( DBUS_BUS_SESSION, &error );
    if ( dbus_error_is_set( &error ) )
    {
        msg_Err( p_keystore, "vlc_dbus_init : "
                 "Connection error to session bus (%s)", error.message );
        dbus_error_free( &error );
    }
    if ( !p_sys->connection )
    {
        msg_Err( p_keystore, "vlc_dbus_init : connection is NULL");
        return VLC_EGENERIC;
    }

    /* requesting name */
    for( unsigned i = 0; i <= 99 && p_sys->psz_app_id == NULL; ++i )
    {
        char psz_dbus_name[strlen( KWALLET_APP_ID ) + strlen( DBUS_INSTANCE_PREFIX ) + 5];

        sprintf( psz_dbus_name, "%s.%s_%02u", KWALLET_APP_ID, DBUS_INSTANCE_PREFIX, i );
        i_ret = dbus_bus_request_name( p_sys->connection, psz_dbus_name, 0,
                                       &error );
        if ( dbus_error_is_set( &error ) )
        {
            msg_Err( p_keystore, "vlc_dbus_init : dbus_bus_request_name :"
                     " error (%s)", error.message );
            dbus_error_free( &error );
        }
        if ( i_ret == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
        {
            p_sys->psz_app_id = strdup( psz_dbus_name );
            if ( !p_sys->psz_app_id )
                goto error;
        }
    }
    if ( p_sys->psz_app_id == NULL )
    {
        msg_Err( p_keystore, "vlc_dbus_init : Too many kwallet instances" );
        goto error;
    }

    /* check to see if any kwallet service is enabled */
    unsigned int i = 0;
    for ( ; i < SERVICE_MAX ; ++i )
    {
        bool b_is_enabled = false;
        if ( kwallet_is_enabled( p_keystore, i, &b_is_enabled ) )
        {
            msg_Err( p_keystore, "vlc_dbus_init : kwallet_is_enabled failed" );
            goto error;
        }
        if ( b_is_enabled == true )
            break;
    }
    if ( i == SERVICE_MAX )
    {
        msg_Err( p_keystore, "vlc_dbus_init : No kwallet service enabled" );
        goto error;
    }
    p_sys->i_sid = i;

    /* getting the name of the wallet assigned to network passwords */
    if ( kwallet_network_wallet( p_keystore ) )
    {
        msg_Err(p_keystore, "vlc_dbus_init : kwallet_network_wallet has failed");
        goto error;
    }

    return VLC_SUCCESS;

error:
    dbus_connection_close( p_sys->connection );
    dbus_connection_unref( p_sys->connection );
    return VLC_EGENERIC;
}

static int
kwallet_has_folder( vlc_keystore* p_keystore, const char* psz_folder_name, bool *b_has_folder )
{
    vlc_keystore_sys* p_sys = p_keystore->p_sys;
    DBusMessage* msg = NULL;
    DBusMessage* repmsg = NULL;
    DBusError error;
    DBusMessageIter args;
    dbus_bool_t b_reply;
    int i_ret = VLC_EGENERIC;

    /* init */
    msg = vlc_dbus_new_method( p_keystore, "hasFolder" );
    if ( !msg )
    {
        msg_Err( p_keystore, "kwallet_has_folder : vlc_dbus_new_method failed" );
        return VLC_EGENERIC;
    }

    /* argument init */
    dbus_message_iter_init_append( msg, &args );
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_INT32, &p_sys->i_handle ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &psz_folder_name ) )
        goto end;
    if ( !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &p_sys->psz_app_id ) )
        goto end;

    /* sending message */
    repmsg = vlc_dbus_send_message( p_keystore, msg );
    if ( !repmsg )
    {
        msg_Err( p_keystore, "kwallet_has_folder : vlc_dbus_send_message failed" );
        goto end;
    }

    /* handling reply */

    dbus_error_init( &error );
    if ( !dbus_message_get_args( repmsg, &error, DBUS_TYPE_BOOLEAN,
                                 &b_reply, DBUS_TYPE_INVALID ) )
    {
        msg_Err( p_keystore, "kwallet_has_folder :"
                 " dbus_message_get_args failed\n%s", error.message );
        goto end;
    }

    *b_has_folder = b_reply;

    i_ret = VLC_SUCCESS;

end:

    if ( msg )
        dbus_message_unref( msg );
    if ( repmsg )
        dbus_message_unref( repmsg);

    return i_ret;
}

static int
kwallet_create_folder( vlc_keystore* p_keystore, const char* psz_folder_name )
{
    vlc_keystore_sys* p_sys = p_keystore->p_sys;
    DBusMessage* msg = NULL;
    DBusMessage* repmsg = NULL;
    DBusError error;
    DBusMessageIter args;
    dbus_bool_t b_reply;
    int i_ret = VLC_EGENERIC;

    /* init */
    msg = vlc_dbus_new_method( p_keystore, "createFolder" );
    if ( !msg )
    {
        msg_Err( p_keystore, "kwallet_create_folder : vlc_dbus_new_method failed" );
        return VLC_EGENERIC;
    }

    /* argument init */
    dbus_message_iter_init_append( msg, &args );
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_INT32, &p_sys->i_handle ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &psz_folder_name ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &p_sys->psz_app_id ) )
        goto end;

    /* sending message */
    repmsg = vlc_dbus_send_message( p_keystore, msg );
    if ( !repmsg )
    {
        msg_Err( p_keystore, "kwallet_create_folder : vlc_dbus_send_message failed" );
        goto end;
    }

    /* handling reply */
    dbus_error_init( &error );
    if ( !dbus_message_get_args( repmsg, &error, DBUS_TYPE_BOOLEAN,
                                 &b_reply, DBUS_TYPE_INVALID ) )
    {
        msg_Err( p_keystore, "kwallet_create_folder :"
                 " dbus_message_get_args failed\n%s", error.message );
        goto end;
    }

    if ( !b_reply )
    {
        msg_Err( p_keystore, "kwallet_create_folder : Could not create folder" );
        goto end;
    }


    i_ret = VLC_SUCCESS;

end:

    if ( msg )
        dbus_message_unref( msg );
    if ( repmsg )
        dbus_message_unref( repmsg );

    return i_ret;
}

static int
kwallet_open( vlc_keystore* p_keystore )
{
    vlc_keystore_sys* p_sys = p_keystore->p_sys;
    DBusMessage* msg = NULL;
    DBusMessage* repmsg = NULL;
    DBusMessageIter args;
    DBusError error;
    unsigned long long ull_win_id = 0;
    unsigned int ui_reply = 1;
    bool b_has_folder;
    int i_ret = VLC_EGENERIC;

    /* init */
    msg = vlc_dbus_new_method( p_keystore, "open" );
    if ( !msg )
    {
        msg_Err( p_keystore, "kwallet_open : vlc_dbus_new_method failed");
        return VLC_EGENERIC;
    }

    /* Init args */
    dbus_message_iter_init_append(msg, &args);
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &p_sys->psz_wallet ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_INT64, &ull_win_id ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &p_sys->psz_app_id ) )
        goto end;

    /* sending message */
    repmsg = vlc_dbus_send_message( p_keystore, msg );
    if ( !repmsg )
    {
        msg_Err( p_keystore, "kwallet_open : vlc_dbus_send_message failed" );
        goto end;
    }

    /* reply arguments */
    dbus_error_init( &error );
    if ( !dbus_message_get_args( repmsg, &error, DBUS_TYPE_INT32,
                                 &ui_reply, DBUS_TYPE_INVALID ) )
    {
        msg_Err( p_keystore, "kwallet_open :"
                 " dbus_message_get_args failed\n%s", error.message );
        goto end;
    }
    p_sys->i_handle = ui_reply;

    /* opening the vlc password folder == VLC_KEYSTORE_NAME */
    if ( kwallet_has_folder( p_keystore, psz_folder, &b_has_folder ) )
        goto end;
    if ( !b_has_folder )
    {
        if ( kwallet_create_folder( p_keystore, psz_folder ) )
        {
            msg_Err( p_keystore, "kwallet_open : could not create folder %s",
                     psz_folder );
            goto end;
        }
    }

    i_ret = VLC_SUCCESS;

end:

    if ( msg )
        dbus_message_unref( msg );
    if ( repmsg )
        dbus_message_unref( repmsg );

    return i_ret;
}

static int
kwallet_has_entry( vlc_keystore* p_keystore, char* psz_entry_name, bool *b_has_entry )
{
    vlc_keystore_sys* p_sys = p_keystore->p_sys;
    DBusMessage* msg = NULL;
    DBusMessage* repmsg = NULL;
    DBusError error;
    DBusMessageIter args;
    dbus_bool_t b_reply;
    int i_ret = VLC_EGENERIC;

    /* init */
    if ( !( msg = vlc_dbus_new_method(p_keystore, "hasEntry" ) ) )
    {
        msg_Err( p_keystore, "kwallet_has_entry : vlc_dbus_new_method failed" );
        return VLC_EGENERIC;
    }

    /* argument init */
    dbus_message_iter_init_append( msg, &args );
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_INT32, &p_sys->i_handle ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &psz_folder ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &psz_entry_name ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &p_sys->psz_app_id ) )
        goto end;

    /* sending message */

    if ( !( repmsg = vlc_dbus_send_message( p_keystore, msg ) ) )
    {
        msg_Err( p_keystore, "kwallet_has_entry : vlc_dbus_send_message failed" );
        goto end;
    }

    /* handling reply */
    dbus_error_init( &error );
    if ( !dbus_message_get_args( repmsg, &error, DBUS_TYPE_BOOLEAN,
                                 &b_reply, DBUS_TYPE_INVALID ) )
    {
        msg_Err( p_keystore, "kwallet_has_entry :"
                 " dbus_message_get_args failed\n%s", error.message );
        goto end;
    }
    *b_has_entry = b_reply;

    i_ret = VLC_SUCCESS;

end:

    if ( msg )
        dbus_message_unref( msg );
    if ( repmsg )
        dbus_message_unref( repmsg );

    return i_ret;
}

static int
kwallet_write_password( vlc_keystore* p_keystore, char* psz_entry_name, const char* psz_secret )
{
    vlc_keystore_sys* p_sys = p_keystore->p_sys;
    DBusMessage* msg = NULL;
    DBusMessage* repmsg = NULL;
    DBusError error;
    DBusMessageIter args;
    int i_reply;
    int i_ret = VLC_EGENERIC;

    /* init */
    if ( !( msg = vlc_dbus_new_method( p_keystore, "writePassword" ) ) )
    {
        msg_Err( p_keystore, "kwallet_write_password : vlc_dbus_new_method failed" );
        return VLC_EGENERIC;
    }

    /* argument init */
    dbus_message_iter_init_append( msg, &args );
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_INT32, &p_sys->i_handle ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &psz_folder ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &psz_entry_name ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &psz_secret ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &p_sys->psz_app_id ) )
        goto end;

    /* sending message */
    if ( !( repmsg = vlc_dbus_send_message( p_keystore, msg ) ) )
    {
        msg_Err( p_keystore, "kwallet_write_password : vlc_dbus_send_message failed" );
        goto end;
    }

    /* handling reply */
    dbus_error_init( &error );
    if ( !dbus_message_get_args( repmsg, &error, DBUS_TYPE_INT32,
                                 &i_reply, DBUS_TYPE_INVALID ) )
    {
        msg_Err( p_keystore, "kwallet_write_password :"
                 " dbus_message_get_args failed\n%s", error.message );
        goto end;
    }

    i_ret = VLC_SUCCESS;

end:

    if ( msg )
        dbus_message_unref( msg );
    if ( repmsg )
        dbus_message_unref( repmsg );

    return i_ret;
}

static int
kwallet_remove_entry( vlc_keystore* p_keystore, char* psz_entry_name )
{
    vlc_keystore_sys* p_sys = p_keystore->p_sys;
    DBusMessage* msg = NULL;
    DBusMessage* repmsg = NULL;
    DBusError error;
    DBusMessageIter args;
    int i_reply;
    bool b_has_entry = false;
    int i_ret = VLC_EGENERIC;

    if ( kwallet_has_entry( p_keystore, psz_entry_name, &b_has_entry ) )
    {
        msg_Err( p_keystore, "kwallet_remove_entry : kwallet_has_entry failed" );
        return VLC_EGENERIC;
    }
    if ( !b_has_entry )
    {
        msg_Err( p_keystore, "kwallet_remove_entry : there is no such entry :"
                "%s", psz_entry_name );
        return VLC_EGENERIC;
    }

    /* init */
    if ( !( msg = vlc_dbus_new_method( p_keystore, "removeEntry" ) ) )
    {
        msg_Err( p_keystore, "kwallet_remove_entry : vlc_dbus_new_method failed" );
        return VLC_EGENERIC;
    }

    /* argument init */
    dbus_message_iter_init_append( msg, &args );
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_INT32, &p_sys->i_handle ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &psz_folder ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &psz_entry_name ) )
        goto end;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &p_sys->psz_app_id ) )
        goto end;

    /* sending message */
    if ( !( repmsg = vlc_dbus_send_message( p_keystore, msg ) ) )
    {
        msg_Err( p_keystore, "kwallet_remove_entry : vlc_dbus_send_message failed" );
        goto end;
    }

    /* handling reply */
    dbus_error_init( &error );
    if ( !dbus_message_get_args( repmsg, &error, DBUS_TYPE_INT32,
                                 &i_reply, DBUS_TYPE_INVALID ) )
    {
        msg_Err( p_keystore, "kwallet_remove entry :"
                 " dbus_message_get_args failed\n%s", error.message );
        goto end;
    }

    i_ret = VLC_SUCCESS;

end:

    if ( msg )
        dbus_message_unref( msg );
    if ( repmsg )
        dbus_message_unref( repmsg );

    return i_ret;
}

static vlc_keystore_entry*
kwallet_read_password_list( vlc_keystore* p_keystore, char* psz_entry_name,
                            unsigned int* pi_count )
{
    vlc_keystore_sys* p_sys = p_keystore->p_sys;
    DBusMessage* msg = NULL;
    DBusMessage* repmsg = NULL;
    DBusMessageIter args;
    DBusMessageIter sub_iter;
    DBusMessageIter dict_iter;
    DBusMessageIter var_iter;
    vlc_keystore_entry* p_entries = NULL;
    size_t i_size;
    uint8_t* p_secret_decoded;
    char* p_reply;
    char* p_secret;
    int i = 0;

    /* init */
    *pi_count = 0;
    if ( !( msg = vlc_dbus_new_method( p_keystore, "readPasswordList" ) ) )
    {
        msg_Err( p_keystore, "kwallet_read_password_list : vlc_dbus_new_method failed" );
        goto error;
    }

    /* argument init */
    dbus_message_iter_init_append( msg, &args );
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_INT32, &p_sys->i_handle ) )
        goto error;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &psz_folder ) )
        goto error;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &psz_entry_name ) )
        goto error;
    if ( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &p_sys->psz_app_id ) )
        goto error;

    /* sending message */
    if ( !( repmsg = vlc_dbus_send_message( p_keystore, msg ) ) )
    {
        msg_Err( p_keystore, "kwallet_read_password_list : vlc_dbus_send_message failed" );
        goto error;
    }

    /* handling reply */
    if ( !dbus_message_iter_init( repmsg, &args ) )
    {
        msg_Err( p_keystore, "kwallet_read_password_list : Message has no arguments" );
        goto error;
    }
    else if ( dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_ARRAY )
    {
        msg_Err( p_keystore, "kwallet_read_password_list : Wrong reply type" );
        goto error;
    }
    else
    {
        /* calculating p_entries's size */
        dbus_message_iter_recurse( &args, &sub_iter );
        do
        {
            if ( dbus_message_iter_get_arg_type( &sub_iter ) != DBUS_TYPE_DICT_ENTRY )
                continue;
            dbus_message_iter_recurse( &sub_iter, &dict_iter );
            if ( dbus_message_iter_get_arg_type( &dict_iter ) != DBUS_TYPE_STRING )
                continue;
            dbus_message_iter_next(&dict_iter);
            if ( dbus_message_iter_get_arg_type( &dict_iter ) != DBUS_TYPE_VARIANT )
                continue;
            ++( *pi_count );
       } while ( dbus_message_iter_next( &sub_iter ) );

        if ( *pi_count == 0 )
            goto error;
        if ( !( p_entries = calloc( *pi_count, sizeof( vlc_keystore_entry ) ) ) )
            goto error;

        dbus_message_iter_init( repmsg, &args );
        /* recurse into the reply array */
        dbus_message_iter_recurse( &args, &sub_iter );
        do
        {
            if ( dbus_message_iter_get_arg_type( &sub_iter ) != DBUS_TYPE_DICT_ENTRY )
            {
                msg_Err( p_keystore, "Wrong type not DBUS_TYPE_DICT_ENTRY" );
                continue;
            }
            /* recurse into the dict-entry in the array */
            dbus_message_iter_recurse( &sub_iter, &dict_iter );
            if ( dbus_message_iter_get_arg_type( &dict_iter ) != DBUS_TYPE_STRING )
            {
                msg_Err( p_keystore, "First type of Dict-Entry is not a string" );
                continue;
            }
            dbus_message_iter_get_basic( &dict_iter, &p_reply );
            dbus_message_iter_next(&dict_iter);
            if ( dbus_message_iter_get_arg_type( &dict_iter ) != DBUS_TYPE_VARIANT )
            {
                msg_Err( p_keystore, "Second type of Dict-Entry is not a variant" );
                continue;
            }
            /* recurse into the variant in the dict-entry */
            dbus_message_iter_recurse( &dict_iter, &var_iter );
            dbus_message_iter_get_basic( &var_iter, &p_secret );

            i_size = vlc_b64_decode_binary( &p_secret_decoded, p_secret);
            if ( key2values( p_reply, &p_entries[i] ) )
                goto error;
            if ( ( vlc_keystore_entry_set_secret( &p_entries[i],
                                                  p_secret_decoded,
                                                  i_size ) ) )
                goto error;

            free(p_secret_decoded);
            i += 1;
        } while ( dbus_message_iter_next( &sub_iter ) );
    }

    dbus_message_unref( msg );
    dbus_message_unref( repmsg );

    return p_entries;

error:
    *pi_count = 0;
    vlc_keystore_release_entries( p_entries, i );
    if ( msg )
        dbus_message_unref( msg );
    if ( repmsg )
        dbus_message_unref( repmsg );
    return NULL;
}


static int
Store( vlc_keystore* p_keystore, const char *const ppsz_values[KEY_MAX],
       const uint8_t* p_secret, size_t i_secret_len, const char* psz_label )
{
    char* psz_key;
    char* psz_b64_secret;

    (void)psz_label;

    psz_key = values2key( ppsz_values, false );
    if ( !psz_key )
        return VLC_ENOMEM;

    psz_b64_secret = vlc_b64_encode_binary( p_secret, i_secret_len );
    if ( !psz_b64_secret )
        return VLC_ENOMEM;

    if ( kwallet_write_password( p_keystore, psz_key, psz_b64_secret ) )
    {
        free( psz_b64_secret );
        free( psz_key );
        return VLC_EGENERIC;
    }

    free( psz_b64_secret );
    free( psz_key );

    return VLC_SUCCESS;
}

static unsigned int
Find( vlc_keystore* p_keystore, const char *const ppsz_values[KEY_MAX],
      vlc_keystore_entry** pp_entries )
{
    char* psz_key;
    unsigned int i_count = 0;

    psz_key = values2key( ppsz_values, true );
    if ( !psz_key )
        return i_count;
    *pp_entries = kwallet_read_password_list( p_keystore, psz_key, &i_count );
    if ( !*pp_entries )
    {
        free( psz_key );
        return i_count;
    }

    free( psz_key );

    return i_count;
}

static unsigned int
Remove( vlc_keystore* p_keystore, const char* const ppsz_values[KEY_MAX] )
{
    char* psz_key;
    vlc_keystore_entry* p_entries;
    unsigned i_count = 0;

    psz_key = values2key( ppsz_values, true );
    if ( !psz_key )
        return 0;

    p_entries = kwallet_read_password_list( p_keystore, psz_key, &i_count );
    if ( !p_entries )
    {
        free( psz_key );
        return 0;
    }

    free( psz_key );

    for ( unsigned int i = 0 ; i < i_count ; ++i )
    {
        psz_key = values2key( ( const char* const* )p_entries[i].ppsz_values, false );
        if ( !psz_key )
        {
            vlc_keystore_release_entries( p_entries, i_count );
            return i;
        }

        if ( kwallet_remove_entry( p_keystore, psz_key ) )
        {
            vlc_keystore_release_entries( p_entries, i_count );
            free( psz_key );
            return i;
        }
        for ( int inc = 0 ; inc < KEY_MAX ; ++inc )
            free( p_entries[i].ppsz_values[inc] );
        free( p_entries[i].p_secret );
        free( psz_key );
    }

    free( p_entries );

    return i_count;
}

static void
Close( vlc_object_t* p_this )
{
    vlc_keystore *p_keystore = ( vlc_keystore* )p_this;

    dbus_connection_close( p_keystore->p_sys->connection );
    dbus_connection_unref( p_keystore->p_sys->connection );
    free( p_keystore->p_sys->psz_app_id );
    free( p_keystore->p_sys->psz_wallet );
    free( p_keystore->p_sys );
}

static int
Open( vlc_object_t* p_this )
{
    vlc_keystore *p_keystore = ( vlc_keystore* )p_this;
    int i_ret;

    p_keystore->p_sys = calloc( 1, sizeof( vlc_keystore_sys ) );
    if ( !p_keystore->p_sys)
        return VLC_ENOMEM;

    i_ret = vlc_dbus_init( p_keystore );
    if ( i_ret )
    {
        msg_Err( p_keystore, "vlc_dbus_init failed" );
        goto error;
    }

    i_ret = kwallet_open( p_keystore );
    if ( i_ret )
    {
        msg_Err( p_keystore, "kwallet_open failed" );
        goto error;
    }

    p_keystore->pf_store = Store;
    p_keystore->pf_find = Find;
    p_keystore->pf_remove = Remove;

    return i_ret;

error:
    free( p_keystore->p_sys );
    return i_ret;
}
