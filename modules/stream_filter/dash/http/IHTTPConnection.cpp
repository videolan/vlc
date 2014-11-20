/*
 * IHTTPConnection.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN Authors
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

#include "IHTTPConnection.h"
#include "Chunk.h"
#include "Helper.h"
#include "dash.hpp"

#include <vlc_network.h>

#include <sstream>

using namespace dash::http;

IHTTPConnection::IHTTPConnection(stream_t *stream_)
{
    stream = stream_;
    httpSocket = -1;
}

IHTTPConnection::~IHTTPConnection()
{

}

bool IHTTPConnection::init(Chunk *chunk)
{
    if(chunk == NULL)
        return false;

    if(!chunk->hasHostname())
    {
        chunk->setUrl(getUrlRelative(chunk));
        if(!chunk->hasHostname())
            return false;
    }

    httpSocket = net_ConnectTCP(stream, chunk->getHostname().c_str(), chunk->getPort());

    if(httpSocket == -1)
        return false;

    return send(getRequestHeader(chunk).append("\r\n"));
}

std::string IHTTPConnection::getRequestHeader(const Chunk *chunk) const
{
    std::stringstream req;
    req << "GET " << chunk->getPath() << " HTTP/1.1\r\n" <<
           "Host: " << chunk->getHostname() << "\r\n" <<
           "User-Agent: " << std::string(stream->p_sys->psz_useragent) << "\r\n";

    if(chunk->usesByteRange())
        req << "Range: bytes=" << chunk->getStartByte() << "-" << chunk->getEndByte() << "\r\n";

    return req.str();
}

std::string IHTTPConnection::getUrlRelative(const Chunk *chunk) const
{
    std::stringstream ss;
    ss << stream->psz_access << "://" << Helper::combinePaths(Helper::getDirectoryPath(stream->psz_path), chunk->getUrl());
    return ss.str();
}
