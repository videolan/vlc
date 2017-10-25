/**
 * @file bdsm/sd.c
 * @brief List host supporting NETBIOS on the local network
 */
/*****************************************************************************
 * Copyright © 2014 Authors and the VideoLAN team
 *
 * Authors: - Julien 'Lta' BALLET <contact # lta 'dot' io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_services_discovery.h>
#include <bdsm/bdsm.h>

#if !defined(BDSM_VERSION_CURRENT) || BDSM_VERSION_CURRENT < 2
# error libdsm version current too low: need at least BDSM_VERSION_CURRENT 2
#endif

#define BROADCAST_TIMEOUT 6 // in seconds

int bdsm_SdOpen( vlc_object_t * );
void bdsm_SdClose( vlc_object_t * );

struct entry_item
{
    netbios_ns_entry *p_entry;
    input_item_t *p_item;
};

struct services_discovery_sys_t
{
    netbios_ns      *p_ns;
    vlc_array_t      entry_item_list;
};

static void entry_item_append( services_discovery_t *p_sd,
                               netbios_ns_entry *p_entry,
                               input_item_t *p_item )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    struct entry_item *p_entry_item = calloc(1, sizeof(struct entry_item));

    if( !p_entry_item )
        return;
    p_entry_item->p_entry = p_entry;
    p_entry_item->p_item = p_item;
    input_item_Hold( p_item );
    vlc_array_append_or_abort( &p_sys->entry_item_list, p_entry_item );
    services_discovery_AddItem( p_sd, p_item );
}

static void entry_item_remove( services_discovery_t *p_sd,
                               netbios_ns_entry *p_entry )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    for( size_t i = 0; i < vlc_array_count( &p_sys->entry_item_list ); i++ )
    {
        struct entry_item *p_entry_item;

        p_entry_item = vlc_array_item_at_index( &p_sys->entry_item_list, i );
        if( p_entry_item->p_entry == p_entry  )
        {
            services_discovery_RemoveItem( p_sd, p_entry_item->p_item );
            input_item_Release( p_entry_item->p_item );
            vlc_array_remove( &p_sys->entry_item_list, i );
            free( p_entry_item );
            break;
        }
    }
}

static void netbios_ns_discover_on_entry_added( void *p_opaque,
                                                netbios_ns_entry *p_entry )
{
    services_discovery_t *p_sd = (services_discovery_t *)p_opaque;

    char type = netbios_ns_entry_type( p_entry );

    if( type == NETBIOS_FILESERVER )
    {
        input_item_t *p_item;
        char *psz_mrl;
        const char *name = netbios_ns_entry_name( p_entry );

        if( asprintf(&psz_mrl, "smb://%s", name) < 0 )
            return;

        p_item = input_item_NewDirectory( psz_mrl, name, ITEM_NET );
        msg_Dbg( p_sd, "Adding item %s", psz_mrl );
        free(psz_mrl);

        entry_item_append( p_sd, p_entry, p_item );
        input_item_Release( p_item );
    }
}

static void netbios_ns_discover_on_entry_removed( void *p_opaque,
                                                  netbios_ns_entry *p_entry )
{
    services_discovery_t *p_sd = (services_discovery_t *)p_opaque;

    entry_item_remove( p_sd, p_entry );
}

int bdsm_SdOpen (vlc_object_t *p_this)
{
    services_discovery_t *p_sd = (services_discovery_t *)p_this;
    services_discovery_sys_t *p_sys = calloc (1, sizeof (*p_sys));
    netbios_ns_discover_callbacks callbacks;

    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_sd->description = _("Windows networks");
    p_sd->p_sys = p_sys;
    vlc_array_init( &p_sys->entry_item_list );

    p_sys->p_ns = netbios_ns_new();
    if( p_sys->p_ns == NULL )
        goto error;

    callbacks.p_opaque = p_sd;
    callbacks.pf_on_entry_added = netbios_ns_discover_on_entry_added;
    callbacks.pf_on_entry_removed = netbios_ns_discover_on_entry_removed;

    if( netbios_ns_discover_start( p_sys->p_ns, BROADCAST_TIMEOUT,
                                   &callbacks) != 0 )
        goto error;

    return VLC_SUCCESS;

    error:
        bdsm_SdClose( p_this );
        return VLC_EGENERIC;
}

void bdsm_SdClose (vlc_object_t *p_this)
{
    services_discovery_t *sd = (services_discovery_t *)p_this;
    services_discovery_sys_t *p_sys = sd->p_sys;

    if( p_sys == NULL )
        return;

    if( p_sys->p_ns )
    {
        netbios_ns_discover_stop( p_sys->p_ns );
        netbios_ns_destroy( p_sys->p_ns );
    }

    for( size_t i = 0; i < vlc_array_count( &p_sys->entry_item_list ); i++ )
    {
        struct entry_item *p_entry_item;

        p_entry_item = vlc_array_item_at_index( &p_sys->entry_item_list, i );
        input_item_Release( p_entry_item->p_item );
        free( p_entry_item );
    }
    vlc_array_clear( &p_sys->entry_item_list );

    free( p_sys );
}
