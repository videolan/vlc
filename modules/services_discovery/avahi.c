/*****************************************************************************
 * avahi.c: Bonjour services discovery module
 *****************************************************************************
 * Copyright (C) 2005-2009, 2016 VideoLAN and VLC authors
 *
 * Authors: Jon Lech Johansen <jon@nanocrew.net>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_services_discovery.h>
#include <vlc_renderer_discovery.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-client/lookup.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* Callbacks */
static int  OpenSD ( vlc_object_t * );
static void CloseSD( vlc_object_t * );
static int  OpenRD ( vlc_object_t * );
static void CloseRD( vlc_object_t * );

VLC_SD_PROBE_HELPER("avahi", N_("Zeroconf network services"), SD_CAT_LAN)
VLC_RD_PROBE_HELPER( "avahi_renderer", "Avahi Zeroconf renderer Discovery" )

vlc_module_begin ()
    set_shortname( "Avahi" )
    set_description( N_("Zeroconf services") )
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_SD )
    set_capability( "services_discovery", 0 )
    set_callbacks( OpenSD, CloseSD )
    add_shortcut( "mdns", "avahi" )

    VLC_SD_PROBE_SUBMODULE
    add_submodule() \
        set_description( N_( "Avahi Renderer Discovery" ) )
        set_category( CAT_SOUT )
        set_subcategory( SUBCAT_SOUT_RENDERER )
        set_capability( "renderer_discovery", 0 )
        set_callbacks( OpenRD, CloseRD )
        add_shortcut( "mdns_renderer", "avahi_renderer" )
        VLC_RD_PROBE_SUBMODULE
vlc_module_end ()

/*****************************************************************************
 * Local structures
 *****************************************************************************/

typedef struct
{
    AvahiThreadedPoll   *poll;
    AvahiClient         *client;
    vlc_dictionary_t    services_name_to_input_item;
    vlc_object_t        *parent;
    bool                renderer;
} discovery_sys_t;

static const struct
{
    const char *psz_protocol;
    const char *psz_service_name;
    bool        b_renderer;
} protocols[] = {
    { "ftp", "_ftp._tcp", false },
    { "smb", "_smb._tcp", false },
    { "nfs", "_nfs._tcp", false },
    { "sftp", "_sftp-ssh._tcp", false },
    { "rtsp", "_rtsp._tcp", false },
    { "chromecast", "_googlecast._tcp", true },
};
#define NB_PROTOCOLS (sizeof(protocols) / sizeof(*protocols))

static char* get_string_list_value( AvahiStringList* txt, const char* key )
{
    AvahiStringList *asl = avahi_string_list_find( txt, key );
    if( asl == NULL )
        return NULL;
    char* res = NULL;
    char *sl_key = NULL;
    char *sl_value = NULL;
    if( avahi_string_list_get_pair( asl, &sl_key, &sl_value, NULL ) == 0 &&
        sl_value != NULL )
    {
        res = strdup( sl_value );
    }

    if( sl_key != NULL )
        avahi_free( (void *)sl_key );
    if( sl_value != NULL )
        avahi_free( (void *)sl_value );
    return res;
}

/*****************************************************************************
 * helpers
 *****************************************************************************/
static void add_renderer( const char *psz_protocol, const char *psz_name,
                          const char *psz_addr, uint16_t i_port,
                          AvahiStringList *txt, discovery_sys_t *p_sys )
{
    vlc_renderer_discovery_t *p_rd = ( vlc_renderer_discovery_t* )(p_sys->parent);
    AvahiStringList *asl = NULL;
    char *friendly_name = NULL;
    char *icon_uri = NULL;
    char *uri = NULL;
    char *model = NULL;
    const char *demux = NULL;
    const char *extra_uri = NULL;
    int renderer_flags = 0;

    if( !strcmp( "chromecast", psz_protocol ) ) {
        /* Capabilities */
        asl = avahi_string_list_find( txt, "ca" );
        if( asl != NULL ) {
            char *key = NULL;
            char *value = NULL;
            if( avahi_string_list_get_pair( asl, &key, &value, NULL ) == 0 &&
                value != NULL )
            {
                int ca = atoi( value );

                if( ( ca & 0x01 ) != 0 )
                    renderer_flags |= VLC_RENDERER_CAN_VIDEO;
                if( ( ca & 0x04 ) != 0 )
                    renderer_flags |= VLC_RENDERER_CAN_AUDIO;
            }

            if( key != NULL )
                avahi_free( (void *)key );
            if( value != NULL )
                avahi_free( (void *)value );
        }

        /* Friendly name */
        friendly_name = get_string_list_value( txt, "fn" );

        /* Icon */
        char* icon_raw = get_string_list_value( txt, "ic" );
        if( icon_raw != NULL ) {
            if( asprintf( &icon_uri, "http://%s:8008%s", psz_addr, icon_raw) < 0 )
                icon_uri = NULL;
            free( icon_raw );
        }

        model = get_string_list_value( txt, "md" );

        if( asprintf( &uri, "%s://%s:%u", psz_protocol, psz_addr, i_port ) < 0 )
            goto error;

        extra_uri = renderer_flags & VLC_RENDERER_CAN_VIDEO ? NULL : "no-video";
        demux = "cc_demux";
    }

    if ( friendly_name && model ) {
        char* combined;
        if ( asprintf( &combined, "%s (%s)", friendly_name, model ) == -1 )
            combined = NULL;
        if ( combined != NULL ) {
            free(friendly_name);
            friendly_name = combined;
        }
    }

    vlc_renderer_item_t *p_renderer_item =
        vlc_renderer_item_new( psz_protocol, friendly_name ? friendly_name : psz_name, uri, extra_uri,
                               demux, icon_uri, renderer_flags );
    if( p_renderer_item == NULL )
        goto error;

    vlc_dictionary_insert( &p_sys->services_name_to_input_item,
        psz_name, p_renderer_item);
    vlc_rd_add_item( p_rd, p_renderer_item );

error:
    free( friendly_name );
    free( icon_uri );
    free( uri );
    free( model );
}

