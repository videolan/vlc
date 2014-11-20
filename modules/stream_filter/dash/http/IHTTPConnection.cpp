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

#include <sstream>

using namespace dash::http;

std::string IHTTPConnection::getRequestHeader(const Chunk *chunk) const
{
    std::string request;
    if(!chunk->usesByteRange())
    {
        request = "GET "    + chunk->getPath()     + " HTTP/1.1" + "\r\n" +
                  "Host: "  + chunk->getHostname() + "\r\n";
    }
    else
    {
        std::stringstream req;
        req << "GET " << chunk->getPath() << " HTTP/1.1\r\n" <<
               "Host: " << chunk->getHostname() << "\r\n" <<
               "Range: bytes=" << chunk->getStartByte() << "-" << chunk->getEndByte() << "\r\n";

        request = req.str();
    }
    return request;
}
