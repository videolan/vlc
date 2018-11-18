/*
 * Transport.cpp
 *****************************************************************************
 * Copyright (C) 2015-2018 VideoLabs, VideoLAN and VLC authors
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Transport.hpp"

using namespace adaptive::http;

Transport::Transport(bool b_secure_)
{
    creds = NULL;
    tls = NULL;
    b_secure = b_secure_;
}

Transport::~Transport()
{
    if(connected())
        disconnect();
}

bool Transport::connect(vlc_object_t *p_object, const std::string &hostname, int port)
{
    if(connected())
        disconnect();

    if(b_secure)
    {
        creds = vlc_tls_ClientCreate(p_object);
        if(!creds)
            return false;
        tls = vlc_tls_SocketOpenTLS(creds, hostname.c_str(), port, "https",
                                    NULL, NULL );
        if(!tls)
        {
            vlc_tls_ClientDelete(creds);
            creds = NULL;
        }
    }
    else
    {
        tls = vlc_tls_SocketOpenTCP(p_object, hostname.c_str(), port);
    }

    return tls != NULL;
}

bool Transport::connected() const
{
    return tls != NULL;
}

void Transport::disconnect()
{
    if(tls)
    {
        vlc_tls_Close(tls);
        tls = NULL;
    }

    if(creds)
    {
        vlc_tls_ClientDelete(creds);
        creds = NULL;
    }
}

ssize_t Transport::read(void *p_buffer, size_t len)
{
    return vlc_tls_Read(tls, p_buffer, len, true);
}

std::string Transport::readline()
{
    char *line = ::vlc_tls_GetLine(tls);
    if(line == NULL)
        return "";

    std::string ret(line);
    ::free(line);
    return ret;
}

bool Transport::send(const void *buf, size_t size)
{
    if (!connected())
        return false;

    if (size == 0)
        return true;

    return vlc_tls_Write(tls, buf, size) == (ssize_t)size;
}

