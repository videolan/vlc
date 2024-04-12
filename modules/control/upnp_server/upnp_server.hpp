/*****************************************************************************
 * upnp_server.hpp : UPnP server module header
 *****************************************************************************
 * Copyright © 2024 VLC authors and VideoLAN
 *
 * Authors: Hamza Parnica <hparnica@gmail.com>
 *          Alaric Senat <alaric@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef UPNP_SERVER_HPP
#define UPNP_SERVER_HPP

#define SERVER_PREFIX "upnp-server-"

#define SERVER_DEFAULT_NAME N_("VLC Media Server")
#define SERVER_NAME_DESC N_("Upnp server name")
#define SERVER_NAME_LONGTEXT N_("The client exposed upnp server name")

struct vlc_object_t;

namespace Server
{
int open(vlc_object_t *p_this);
void close(vlc_object_t *p_this);
} // namespace Server

#endif /* UPNP_SERVER_HPP */
