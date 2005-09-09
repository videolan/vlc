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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Includes
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <avahi-client/client.h>
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
    set_description( _("Bonjour services") );
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
    /* playlist node */
    playlist_item_t     *p_node;
    playlist_t          *p_playlist;

    AvahiSimplePoll     *simple_poll;
    AvahiClient         *client;
    AvahiServiceBrowser *sb;
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

    if( state == AVAHI_CLIENT_DISCONNECTED )
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
    void* userdata )
{
    services_discovery_t *p_sd = ( services_discovery_t* )userdata;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    if( event == AVAHI_RESOLVER_TIMEOUT )
    {
        msg_Err( p_sd,
                 "failed to resolve service '%s' of type '%s' in domain '%s'",
                 name, type, domain );
    }
    else if( event == AVAHI_RESOLVER_FOUND )
    {
        char a[128];
        char *psz_uri = NULL;
        AvahiStringList *asl;
        playlist_item_t *p_item = NULL;

        msg_Dbg( p_sd, "service '%s' of type '%s' in domain '%s'",
                 name, type, domain );

        avahi_address_snprint(a, (sizeof(a)/sizeof(a[0]))-1, address);

        asl = avahi_string_list_find( txt, "path" );
        if( asl != NULL )
        {
            size_t size;
            char *key = NULL;
            char *value = NULL;
            if( avahi_string_list_get_pair( asl, &key, &value, &size ) == 0 )
                asprintf( &psz_uri, "http://%s:%d%s", a, port, value );
        }
        else
        {
            asprintf( &psz_uri, "http://%s:%d", a, port );
        }

        if( psz_uri != NULL )
            p_item = playlist_ItemNew( p_sd, psz_uri, name );
        if( p_item != NULL )
        {
            p_item->i_flags &= ~PLAYLIST_SKIP_FLAG;

            playlist_NodeAddItem( p_sys->p_playlist, p_item,
                                  VIEW_CATEGORY, p_sys->p_node,
                                  PLAYLIST_APPEND, PLAYLIST_END );
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
    void* userdata )
{
    services_discovery_t *p_sd = ( services_discovery_t* )userdata;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    if( event == AVAHI_BROWSER_NEW )
    {
        if( avahi_service_resolver_new( p_sys->client, interface, protocol,
                                        name, type, domain, AVAHI_PROTO_UNSPEC,
                                        resolve_callback, userdata ) == NULL )
        {
            msg_Err( p_sd, "failed to resolve service '%s': %s", name,
                     avahi_strerror( avahi_client_errno( p_sys->client ) ) );
        }
    }
    else
    {
        playlist_item_t *p_item;

        p_item = playlist_ChildSearchName( p_sys->p_node, name );
        if( p_item == NULL )
        {
            msg_Err( p_sd, "failed to find service '%s' in playlist", name );
        }
        else
        {
            playlist_Delete( p_sys->p_playlist, p_item->input.i_id );
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
    playlist_view_t *p_view;
    vlc_value_t val;
    int err;

    p_sd->p_sys = p_sys = (services_discovery_sys_t *)malloc(
        sizeof( services_discovery_sys_t ) );
    if( p_sd->p_sys == NULL )
    {
        msg_Err( p_sd, "out of memory" );
        return VLC_EGENERIC;
    }

    memset( p_sys, 0, sizeof(*p_sys) );

    p_sys->simple_poll = avahi_simple_poll_new();
    if( p_sys->simple_poll == NULL )
    {
        msg_Err( p_sd, "failed to create avahi simple poll" );
        goto error;
    }

    p_sys->client = avahi_client_new( avahi_simple_poll_get(p_sys->simple_poll),
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
                                           browse_callback, p_sd );
    if( p_sys->sb == NULL )
    {
        msg_Err( p_sd, "failed to create avahi service browser" );
        goto error;
    }

    /* Create our playlist node */
    p_sys->p_playlist = (playlist_t *)vlc_object_find( p_sd,
                                                       VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( !p_sys->p_playlist )
    {
        msg_Warn( p_sd, "unable to find playlist, cancelling");
        goto error;
    }

    p_view = playlist_ViewFind( p_sys->p_playlist, VIEW_CATEGORY );
    p_sys->p_node = playlist_NodeCreate( p_sys->p_playlist, VIEW_CATEGORY,
                                         _("Bonjour"), p_view->p_root );

    p_sys->p_node->i_flags |= PLAYLIST_RO_FLAG;
    val.b_bool = VLC_TRUE;
    var_Set( p_sys->p_playlist, "intf-change", val );

    p_sd->pf_run = Run;

    return VLC_SUCCESS;

error:
    if( p_sys->p_playlist != NULL )
        vlc_object_release( p_sys->p_playlist );
    if( p_sys->sb != NULL )
        avahi_service_browser_free( p_sys->sb );
    if( p_sys->client != NULL )
        avahi_client_free( p_sys->client );
    if( p_sys->simple_poll != NULL )
        avahi_simple_poll_free( p_sys->simple_poll );

    free( (void *)p_sys );

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

    playlist_NodeDelete( p_sys->p_playlist, p_sys->p_node, VLC_TRUE, VLC_TRUE );
    vlc_object_release( p_sys->p_playlist );

    free( p_sys );
}

/*****************************************************************************
 * Run: main thread
 *****************************************************************************/
static void Run( services_discovery_t *p_sd )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    while( !p_sd->b_die )
    {
        if( avahi_simple_poll_iterate( p_sys->simple_poll, 100 ) != 0 )
        {
            msg_Err( p_sd, "poll iterate failed" );
            break;
        }
    }
}
