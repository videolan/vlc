/*****************************************************************************
 * microdns.c: mDNS services discovery module
 *****************************************************************************
 * Copyright © 2016 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
 *          Thomas Guillem <thomas@gllm.fr>
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
# include <config.h>
#endif

#include <stdatomic.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_services_discovery.h>
#include <vlc_renderer_discovery.h>

#include <microdns/microdns.h>

static int OpenSD( vlc_object_t * );
static void CloseSD( vlc_object_t * );
static int OpenRD( vlc_object_t * );
static void CloseRD( vlc_object_t * );

VLC_SD_PROBE_HELPER( "microdns", N_("mDNS Network Discovery"), SD_CAT_LAN )
VLC_RD_PROBE_HELPER( "microdns_renderer", "mDNS renderer Discovery" )

#define CFG_PREFIX "sd-microdns-"

#define LISTEN_INTERVAL VLC_TICK_FROM_SEC(15) /* 15 seconds */
#define TIMEOUT (3 * LISTEN_INTERVAL + VLC_TICK_FROM_SEC(5)) /* 3 * interval + 5 seconds */

/*
 * Module descriptor
 */
vlc_module_begin()
    set_shortname( "mDNS" )
    set_description( N_( "mDNS Network Discovery" ) )
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_SD )
    set_capability( "services_discovery", 0 )
    set_callbacks( OpenSD, CloseSD )
    add_shortcut( "mdns", "microdns" )
    VLC_SD_PROBE_SUBMODULE
    add_submodule() \
        set_description( N_( "mDNS Renderer Discovery" ) )
        set_category( CAT_SOUT )
        set_subcategory( SUBCAT_SOUT_RENDERER )
        set_capability( "renderer_discovery", 0 )
        set_callbacks( OpenRD, CloseRD )
        add_shortcut( "mdns_renderer", "microdns_renderer" )
        VLC_RD_PROBE_SUBMODULE
vlc_module_end ()

static const struct
{
    const char *psz_protocol;
    const char *psz_service_name;
    bool        b_renderer;
    int         i_renderer_flags;
} protocols[] = {
    { "ftp", "_ftp._tcp.local", false, 0 },
    { "smb", "_smb._tcp.local", false, 0 },
    { "nfs", "_nfs._tcp.local", false, 0 },
    { "sftp", "_sftp-ssh._tcp.local", false, 0 },
    { "rtsp", "_rtsp._tcp.local", false, 0 },
    { "chromecast", "_googlecast._tcp.local", true, VLC_RENDERER_CAN_AUDIO },
};
#define NB_PROTOCOLS (sizeof(protocols) / sizeof(*protocols))

struct discovery_sys
{
    vlc_thread_t        thread;
    atomic_bool         stop;
    struct mdns_ctx *   p_microdns;
    const char *        ppsz_service_names[NB_PROTOCOLS];
    unsigned int        i_nb_service_names;
    vlc_array_t         items;
};

struct item
{
    char *              psz_uri;
    input_item_t *      p_input_item;
    vlc_renderer_item_t*p_renderer_item;
    vlc_tick_t          i_last_seen;
};

struct srv
{
    const char *psz_protocol;
    char *      psz_device_name;
    uint16_t    i_port;
    struct
    {
        char *      psz_model;
        char *      psz_icon;
        int         i_renderer_flags;
    } renderer;
};

static const char *const ppsz_options[] = {
    NULL
};

static void
print_error( vlc_object_t *p_obj, const char *psz_what, int i_status )
{
    char psz_err_str[128];

    if( mdns_strerror( i_status, psz_err_str, sizeof(psz_err_str) ) == 0)
        msg_Err( p_obj, "mDNS %s error: %s", psz_what, psz_err_str);
    else
        msg_Err( p_obj, "mDNS %s error: unknown: %d", psz_what, i_status);
}


static int
strrcmp(const char *s1, const char *s2)
{
    size_t m, n;
    m = strlen(s1);
    n = strlen(s2);
    if (n > m)
        return 1;
    return strncmp(s1 + m - n, s2, n);
}

