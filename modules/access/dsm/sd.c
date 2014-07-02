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

#include "common.h"

struct services_discovery_sys_t
{
    netbios_ns      *ns;
};

int vlc_sd_probe_Open (vlc_object_t *p_this)
{
    vlc_probe_t *p_probe = (vlc_probe_t *)p_this;

    vlc_sd_probe_Add( p_probe, "dsm{longname=\"Windows networks\"}",
                      N_( "Windows networks" ), SD_CAT_LAN );

    return VLC_PROBE_CONTINUE;
}

int SdOpen (vlc_object_t *p_this)
{
    services_discovery_t *p_sd = (services_discovery_t *)p_this;
    services_discovery_sys_t *p_sys = malloc (sizeof (*p_sys));

    if( p_sys == NULL )
        return VLC_ENOMEM;
    p_sd->p_sys = p_sys;

    /* Let's create a NETBIOS name service object */
    p_sys->ns = netbios_ns_new();
    if( p_sys->ns == NULL )
        goto error;

    if( !netbios_ns_discover( p_sys->ns ) )
        goto error;

    for( ssize_t i = 0; i < netbios_ns_entry_count( p_sys->ns ); i++ )
    {
        netbios_ns_entry *p_entry = netbios_ns_entry_at( p_sys->ns, i );

        if( p_entry->type == 0x20 )
        {
            input_item_t *p_item;
            char *psz_mrl;

            if( asprintf(&psz_mrl, "smb://%s", p_entry->name) < 0 )
                goto error;

            p_item = input_item_NewWithType( psz_mrl, p_entry->name, 0, NULL,
                                             0, -1, ITEM_TYPE_NODE );
            msg_Dbg( p_sd, "Adding item %s", psz_mrl );

            services_discovery_AddItem( p_sd, p_item, NULL );

            free( psz_mrl );
        }
    }

    return VLC_SUCCESS;

    error:
        if( p_sys->ns != NULL )
            netbios_ns_destroy( p_sys->ns );
        free( p_sys );
        p_sd->p_sys = NULL;

        return VLC_EGENERIC;
}

void SdClose (vlc_object_t *p_this)
{
    services_discovery_t *sd = (services_discovery_t *)p_this;
    services_discovery_sys_t *p_sys = sd->p_sys;

    if( p_sys == NULL )
        return;

    if( p_sys->ns != NULL )
        netbios_ns_destroy( p_sys->ns );
}

