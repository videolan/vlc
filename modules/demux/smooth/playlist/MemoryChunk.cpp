/*
 * MemoryChunk.cpp
 *****************************************************************************
 * Copyright (C) 2015 - VideoLAN Authors
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

#include "MemoryChunk.hpp"

#include <vlc_block.h>

using namespace smooth::http;

MemoryChunkSource::MemoryChunkSource(block_t *block)
{
    data = block;
    i_read = 0;
    contentLength = data->i_buffer;
}

MemoryChunkSource::~MemoryChunkSource()
{
    if(data)
        block_Release(data);
}

bool MemoryChunkSource::hasMoreData() const
{
    return i_read > contentLength;
}

block_t * MemoryChunkSource::readBlock()
{
    block_t *p_block = NULL;
    if(data)
    {
        p_block = data;
        data = NULL;
    }
    return p_block;
}

block_t * MemoryChunkSource::read(size_t toread)
{
    if(!data)
        return NULL;

    block_t * p_block = NULL;

    toread = __MIN(data->i_buffer - i_read, toread);
    if(toread > 0)
    {
        if((p_block = block_Alloc(toread)))
        {
            memcpy(p_block->p_buffer, &data->p_buffer[i_read], toread);
            p_block->i_buffer = toread;
            i_read += toread;
        }
    }

    return p_block;
}
