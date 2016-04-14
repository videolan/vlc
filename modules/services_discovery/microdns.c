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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_services_discovery.h>

#include <microdns/microdns.h>

static int Open( vlc_object_t * );
static void Close( vlc_object_t * );

VLC_SD_PROBE_HELPER( "microdns", "mDNS Network Discovery", SD_CAT_LAN )


#define CFG_PREFIX "sd-microdns-"

#define LISTEN_INTERVAL INT64_C(20000000) /* 20 seconds */
#define TIMEOUT (LISTEN_INTERVAL + INT64_C(5000000)) /* interval + 5 seconds */

/*
 * Module descriptor
 */
vlc_module_begin()
    set_shortname( "mDNS" )
    set_description( N_( "mDNS Network Discovery" ) )
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_SD )
    set_capability( "services_discovery", 0 )
    set_callbacks( Open, Close )
    add_shortcut( "mdns", "microdns" )
    VLC_SD_PROBE_SUBMODULE
vlc_module_end ()

struct services_discovery_sys_t
{
    vlc_thread_t        thread;
    atomic_bool         stop;
    struct mdns_ctx *   p_microdns;
    const char **       ppsz_service_names;
    unsigned int        i_nb_service_names;
    vlc_array_t         items;
};

struct item
{
    char *          psz_uri;
    input_item_t *  p_input_item;
    mtime_t         i_last_seen;
};

struct srv
{
    const char *psz_protocol;
    char *      psz_device_name;
    uint16_t    i_port;
};

static const struct
{
    const char *psz_protocol;
    const char *psz_service_name;
    uint16_t    i_default_port;
} protocols[] = {
    { "ftp", "_ftp._tcp.local", 21 },
    { "smb", "_smb._tcp.local", 445 },
    { "nfs", "_nfs._tcp.local", 2049 },
    { "sftp", "_sftp-ssh._tcp.local", 22 },
    { "rtsp", "_rtsp._tcp.local", 554 },
};
#define NB_PROTOCOLS (sizeof(protocols) / sizeof(*protocols))

static const char *const ppsz_options[] = {
    NULL
};

static void
print_error( services_discovery_t *p_sd, const char *psz_what, int i_status )
{
    char psz_err_str[128];

    if( mdns_strerror( i_status, psz_err_str, sizeof(psz_err_str) ) == 0)
        msg_Err( p_sd, "mDNS %s error: %s", psz_what, psz_err_str);
    else
        msg_Err( p_sd, "mDNS %s error: unknown: %d", psz_what, i_status);
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
items_add_input( services_discovery_t *p_sd, char *psz_uri,
                 const char *psz_name )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;

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
    p_item->i_last_seen = mdate();
    vlc_array_append( &p_sys->items, p_item );
    services_discovery_AddItem( p_sd, p_input_item, NULL );

    return VLC_SUCCESS;
}

static void
items_release( services_discovery_t *p_sd, struct item *p_item, bool b_notify )
{
    input_item_Release( p_item->p_input_item );
    if( b_notify )
        services_discovery_RemoveItem( p_sd, p_item->p_input_item );
    free( p_item->psz_uri );
    free( p_item );
}

static bool
items_exists( services_discovery_t *p_sd, const char *psz_uri )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    for( int i = 0; i < vlc_array_count( &p_sys->items ); ++i )
    {
        struct item *p_item = vlc_array_item_at_index( &p_sys->items, i );
        if( strcmp( psz_uri, p_item->psz_uri ) == 0 )
        {
            p_item->i_last_seen = mdate();
            return true;
        }
    }
    return false;
}

static void
items_timeout( services_discovery_t *p_sd )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    mtime_t i_now = mdate();

    /* Remove items that are not seen since TIMEOUT */
    for( int i = 0; i < vlc_array_count( &p_sys->items ); ++i )
    {
        struct item *p_item = vlc_array_item_at_index( &p_sys->items, i );
        if( i_now - p_item->i_last_seen > TIMEOUT )
        {
            items_release( p_sd, p_item, true );
            vlc_array_remove( &p_sys->items, i-- );
        }
    }
}

static void
items_clear( services_discovery_t *p_sd )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    for( int i = 0; i < vlc_array_count( &p_sys->items ); ++i )
    {
        struct item *p_item = vlc_array_item_at_index( &p_sys->items, i );
        items_release( p_sd, p_item, false );
    }
    vlc_array_clear( &p_sys->items );
}

