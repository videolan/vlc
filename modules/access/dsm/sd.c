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

int bdsm_SdOpen( vlc_object_t * );
void bdsm_SdClose( vlc_object_t * );
int bdsm_sd_probe_Open( vlc_object_t * );

struct services_discovery_sys_t
{
    netbios_ns      *p_ns;
    vlc_thread_t    thread;

    atomic_bool     stop;
};

int bdsm_sd_probe_Open (vlc_object_t *p_this)
{
    vlc_probe_t *p_probe = (vlc_probe_t *)p_this;

    vlc_sd_probe_Add( p_probe, "dsm{longname=\"Windows networks\"}",
                      N_( "Windows networks" ), SD_CAT_LAN );

    return VLC_PROBE_CONTINUE;
}

static void *Run( void *data )
{
    services_discovery_t *p_sd = data;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    if( !netbios_ns_discover( p_sys->p_ns ) )
        return NULL;

    if (atomic_load(&p_sys->stop))
        return NULL;

    for( ssize_t i = 0; i < netbios_ns_entry_count( p_sys->p_ns ); i++ )
    {
        netbios_ns_entry *p_entry = netbios_ns_entry_at( p_sys->p_ns, i );
        char type = netbios_ns_entry_type( p_entry );

        if( type == NETBIOS_FILESERVER )
        {
            input_item_t *p_item;
            char *psz_mrl;
            const char *name = netbios_ns_entry_name( p_entry );

            if( asprintf(&psz_mrl, "smb://%s", name) < 0 )
                return NULL;

            p_item = input_item_NewWithType( psz_mrl, name, 0, NULL,
                                             0, -1, ITEM_TYPE_NODE );
            msg_Dbg( p_sd, "Adding item %s", psz_mrl );

            services_discovery_AddItem( p_sd, p_item, NULL );

        }
    }
    return NULL;
}

int bdsm_SdOpen (vlc_object_t *p_this)
{
    services_discovery_t *p_sd = (services_discovery_t *)p_this;
    services_discovery_sys_t *p_sys = calloc (1, sizeof (*p_sys));

    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_sd->p_sys = p_sys;

    p_sys->p_ns = netbios_ns_new();
    if( p_sys->p_ns == NULL )
        goto error;

    atomic_store(&p_sys->stop, false);

    if( vlc_clone( &p_sys->thread, Run, p_sd, VLC_THREAD_PRIORITY_LOW ) )
    {
        p_sys->thread = 0;
        goto error;
    }

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

    if( p_sys->thread ) {
        atomic_store(&p_sys->stop, true);

        if( p_sys->p_ns )
            netbios_ns_abort( p_sys->p_ns );
        vlc_join( p_sys->thread, NULL );
    }
    if( p_sys->p_ns )
        netbios_ns_destroy( p_sys->p_ns );

    free( p_sys );
}

