/*****************************************************************************
 * Clients.cpp
 *****************************************************************************
 * Copyright © 2024 VLC authors and VideoLAN
 *
 * Authors: Alaric Senat <alaric@videolabs.io>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>
#include <upnp/upnp.h>

#if defined(_WIN32)
#include <winsock2.h>
#elif defined(HAVE_SYS_SOCKET_H)
#include <sys/socket.h>
#endif

#include "utils.hpp"

namespace utils
{
std::string get_server_url()
{
    // TODO support ipv6
    const std::string addr = UpnpGetServerIpAddress();
    const std::string port = std::to_string(UpnpGetServerPort());
    return "http://" + addr + ':' + port + '/';
}

std::string addr_to_string(const sockaddr_storage *addr)
{
    const void *ip;
    if (addr->ss_family == AF_INET6)
    {
        ip = &reinterpret_cast<const struct sockaddr_in6 *>(addr)->sin6_addr;
    }
    else
    {
        ip = &reinterpret_cast<const struct sockaddr_in *>(addr)->sin_addr;
    }

    char buff[INET6_ADDRSTRLEN];
    inet_ntop(addr->ss_family, ip, buff, sizeof(buff));
    return buff;
}
} // namespace utils