static int
items_add_input( struct discovery_sys *p_sys, services_discovery_t *p_sd,
                 char *psz_uri, const char *psz_name )
{
    struct item *p_item = malloc( sizeof(struct item) );
    if( p_item == NULL )
    {
        free( psz_uri );
        return VLC_ENOMEM;
    }

    input_item_t *p_input_item =
        input_item_NewDirectory( psz_uri, psz_name, ITEM_NET );
    if( p_input_item == NULL )
    {
        free( psz_uri );
        free( p_item );
        return VLC_ENOMEM;
    }

    p_item->psz_uri = psz_uri;
    p_item->p_input_item = p_input_item;
    p_item->p_renderer_item = NULL;
    p_item->i_last_seen = vlc_tick_now();
    vlc_array_append_or_abort( &p_sys->items, p_item );
    services_discovery_AddItem( p_sd, p_input_item );

    return VLC_SUCCESS;
}

static int
items_add_renderer( struct discovery_sys *p_sys, vlc_renderer_discovery_t *p_rd,
                    const char *psz_name, char *psz_uri,
                    const char *psz_demux_filter, const char *psz_icon_uri,
                    int i_flags )
{
    struct item *p_item = malloc( sizeof(struct item) );
    if( p_item == NULL )
        return VLC_ENOMEM;

    const char *psz_extra_uri = i_flags & VLC_RENDERER_CAN_VIDEO ? NULL : "no-video";

    vlc_renderer_item_t *p_renderer_item =
        vlc_renderer_item_new( "chromecast", psz_name, psz_uri, psz_extra_uri,
                               psz_demux_filter, psz_icon_uri, i_flags );
    if( p_renderer_item == NULL )
    {
        free( psz_uri );
        free( p_item );
        return VLC_ENOMEM;
    }

    p_item->psz_uri = psz_uri;
    p_item->p_input_item = NULL;
    p_item->p_renderer_item = p_renderer_item;
    p_item->i_last_seen = vlc_tick_now();
    vlc_array_append_or_abort( &p_sys->items, p_item );
    vlc_rd_add_item( p_rd, p_renderer_item );

    return VLC_SUCCESS;
}

static void
items_release( struct discovery_sys *p_sys, struct item *p_item )
{
    (void) p_sys;
    if( p_item->p_input_item != NULL )
    {
        input_item_Release( p_item->p_input_item );
    }
    else
    {
        assert( p_item->p_renderer_item != NULL );
        vlc_renderer_item_release( p_item->p_renderer_item );
    }

    free( p_item->psz_uri );
    free( p_item );
}

static bool
items_exists( struct discovery_sys *p_sys, const char *psz_uri )
{
    for( size_t i = 0; i < vlc_array_count( &p_sys->items ); ++i )
    {
        struct item *p_item = vlc_array_item_at_index( &p_sys->items, i );
        if( strcmp( p_item->psz_uri, psz_uri ) == 0 )
        {
            p_item->i_last_seen = vlc_tick_now();
            return true;
        }
    }
    return false;
}

static void
items_timeout( struct discovery_sys *p_sys, services_discovery_t *p_sd,
               vlc_renderer_discovery_t *p_rd )
{
    assert( p_rd != NULL || p_sd != NULL );
    vlc_tick_t i_now = vlc_tick_now();

    /* Remove items that are not seen since TIMEOUT */
    for( size_t i = 0; i < vlc_array_count( &p_sys->items ); ++i )
    {
        struct item *p_item = vlc_array_item_at_index( &p_sys->items, i );
        if( i_now - p_item->i_last_seen > TIMEOUT )
        {
            if( p_sd != NULL )
                services_discovery_RemoveItem( p_sd, p_item->p_input_item );
            else
                vlc_rd_remove_item( p_rd, p_item->p_renderer_item );
            items_release( p_sys, p_item );
            vlc_array_remove( &p_sys->items, i-- );
        }
    }
}

static void
items_clear( struct discovery_sys *p_sys )
{
    for( size_t i = 0; i < vlc_array_count( &p_sys->items ); ++i )
    {
        struct item *p_item = vlc_array_item_at_index( &p_sys->items, i );
        items_release( p_sys, p_item );
    }
    vlc_array_clear( &p_sys->items );
}

static void clear_srvs( struct srv *p_srvs, unsigned int i_nb_srv )
{
    for( unsigned int i = 0; i < i_nb_srv; ++i )
    {
        free( p_srvs[i].psz_device_name );
        free( p_srvs[i].renderer.psz_model);
        free( p_srvs[i].renderer.psz_icon );
    }
    free( p_srvs );
}