/*****************************************************************************
 * client_callback
 *****************************************************************************/
static void client_callback( AvahiClient *c, AvahiClientState state,
                             void * userdata )
{
    discovery_sys_t *p_sys = userdata;

    if( state == AVAHI_CLIENT_FAILURE &&
        (avahi_client_errno(c) == AVAHI_ERR_DISCONNECTED) )
    {
        msg_Err( p_sys->parent, "avahi client disconnected" );
        avahi_threaded_poll_quit( p_sys->poll );
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
    AvahiLookupResultFlags flags,
    void* userdata )
{
    discovery_sys_t *p_sys = userdata;

    VLC_UNUSED(interface); VLC_UNUSED(host_name);
    VLC_UNUSED(flags);

    if( event == AVAHI_RESOLVER_FAILURE )
    {
        msg_Err( p_sys->parent,
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

        msg_Info( p_sys->parent, "service '%s' of type '%s' in domain '%s' port %i",
                  name, type, domain, port );

        avahi_address_snprint(a, (sizeof(a)/sizeof(a[0]))-1, address);
        if( protocol == AVAHI_PROTO_INET6 )
            if( asprintf( &psz_addr, "[%s]", a ) == -1 )
            {
                avahi_service_resolver_free( r );
                return;
            }

        const char *psz_protocol = NULL;
        bool is_renderer = false;
        for( unsigned int i = 0; i < NB_PROTOCOLS; i++ )
        {
            if( !strcmp(type, protocols[i].psz_service_name) )
            {
                psz_protocol = protocols[i].psz_protocol;
                is_renderer = protocols[i].b_renderer;
                break;
            }
        }
        if( psz_protocol == NULL )
        {
            free( psz_addr );
            avahi_service_resolver_free( r );
            return;
        }

        if( txt != NULL && is_renderer )
        {
            const char* addr_v4v6 = psz_addr != NULL ? psz_addr : a;
            add_renderer( psz_protocol, name, addr_v4v6, port, txt, p_sys );
            free( psz_addr );
            avahi_service_resolver_free( r );
            return;
        }

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
                if( asprintf( &psz_uri, "%s://%s:%d%s",
                          psz_protocol,
                          psz_addr != NULL ? psz_addr : a,
                          port, value ) == -1 )
                {
                    free( psz_addr );
                    avahi_service_resolver_free( r );
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
            if( asprintf( &psz_uri, "%s://%s:%d",
                      psz_protocol,
                      psz_addr != NULL ? psz_addr : a, port ) == -1 )
            {
                free( psz_addr );
                avahi_service_resolver_free( r );
                return;
            }
        }

        free( psz_addr );

        if( psz_uri != NULL )
        {
            p_input = input_item_NewDirectory( psz_uri, name, ITEM_NET );
            free( psz_uri );
        }
        if( p_input != NULL )
        {
            services_discovery_t *p_sd = ( services_discovery_t* )(p_sys->parent);
            vlc_dictionary_insert( &p_sys->services_name_to_input_item,
                name, p_input );
            services_discovery_AddItem( p_sd, p_input );
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
    AvahiLookupResultFlags flags,
    void* userdata )
{
    VLC_UNUSED(b);
    VLC_UNUSED(flags);
    discovery_sys_t *p_sys = userdata;
    if( event == AVAHI_BROWSER_NEW )
    {
        if( avahi_service_resolver_new( p_sys->client, interface, protocol,
                                        name, type, domain, AVAHI_PROTO_UNSPEC,
                                        0,
                                        resolve_callback, userdata ) == NULL )
        {
            msg_Err( p_sys->parent, "failed to resolve service '%s': %s", name,
                     avahi_strerror( avahi_client_errno( p_sys->client ) ) );
        }
    }
    else if( event == AVAHI_BROWSER_REMOVE && name )
    {
        /** \todo Store the input id and search it, rather than searching the items */
        void *p_item;
        p_item = vlc_dictionary_value_for_key(
                        &p_sys->services_name_to_input_item,
                        name );
        if( !p_item )
            msg_Err( p_sys->parent, "failed to find service '%s' in playlist", name );
        else
        {
            if( p_sys->renderer )
            {
                vlc_renderer_discovery_t *p_rd = ( vlc_renderer_discovery_t* )(p_sys->parent);
                vlc_rd_remove_item( p_rd, p_item );
                vlc_renderer_item_release( p_item );
            }
            else
            {
                services_discovery_t *p_sd = ( services_discovery_t* )(p_sys->parent);
                services_discovery_RemoveItem( p_sd, p_item );
                input_item_Release( p_item );
            }
            vlc_dictionary_remove_value_for_key(
                        &p_sys->services_name_to_input_item,
                        name, NULL, NULL );
        }
    }
}

static void clear_input_item( void* p_item, void* p_obj )
{
    VLC_UNUSED( p_obj );
    input_item_Release( p_item );
}

static void clear_renderer_item( void* p_item, void* p_obj )
{
    VLC_UNUSED( p_obj );
    vlc_renderer_item_release( p_item );
}

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int OpenCommon( discovery_sys_t *p_sys )
{
    int err;

    vlc_dictionary_init( &p_sys->services_name_to_input_item, 1 );

    p_sys->poll = avahi_threaded_poll_new();
    if( p_sys->poll == NULL )
    {
        msg_Err( p_sys->parent, "failed to create Avahi threaded poll" );
        goto error;
    }

    p_sys->client = avahi_client_new( avahi_threaded_poll_get(p_sys->poll),
                                      0, client_callback, p_sys, &err );
    if( p_sys->client == NULL )
    {
        msg_Err( p_sys->parent, "failed to create avahi client: %s",
                 avahi_strerror( err ) );
        goto error;
    }

    for( unsigned i = 0; i < NB_PROTOCOLS; i++ )
    {
        if( protocols[i].b_renderer != p_sys->renderer )
            continue;

        AvahiServiceBrowser *sb;
        sb = avahi_service_browser_new( p_sys->client, AVAHI_IF_UNSPEC,
                AVAHI_PROTO_UNSPEC,
                protocols[i].psz_service_name, NULL,
                0, browse_callback, p_sys );
        if( sb == NULL )
        {
            msg_Err( p_sys->parent, "failed to create avahi service browser %s", avahi_strerror( avahi_client_errno(p_sys->client) ) );
            goto error;
        }
    }

    avahi_threaded_poll_start( p_sys->poll );

    return VLC_SUCCESS;

error:
    if( p_sys->client != NULL )
        avahi_client_free( p_sys->client );
    if( p_sys->poll != NULL )
        avahi_threaded_poll_free( p_sys->poll );

    return VLC_EGENERIC;
}

static int OpenSD( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    p_sd->description = _("Zeroconf network services");

    discovery_sys_t *p_sys = p_sd->p_sys = calloc( 1, sizeof( discovery_sys_t ) );
    if( !p_sd->p_sys )
        return VLC_ENOMEM;
    p_sys->parent = p_this;
    p_sys->renderer = false;

    int ret = OpenCommon( p_sys );
    if( ret != VLC_SUCCESS )
    {
        vlc_dictionary_clear( &p_sys->services_name_to_input_item,
                              clear_input_item, NULL );
        free( p_sys );
    }
    return ret;
}

static int OpenRD( vlc_object_t *p_this )
{
    vlc_renderer_discovery_t *p_rd = (vlc_renderer_discovery_t *)p_this;

    discovery_sys_t *p_sys = p_rd->p_sys = calloc( 1, sizeof( discovery_sys_t ) );
    if( !p_rd->p_sys )
        return VLC_ENOMEM;
    p_sys->parent = p_this;
    p_sys->renderer = true;

    int ret = OpenCommon( p_sys );
    if( ret != VLC_SUCCESS )
    {
        vlc_dictionary_clear( &p_sys->services_name_to_input_item,
                              clear_renderer_item, NULL );
        free( p_sys );
    }
    return ret;
}

/*****************************************************************************
 * Close: cleanup
 *****************************************************************************/
static void CloseCommon( discovery_sys_t *p_sys )
{
    avahi_threaded_poll_stop( p_sys->poll );

    avahi_client_free( p_sys->client );
    avahi_threaded_poll_free( p_sys->poll );

}

static void CloseSD( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    discovery_sys_t *p_sys = p_sd->p_sys;
    CloseCommon( p_sys );
    vlc_dictionary_clear( &p_sys->services_name_to_input_item,
                          clear_input_item, NULL );
    free( p_sys );
}

static void CloseRD( vlc_object_t *p_this )
{
    vlc_renderer_discovery_t *p_rd = (vlc_renderer_discovery_t *)p_this;
    discovery_sys_t *p_sys = p_rd->p_sys;
    CloseCommon( p_sys );
    vlc_dictionary_clear( &p_sys->services_name_to_input_item,
                          clear_renderer_item, NULL );
    free( p_sys );
}
