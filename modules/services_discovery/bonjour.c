/*****************************************************************************
 * bonjour.c: Bonjour services discovery module
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon@nanocrew.net>
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
 * Includes
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_arrays.h>

#include <avahi-client/client.h>
#ifdef HAVE_AVAHI_06
# include <avahi-client/publish.h>
# include <avahi-client/lookup.h>
#endif
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* Callbacks */
    static int  Open ( vlc_object_t * );
    static void Close( vlc_object_t * );

vlc_module_begin();
    set_shortname( "Bonjour" );
    set_description( N_("Bonjour services") );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );
    set_capability( "services_discovery", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local structures
 *****************************************************************************/

struct services_discovery_sys_t
{
    AvahiSimplePoll     *simple_poll;
    AvahiClient         *client;
    AvahiServiceBrowser *sb;
    vlc_dictionary_t    services_name_to_input_item;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* Main functions */
    static void Run    ( services_discovery_t *p_intf );

/*****************************************************************************
 * client_callback
 *****************************************************************************/
static void client_callback( AvahiClient *c, AvahiClientState state,
                             void * userdata )
{
    services_discovery_t *p_sd = ( services_discovery_t* )userdata;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

#ifdef HAVE_AVAHI_06
    if( state == AVAHI_CLIENT_FAILURE &&
        (avahi_client_errno(c) == AVAHI_ERR_DISCONNECTED) )
#else
    if( state == AVAHI_CLIENT_DISCONNECTED )
#endif
    {
        msg_Err( p_sd, "avahi client disconnected" );
        avahi_simple_poll_quit( p_sys->simple_poll );
    }
}

/*****************************************************************************
 * resolve_callback
 *****************************************************************************/
static void resolve_callback(
    AvahiServiceResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *address,
    uint16_t port,
    AvahiStringList *txt,
#ifdef HAVE_AVAHI_06
    AvahiLookupResultFlags flags,
#endif
    void* userdata )
{
    services_discovery_t *p_sd = ( services_discovery_t* )userdata;
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    
    VLC_UNUSED(interface); VLC_UNUSED(host_name);
#ifdef HAVE_AVAHI_06
    VLC_UNUSED(flags);
#endif

#ifdef HAVE_AVAHI_06
    if( event == AVAHI_RESOLVER_FAILURE )
#else
    if( event == AVAHI_RESOLVER_TIMEOUT )
#endif
    {
        msg_Err( p_sd,
                 "failed to resolve service '%s' of type '%s' in domain '%s'",
                 name, type, domain );
    }
    else if( event == AVAHI_RESOLVER_FOUND )
    {
        char a[128];
        char *psz_uri = NULL;
        char *psz_addr = NULL;
        AvahiStringList *asl = NULL;
        input_item_t *p_input = NULL;

        msg_Dbg( p_sd, "service '%s' of type '%s' in domain '%s'",
                 name, type, domain );

        avahi_address_snprint(a, (sizeof(a)/sizeof(a[0]))-1, address);
        if( protocol == AVAHI_PROTO_INET6 )
            if( asprintf( &psz_addr, "[%s]", a ) == -1 )
                return;

        if( txt != NULL )
            asl = avahi_string_list_find( txt, "path" );
        if( asl != NULL )
        {
            size_t size;
            char *key = NULL;
            char *value = NULL;
            if( avahi_string_list_get_pair( asl, &key, &value, &size ) == 0 &&
                value != NULL )
            {
                if( asprintf( &psz_uri, "http://%s:%d%s",
                          psz_addr != NULL ? psz_addr : a, port, value ) == -1 )
                {
                    free( psz_addr );
                    return;
                }
            }
            if( key != NULL )
                avahi_free( (void *)key );
            if( value != NULL )
                avahi_free( (void *)value );
        }
        else
        {
            if( asprintf( &psz_uri, "http://%s:%d",
                      psz_addr != NULL ? psz_addr : a, port ) == -1 )
            {
                free( psz_addr );
                return;
            }
        }

        if( psz_addr != NULL )
            free( (void *)psz_addr );

        if( psz_uri != NULL )
        {
            p_input = input_item_NewExt( p_sd, psz_uri, name, 0, NULL, -1 );
            free( (void *)psz_uri );
        }
        if( p_input != NULL )
        {
            vlc_dictionary_insert( &p_sys->services_name_to_input_item,
                name, p_input );
            services_discovery_AddItem( p_sd, p_input, NULL /* no category */ );
            vlc_gc_decref( p_input );
       }
    }

    avahi_service_resolver_free( r );
}

/*****************************************************************************
 * browser_callback
 *****************************************************************************/
static void browse_callback(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
#ifdef HAVE_AVAHI_06
    AvahiLookupResultFlags flags,
#endif
    void* userdata )
{
    VLC_UNUSED(b);
#ifdef HAVE_AVAHI_06
    VLC_UNUSED(flags);
#endif
    services_discovery_t *p_sd = ( services_discovery_t* )userdata;
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    if( event == AVAHI_BROWSER_NEW )
    {
        if( avahi_service_resolver_new( p_sys->client, interface, protocol,
                                        name, type, domain, AVAHI_PROTO_UNSPEC,
#ifdef HAVE_AVAHI_06
                                        0,
#endif
                                        resolve_callback, userdata ) == NULL )
        {
            msg_Err( p_sd, "failed to resolve service '%s': %s", name,
                     avahi_strerror( avahi_client_errno( p_sys->client ) ) );
        }
    }
    else if( name )
    {
        /** \todo Store the input id and search it, rather than searching the items */
        input_item_t *p_item;
        p_item = vlc_dictionary_value_for_key(
                        &p_sys->services_name_to_input_item,
                        name );
        if( !p_item )
            msg_Err( p_sd, "failed to find service '%s' in playlist", name );
        else
        {
            services_discovery_RemoveItem( p_sd, p_item );
            vlc_dictionary_remove_value_for_key(
                        &p_sys->services_name_to_input_item,
                        name );
        }
    }
}

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys;
    int err;

    p_sd->p_sys = p_sys = (services_discovery_sys_t *)malloc(
        sizeof( services_discovery_sys_t ) );

    if( !p_sys )
        return VLC_ENOMEM;

    memset( p_sys, 0, sizeof(*p_sys) );

    vlc_dictionary_init( &p_sys->services_name_to_input_item, 1 );

    p_sys->simple_poll = avahi_simple_poll_new();
    if( p_sys->simple_poll == NULL )
    {
        msg_Err( p_sd, "failed to create avahi simple poll" );
        goto error;
    }

    p_sys->client = avahi_client_new( avahi_simple_poll_get(p_sys->simple_poll),
#ifdef HAVE_AVAHI_06
                                      0,
#endif
                                      client_callback, p_sd, &err );
    if( p_sys->client == NULL )
    {
        msg_Err( p_sd, "failed to create avahi client: %s",
                 avahi_strerror( err ) );
        goto error;
    }

    p_sys->sb = avahi_service_browser_new( p_sys->client, AVAHI_IF_UNSPEC,
                                           AVAHI_PROTO_UNSPEC,
                                           "_vlc-http._tcp", NULL,
#ifdef HAVE_AVAHI_06
                                           0,
#endif
                                           browse_callback, p_sd );
    if( p_sys->sb == NULL )
    {
        msg_Err( p_sd, "failed to create avahi service browser" );
        goto error;
    }

    services_discovery_SetLocalizedName( p_sd, _("Bonjour") );

    p_sd->pf_run = Run;

    return VLC_SUCCESS;

error:
    if( p_sys->sb != NULL )
        avahi_service_browser_free( p_sys->sb );
    if( p_sys->client != NULL )
        avahi_client_free( p_sys->client );
    if( p_sys->simple_poll != NULL )
        avahi_simple_poll_free( p_sys->simple_poll );

    vlc_dictionary_clear( &p_sys->services_name_to_input_item );
    free( p_sys );

    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: cleanup
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    avahi_service_browser_free( p_sys->sb );
    avahi_client_free( p_sys->client );
    avahi_simple_poll_free( p_sys->simple_poll );

    vlc_dictionary_clear( &p_sys->services_name_to_input_item );
    free( p_sys );
}

/*****************************************************************************
 * Run: main thread
 *****************************************************************************/
static void Run( services_discovery_t *p_sd )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    while( vlc_object_alive (p_sd) )
    {
        if( avahi_simple_poll_iterate( p_sys->simple_poll, 100 ) != 0 )
        {
            msg_Err( p_sd, "poll iterate failed" );
            break;
        }
    }
}
