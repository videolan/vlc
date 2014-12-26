/*
 * HTTPConnection.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
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

#include "HTTPConnection.h"
#include "Chunk.h"

#include <sstream>

using namespace dash::http;

HTTPConnection::HTTPConnection  (stream_t *stream, Chunk *chunk_) :
                IHTTPConnection (stream)
{
    toRead = 0;
    chunk = NULL;
    bindChunk(chunk_);
}

std::string HTTPConnection::buildRequestHeader(const std::string &path) const
{
    std::string req = IHTTPConnection::buildRequestHeader(path);
    return req.append("Connection: close\r\n");
}

void HTTPConnection::bindChunk(Chunk *chunk_)
{
    if(chunk_ == chunk)
        return;
    if (chunk_)
        chunk_->setConnection(this);
    chunk = chunk_;
}

void HTTPConnection::releaseChunk()
{
    if(chunk)
    {
        chunk->setConnection(NULL);
        chunk = NULL;
    }
}

void HTTPConnection::onHeader(const std::string &key,
                              const std::string &value)
{
    if(key == "Content-Length")
    {
        std::istringstream ss(value);
        size_t length;
        ss >> length;
        chunk->setLength(length);
        toRead = length;
    }
}

std::string HTTPConnection::extraRequestHeaders() const
{
    std::stringstream ss;
    if(chunk->usesByteRange())
    {
        ss << "Range: bytes=" << chunk->getStartByte() << "-";
        if(chunk->getEndByte())
            ss << chunk->getEndByte();
        ss << "\r\n";
    }
    return ss.str();
}

bool HTTPConnection::isAvailable() const
{
    return chunk == NULL;
}

void HTTPConnection::disconnect()
{
    toRead = 0;
}
