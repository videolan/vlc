/*
 * Chunk.cpp
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

#include "Chunk.h"
#include "HTTPConnection.hpp"
#include "HTTPConnectionManager.h"

#include <vlc_common.h>
#include <vlc_url.h>
#include <vlc_block.h>

using namespace adaptative::http;

AbstractChunkSource::AbstractChunkSource()
{
    parentChunk = NULL;
}

AbstractChunkSource::~AbstractChunkSource()
{

}

void AbstractChunkSource::setParentChunk(AbstractChunk *parent)
{
    parentChunk = parent;
}


AbstractChunk::AbstractChunk(AbstractChunkSource *source_)
{
    length = 0;
    bytesRead = 0;
    source = source_;
    source->setParentChunk(this);
}

AbstractChunk::~AbstractChunk()
{
    delete source;
}

void AbstractChunk::setLength(size_t length)
{
    this->length = length;
}

size_t AbstractChunk::getBytesRead() const
{
    return this->bytesRead;
}

void AbstractChunk::setBytesRead(size_t bytes)
{
    this->bytesRead = bytes;
}

size_t AbstractChunk::getBytesToRead() const
{
    return length - bytesRead;
}

void AbstractChunk::setBytesRange(const BytesRange &range)
{
    bytesRange = range;
    if(bytesRange.isValid() && bytesRange.getEndByte())
        setLength(bytesRange.getEndByte() - bytesRange.getStartByte());
}

const BytesRange & AbstractChunk::getBytesRange() const
{
    return bytesRange;
}

block_t * AbstractChunk::read(size_t sizehint, mtime_t *time)
{
    if(!source)
        return NULL;

    *time = mdate();
    block_t *block = source->read(sizehint);
    *time = mdate() - *time;

    if(block)
    {
        setBytesRead(getBytesRead() + block->i_buffer);
        onDownload(&block);
    }

    return block;
}

HTTPChunkSource::HTTPChunkSource(const std::string& url, HTTPConnectionManager *manager) :
    AbstractChunkSource(),
    port         (0),
    connection   (NULL),
    connManager  (manager)
{
    if(!init(url))
        throw VLC_EGENERIC;
}

HTTPChunkSource::~HTTPChunkSource()
{
    if(connection)
        connection->setUsed(false);
}

bool HTTPChunkSource::init(const std::string &url)
{
    this->url = url;

    std::size_t pos = url.find("://");
    if(pos != std::string::npos)
    {
        scheme = url.substr(0, pos);
    }

    if(scheme != "http" && scheme != "https")
        return false;

    vlc_url_t url_components;
    vlc_UrlParse(&url_components, url.c_str());

    if(url_components.psz_path)
        path = url_components.psz_path;
    if(url_components.psz_option)
    {
        path += "?";
        path += url_components.psz_option;
    }
    port = url_components.i_port ? url_components.i_port :
                         ((scheme == "https") ? 443 : 80);
    if(url_components.psz_host)
        hostname = url_components.psz_host;

    vlc_UrlClean(&url_components);

    if(path.empty() || hostname.empty())
        return false;

    return true;
}

block_t * HTTPChunkSource::read(size_t maxread)
{
    if(!connManager || !parentChunk)
        return NULL;

    if(!connection)
    {
        connection = connManager->getConnection(scheme, hostname, port);
        if(!connection)
            return NULL;
    }

    if(parentChunk->getBytesRead() == 0)
    {
        if( connection->query(path, parentChunk->getBytesRange()) != VLC_SUCCESS )
            return NULL;
        parentChunk->setLength(connection->getContentLength());
    }

    if( parentChunk->getBytesToRead() == 0 )
        return NULL;

    /* Because we don't know Chunk size at start, we need to get size
       from content length */
    size_t readsize = parentChunk->getBytesToRead();
    if (readsize > maxread)
        readsize = maxread;

    block_t *block = block_Alloc(readsize);
    if(!block)
        return NULL;

    ssize_t ret = connection->read(block->p_buffer, readsize);
    if(ret < 0)
    {
        block_Release(block);
        return NULL;
    }

    block->i_buffer = (size_t)ret;

    return block;
}


HTTPChunk::HTTPChunk(const std::string &url, HTTPConnectionManager *manager):
    AbstractChunk(new HTTPChunkSource(url, manager))
{

}

HTTPChunk::~HTTPChunk()
{

}