static int
parse_entries( const struct rr_entry *p_entries, bool b_renderer,
               struct srv **pp_srvs, unsigned int *p_nb_srv,
               const char **ppsz_ip, bool *p_ipv6 )
{
    /* Count the number of servers */
    unsigned int i_nb_srv = 0;
    for( const struct rr_entry *p_entry = p_entries;
         p_entry != NULL; p_entry = p_entry->next )
    {
        if( p_entry->type == RR_SRV )
            i_nb_srv++;
    }
    if( i_nb_srv == 0 )
        return VLC_EGENERIC;

    struct srv *p_srvs = calloc(i_nb_srv, sizeof(struct srv));
    if( p_srvs == NULL )
        return VLC_EGENERIC;

    /* There is one ip for several srvs, fetch them */
    const char *psz_ip = NULL;
    struct srv *p_srv = NULL;
    i_nb_srv = 0;
    for( const struct rr_entry *p_entry = p_entries;
         p_entry != NULL; p_entry = p_entry->next )
    {
        if( p_entry->type == RR_SRV )
        {
            for( unsigned i = 0; i < NB_PROTOCOLS; ++i )
            {
                if( !strrcmp( p_entry->name, protocols[i].psz_service_name ) &&
                    protocols[i].b_renderer == b_renderer )
                {
                    p_srv = &p_srvs[i_nb_srv];

                    p_srv->psz_device_name =
                        strndup( p_entry->name, strlen( p_entry->name )
                                 - strlen( protocols[i].psz_service_name ) - 1);
                    if( p_srv->psz_device_name == NULL )
                        break;
                    p_srv->psz_protocol = protocols[i].psz_protocol;
                    p_srv->i_port = p_entry->data.SRV.port;
                    p_srv->renderer.i_renderer_flags = protocols[i].i_renderer_flags;
                    ++i_nb_srv;
                    break;
                }
            }
        }
        else if( p_entry->type == RR_A && psz_ip == NULL )
            psz_ip = p_entry->data.A.addr_str;
        else if( p_entry->type == RR_AAAA && psz_ip == NULL )
        {
            psz_ip = p_entry->data.AAAA.addr_str;
            *p_ipv6 = true;
        }
        else if( p_entry->type == RR_TXT && p_srv != NULL )
        {
            for ( struct rr_data_txt *p_txt = p_entry->data.TXT;
                  p_txt != NULL ; p_txt = p_txt->next )
            {
                if( !strcmp( p_srv->psz_protocol, "chromecast" ) )
                {
                    if ( !strncmp( "fn=", p_txt->txt, 3 ) )
                    {
                        free( p_srv->psz_device_name );
                        p_srv->psz_device_name = strdup( p_txt->txt + 3 );
                    }
                    else if( !strncmp( "ca=", p_txt->txt, 3 ) )
                    {
                        int ca = atoi( p_txt->txt + 3);
                        /*
                         * For chromecast, the `ca=` is composed from (at least)
                         * 0x01 to indicate video support
                         * 0x04 to indivate audio support
                         */
                        if ( ( ca & 0x01 ) != 0 )
                            p_srv->renderer.i_renderer_flags |= VLC_RENDERER_CAN_VIDEO;
                        if ( ( ca & 0x04 ) != 0 )
                            p_srv->renderer.i_renderer_flags |= VLC_RENDERER_CAN_AUDIO;
                    }
                    else if( !strncmp("md=", p_txt->txt, 3) )
                    {
                        free( p_srv->renderer.psz_model );
                        p_srv->renderer.psz_model = strdup( p_txt->txt + 3 );
                    }
                    else if( !strncmp("ic=", p_txt->txt, 3) )
                    {
                        free( p_srv->renderer.psz_icon );
                        p_srv->renderer.psz_icon = strdup( p_txt->txt + 3 );
                    }
                }
            }
        }
    }
    if( psz_ip == NULL || i_nb_srv == 0 )
    {
        clear_srvs( p_srvs, i_nb_srv );
        return VLC_EGENERIC;
    }

    *pp_srvs = p_srvs;
    *p_nb_srv = i_nb_srv;
    *ppsz_ip = psz_ip;
    return VLC_SUCCESS;
}

static char *
create_uri( const char *psz_protocol, const char *psz_ip, bool b_ipv6,
            uint16_t i_port )
{
    char *psz_uri;

    return asprintf( &psz_uri, "%s://%s%s%s:%u", psz_protocol,
                     b_ipv6 ? "[" : "", psz_ip, b_ipv6 ? "]" : "",
                     i_port ) < 0 ? NULL : psz_uri;
}

