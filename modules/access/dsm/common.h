/**
 * @file bdsm/common.h
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

#include <bdsm/bdsm.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_services_discovery.h>
#include <vlc_url.h>

int bdsm_SdOpen( vlc_object_t * );
void bdsm_SdClose( vlc_object_t * );
int bdsm_sd_probe_Open( vlc_object_t * );

int BrowserInit( access_t *p_access );

struct access_sys_t
{
    netbios_ns         *p_ns;               /**< Netbios name service */
    smb_session        *p_session;          /**< bdsm SMB Session object */
    smb_creds           creds;              /**< Credentials used to connect */

    vlc_url_t           url;
    char               *psz_share;
    char               *psz_path;

    char                netbios_name[16];
    struct in_addr      addr;

    smb_fd              i_fd;               /**< SMB fd for the file we're reading */
    smb_tid             i_tid;              /**< SMB Tree ID we're connected to */
};