static void
new_entries_cb( void *p_this, int i_status,
                    const struct rr_entry *p_entries )
{
    services_discovery_t *p_sd = p_this;

    if( i_status < 0 )
    {
        print_error( p_sd, "entry callback", i_status );
        return;
    }

    /* Count the number of servers */
    unsigned int i_nb_srv = 0, i_srv_idx = 0;
    for( const struct rr_entry *p_entry = p_entries;
         p_entry != NULL; p_entry = p_entry->next )
    {
        if( p_entry->type == RR_SRV )
            i_nb_srv++;
    }
    if( i_nb_srv == 0 )
        return;

    struct srv *p_srvs = calloc(i_nb_srv, sizeof(struct srv));
    if( p_srvs == NULL )
        return;

    /* There is one ip for several srvs, fetch them */
    const char *psz_ip = NULL;
    for( const struct rr_entry *p_entry = p_entries;
         p_entry != NULL; p_entry = p_entry->next )
    {
        if( p_entry->type == RR_SRV )
        {
            struct srv *p_srv = &p_srvs[i_srv_idx];

            for( unsigned i = 0; i < NB_PROTOCOLS; ++i )
            {
                if( !strrcmp( p_entry->name, protocols[i].psz_service_name )  )
                {
                    p_srv->psz_device_name =
                        strndup( p_entry->name, strlen( p_entry->name )
                                 - strlen( protocols[i].psz_service_name ) - 1);
                    if( p_srv->psz_device_name == NULL )
                        break;
                    p_srv->psz_protocol = protocols[i].psz_protocol;
                    if( protocols[i].i_default_port != p_entry->data.SRV.port )
                        p_srv->i_port = p_entry->data.SRV.port;
                    ++i_srv_idx;
                    break;
                }
            }
        }
        else if( p_entry->type == RR_A && psz_ip == NULL )
            psz_ip = p_entry->data.A.addr_str;
        /* TODO support ipv6
        else if( p_entry->type == RR_AAAA )
            psz_ip = p_entry->data.AAAA.addr_str;
        */
    }
    if( psz_ip == NULL || i_srv_idx == 0 )
    {
        free( p_srvs );
        return;
    }

    /* send new input items (if they don't already exist) */
    for( i_srv_idx = 0; i_srv_idx < i_nb_srv; ++i_srv_idx )
    {
        struct srv *p_srv = &p_srvs[i_srv_idx];
        char psz_port[7]; /* ":65536\0" */
        if( p_srv->i_port != 0 )
            sprintf( psz_port, ":%u", p_srv->i_port );

        char *psz_uri;
        if( asprintf( &psz_uri, "%s://%s%s", p_srv->psz_protocol, psz_ip,
                      p_srv->i_port != 0 ? psz_port : "" ) < 0 )
            continue;

        if( items_exists( p_sd, psz_uri ) )
        {
            free( psz_uri );
            continue;
        }
        items_add_input( p_sd, psz_uri, p_srv->psz_device_name );
    }

    for( i_srv_idx = 0; i_srv_idx < i_nb_srv; ++i_srv_idx )
        free( p_srvs[i_srv_idx].psz_device_name );
    free( p_srvs );
}

static bool
stop_cb( void *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    if( atomic_load( &p_sys->stop ) )
        return true;
    else
    {
        items_timeout( p_sd );
        return false;
    }
}

static void *
Run( void *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    int i_status = mdns_listen( p_sys->p_microdns,
                                p_sys->ppsz_service_names,
                                p_sys->i_nb_service_names,
                                RR_PTR, LISTEN_INTERVAL / INT64_C(1000000),
                                stop_cb, new_entries_cb, p_sd );

    if( i_status < 0 )
        print_error( p_sd, "listen", i_status );

    return NULL;
}

static int
Open( vlc_object_t *p_obj )
{
    services_discovery_t *p_sd = (services_discovery_t *)p_obj;
    services_discovery_sys_t *p_sys = NULL;
    int i_ret = VLC_ENOMEM;

    p_sd->p_sys = p_sys = calloc( 1, sizeof(services_discovery_sys_t) );
    if( !p_sys )
        return VLC_ENOMEM;

    atomic_init( &p_sys->stop, false );
    vlc_array_init( &p_sys->items );
    config_ChainParse( p_sd, CFG_PREFIX, ppsz_options, p_sd->p_cfg );

    /* Listen to protocols that are handled by VLC */
    const unsigned i_count = NB_PROTOCOLS;
    p_sys->ppsz_service_names = calloc( i_count, sizeof(char*) );
    if( !p_sys->ppsz_service_names )
        goto error;

    for( unsigned int i = 0; i < i_count; ++i )
    {
        /* Listen to a protocol only if a module can handle it */
        if( module_exists( protocols[i].psz_protocol ) )
            p_sys->ppsz_service_names[p_sys->i_nb_service_names++] =
                protocols[i].psz_service_name;
    }

    i_ret = VLC_EGENERIC;
    if( p_sys->i_nb_service_names == 0 )
    {
        msg_Err( p_sd, "no services found" );
        goto error;
    }
    for( unsigned int i = 0; i < p_sys->i_nb_service_names; ++i )
        msg_Dbg( p_sd, "mDNS: listening to %s", p_sys->ppsz_service_names[i] );

    int i_status;
    if( ( i_status = mdns_init( &p_sys->p_microdns, MDNS_ADDR_IPV4,
                                MDNS_PORT ) ) < 0 )
    {
        print_error( p_sd, "init", i_status );
        goto error;
    }

    if( vlc_clone( &p_sys->thread, Run, p_sd, VLC_THREAD_PRIORITY_LOW) )
    {
        msg_Err( p_sd, "Can't run the lookup thread" );
        goto error;
    }

    return VLC_SUCCESS;
error:
    if( p_sys->p_microdns != NULL )
        mdns_destroy( p_sys->p_microdns );
    free( p_sys->ppsz_service_names );
    free( p_sys );
    return i_ret;
}

static void
Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = (services_discovery_t *) p_this;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    atomic_store( &p_sys->stop, true );
    vlc_join( p_sys->thread, NULL );

    items_clear( p_sd );
    mdns_destroy( p_sys->p_microdns );

    free( p_sys->ppsz_service_names );
    free( p_sys );
}