static void
new_entries_sd_cb( void *p_this, int i_status, const struct rr_entry *p_entries )
{
    services_discovery_t *p_sd = (services_discovery_t *)p_this;
    struct discovery_sys *p_sys = p_sd->p_sys;
    if( i_status < 0 )
    {
        print_error( VLC_OBJECT( p_sd ), "entry callback", i_status );
        return;
    }

    struct srv *p_srvs;
    unsigned i_nb_srv;
    const char *psz_ip;
    bool b_ipv6 = false;
    if( parse_entries( p_entries, false, &p_srvs, &i_nb_srv,
                       &psz_ip, &b_ipv6 ) != VLC_SUCCESS )
        return;

    /* send new input items (if they don't already exist) */
    for( unsigned int i = 0; i < i_nb_srv; ++i )
    {
        struct srv *p_srv = &p_srvs[i];
        char *psz_uri = create_uri( p_srv->psz_protocol, psz_ip, b_ipv6,
                                    p_srv->i_port );

        if( psz_uri == NULL )
            break;

        if( items_exists( p_sys, psz_uri ) )
        {
            free( psz_uri );
            continue;
        }
        items_add_input( p_sys, p_sd, psz_uri, p_srv->psz_device_name );
    }

    clear_srvs( p_srvs, i_nb_srv );
}


static bool
stop_sd_cb( void *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    struct discovery_sys *p_sys = p_sd->p_sys;

    if( atomic_load( &p_sys->stop ) )
        return true;
    else
    {
        items_timeout( p_sys, p_sd, NULL );
        return false;
    }
}

static void *
RunSD( void *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    struct discovery_sys *p_sys = p_sd->p_sys;

    int i_status = mdns_listen( p_sys->p_microdns,
                                p_sys->ppsz_service_names,
                                p_sys->i_nb_service_names,
                                RR_PTR, SEC_FROM_VLC_TICK(LISTEN_INTERVAL),
                                stop_sd_cb, new_entries_sd_cb, p_sd );

    if( i_status < 0 )
        print_error( VLC_OBJECT( p_sd ), "listen", i_status );

    return NULL;
}

static void
new_entries_rd_cb( void *p_this, int i_status, const struct rr_entry *p_entries )
{
    vlc_renderer_discovery_t *p_rd = (vlc_renderer_discovery_t *)p_this;
    struct discovery_sys *p_sys = p_rd->p_sys;
    if( i_status < 0 )
    {
        print_error( VLC_OBJECT( p_rd ), "entry callback", i_status );
        return;
    }

    struct srv *p_srvs;
    unsigned i_nb_srv;
    const char *psz_ip;
    bool b_ipv6 = false;
    if( parse_entries( p_entries, true, &p_srvs, &i_nb_srv,
                       &psz_ip, &b_ipv6 ) != VLC_SUCCESS )
        return;

    /* send new input items (if they don't already exist) */
    for( unsigned int i = 0; i < i_nb_srv; ++i )
    {
        struct srv *p_srv = &p_srvs[i];
        char *psz_icon_uri = NULL;
        char *psz_uri = create_uri( p_srv->psz_protocol, psz_ip, b_ipv6,
                                    p_srv->i_port );
        const char *psz_demux_filter = NULL;

        if( psz_uri == NULL )
            break;

        if( items_exists( p_sys, psz_uri ) )
        {
            free( psz_uri );
            continue;
        }

        if( p_srv->renderer.psz_icon != NULL
         && asprintf( &psz_icon_uri, "http://%s:8008%s", psz_ip, p_srv->renderer.psz_icon )
                      == -1 )
        {
            free( psz_uri );
            break;
        }
        if( p_srv->renderer.psz_model != NULL )
        {
            char* psz_name;
            if ( asprintf( &psz_name, "%s (%s)", p_srv->psz_device_name,
                           p_srv->renderer.psz_model ) > 0 )
            {
                free( p_srv->psz_device_name );
                p_srv->psz_device_name = psz_name;
            }
        }

        if( strcmp( p_srv->psz_protocol, "chromecast" ) == 0)
            psz_demux_filter = "cc_demux";

        items_add_renderer( p_sys, p_rd, p_srv->psz_device_name, psz_uri,
                            psz_demux_filter, psz_icon_uri,
                            p_srv->renderer.i_renderer_flags );
        free(psz_icon_uri);
    }

    clear_srvs( p_srvs, i_nb_srv );
}

