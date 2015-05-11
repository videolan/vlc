/*
 * Sockets.cpp
 *****************************************************************************
 * Copyright (C) 2015 - VideoLAN authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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
#include "Sockets.hpp"

#include <vlc_network.h>
#include <cerrno>

using namespace adaptative::http;

Socket::Socket()
{
    netfd = -1;
}

Socket::~Socket()
{
    disconnect();
}

bool Socket::connect(vlc_object_t *stream, const std::string &hostname, int port)
{
    netfd = net_ConnectTCP(stream, hostname.c_str(), port);

    if(netfd == -1)
        return false;

    return true;
}

bool Socket::connected() const
{
    return (netfd != -1);
}

void Socket::disconnect()
{
    if (netfd >= 0)
    {
        net_Close(netfd);
        netfd = -1;
    }
}

ssize_t Socket::read(vlc_object_t *stream, void *p_buffer, size_t len, bool retry)
{
    ssize_t size;
    do
    {
        size = net_Read(stream, netfd, NULL, p_buffer, len, retry);
    } while (size < 0 && (errno == EINTR || errno==EAGAIN) );
    return size;
}

std::string Socket::readline(vlc_object_t *stream)
{
    char *line = ::net_Gets(stream, netfd, NULL);
    if(line == NULL)
        return "";
    std::string ret(line);
    ::free(line);
    return ret;
}

bool Socket::send(vlc_object_t *stream, const void *buf, size_t size)
{
    if (netfd == -1)
        return false;

    if (size == 0)
        return true;

    ssize_t ret = net_Write(stream, netfd, NULL, buf, size);
    if (ret <= 0)
        return false;

    if ( (size_t)ret < size )
        send( stream, ((uint8_t*)buf) + ret, size - ret );

    return true;
}