static bool
stop_rd_cb( void *p_this )
{
    vlc_renderer_discovery_t *p_rd = p_this;
    struct discovery_sys *p_sys = p_rd->p_sys;

    if( atomic_load( &p_sys->stop ) )
        return true;
    else
    {
        items_timeout( p_sys, NULL, p_rd );
        return false;
    }
}

static void *
RunRD( void *p_this )
{
    vlc_renderer_discovery_t *p_rd = p_this;
    struct discovery_sys *p_sys = p_rd->p_sys;

    int i_status = mdns_listen( p_sys->p_microdns,
                                p_sys->ppsz_service_names,
                                p_sys->i_nb_service_names,
                                RR_PTR, SEC_FROM_VLC_TICK(LISTEN_INTERVAL),
                                stop_rd_cb, new_entries_rd_cb, p_rd );

    if( i_status < 0 )
        print_error( VLC_OBJECT( p_rd ), "listen", i_status );

    return NULL;
}

static int
OpenCommon( vlc_object_t *p_obj, struct discovery_sys *p_sys, bool b_renderer )
{
    int i_ret = VLC_EGENERIC;
    atomic_init( &p_sys->stop, false );
    vlc_array_init( &p_sys->items );

    /* Listen to protocols that are handled by VLC */
    for( unsigned int i = 0; i < NB_PROTOCOLS; ++i )
    {
        if( protocols[i].b_renderer == b_renderer )
            p_sys->ppsz_service_names[p_sys->i_nb_service_names++] =
                protocols[i].psz_service_name;
    }

    if( p_sys->i_nb_service_names == 0 )
    {
        msg_Err( p_obj, "no services found" );
        goto error;
    }
    for( unsigned int i = 0; i < p_sys->i_nb_service_names; ++i )
        msg_Dbg( p_obj, "mDNS: listening to %s %s", p_sys->ppsz_service_names[i],
                 b_renderer ? "renderer" : "service" );

    int i_status;
    if( ( i_status = mdns_init( &p_sys->p_microdns, MDNS_ADDR_IPV4,
                                MDNS_PORT ) ) < 0 )
    {
        print_error( p_obj, "init", i_status );
        goto error;
    }

    if( vlc_clone( &p_sys->thread, b_renderer ? RunRD : RunSD, p_obj,
                   VLC_THREAD_PRIORITY_LOW) )
    {
        msg_Err( p_obj, "Can't run the lookup thread" );
        goto error;
    }

    return VLC_SUCCESS;
error:
    if( p_sys->p_microdns != NULL )
        mdns_destroy( p_sys->p_microdns );
    free( p_sys );
    return i_ret;
}

static void
CleanCommon( struct discovery_sys *p_sys )
{
    atomic_store( &p_sys->stop, true );
    vlc_join( p_sys->thread, NULL );

    items_clear( p_sys );
    mdns_destroy( p_sys->p_microdns );
    free( p_sys );
}

static int
OpenSD( vlc_object_t *p_obj )
{
    services_discovery_t *p_sd = (services_discovery_t *)p_obj;

    struct discovery_sys *p_sys = calloc( 1, sizeof(struct discovery_sys) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_sd->p_sys = p_sys;

    p_sd->description = _("mDNS Network Discovery");
    config_ChainParse( p_sd, CFG_PREFIX, ppsz_options, p_sd->p_cfg );

    return OpenCommon( p_obj, p_sys, false );
}

static void
CloseSD( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = (services_discovery_t *) p_this;
    struct discovery_sys *p_sys = p_sd->p_sys;

    CleanCommon( p_sys );
}

static int
OpenRD( vlc_object_t *p_obj )
{
    vlc_renderer_discovery_t *p_rd = (vlc_renderer_discovery_t *)p_obj;

    struct discovery_sys *p_sys = calloc( 1, sizeof(struct discovery_sys) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_rd->p_sys = p_sys;

    config_ChainParse( p_rd, CFG_PREFIX, ppsz_options, p_rd->p_cfg );

    return OpenCommon( p_obj, p_sys, true );
}

static void
CloseRD( vlc_object_t *p_this )
{
    vlc_renderer_discovery_t *p_rd = (vlc_renderer_discovery_t *) p_this;
    struct discovery_sys *p_sys = p_rd->p_sys;

    CleanCommon( p_sys );
}
