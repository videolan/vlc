/*
 * SourceStream.cpp
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC Authors
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

#include "SourceStream.hpp"

#include "../ChunksSource.hpp"
#include "../http/Chunk.h"
#include <vlc_stream.h>
#include <vlc_demux.h>

using namespace adaptive;

ChunksSourceStream::ChunksSourceStream(vlc_object_t *p_obj_, ChunksSource *source_)
    : b_eof( false )
    , p_obj( p_obj_ )
    , source( source_ )
    , p_block( NULL )
{ }

ChunksSourceStream::~ChunksSourceStream()
{
    Reset();
}

void ChunksSourceStream::Reset()
{
    if(p_block)
        block_Release(p_block);
    p_block = NULL;
    b_eof = false;
}

stream_t * ChunksSourceStream::makeStream()
{
    stream_t *p_stream = vlc_stream_CommonNew( p_obj, delete_Callback );
    if(p_stream)
    {
        p_stream->pf_control = control_Callback;
        p_stream->pf_read = read_Callback;
        p_stream->pf_readdir = NULL;
        p_stream->pf_seek = seek_Callback;
        p_stream->p_sys = this;
    }
    return p_stream;
}

std::string ChunksSourceStream::getContentType()
{
    if(!b_eof && !p_block)
    {
        p_block = source->readNextBlock();
        b_eof = !p_block;
    }
    return source->getContentType();
}

ssize_t ChunksSourceStream::Read(uint8_t *buf, size_t size)
{
    size_t i_copied = 0;
    size_t i_toread = size;

    while(i_toread && !b_eof)
    {
        if(!p_block && !(p_block = source->readNextBlock()))
        {
            b_eof = true;
            break;
        }

        if(p_block->i_buffer > i_toread)
        {
            if(buf)
                memcpy(buf + i_copied, p_block->p_buffer, i_toread);
            i_copied += i_toread;
            p_block->p_buffer += i_toread;
            p_block->i_buffer -= i_toread;
            i_toread = 0;
        }
        else
        {
            if(buf)
                memcpy(buf + i_copied, p_block->p_buffer, p_block->i_buffer);
            i_copied += p_block->i_buffer;
            i_toread -= p_block->i_buffer;
            block_Release(p_block);
            p_block = NULL;
        }
    }

    return i_copied;
}

int ChunksSourceStream::Seek(uint64_t)
{
    return VLC_EGENERIC;
}

ssize_t ChunksSourceStream::read_Callback(stream_t *s, void *buf, size_t size)
{
    ChunksSourceStream *me = reinterpret_cast<ChunksSourceStream *>(s->p_sys);
    return me->Read(reinterpret_cast<uint8_t *>(buf), size);
}

int ChunksSourceStream::seek_Callback(stream_t *s, uint64_t i_pos)
{
    ChunksSourceStream *me = reinterpret_cast<ChunksSourceStream *>(s->p_sys);
    return me->Seek(i_pos);
}

int ChunksSourceStream::control_Callback(stream_t *s, int i_query, va_list args)
{
    ChunksSourceStream *me = reinterpret_cast<ChunksSourceStream *>(s->p_sys);
    switch( i_query )
    {
        case STREAM_GET_SIZE:
            *(va_arg( args, uint64_t * )) = 0;
            return VLC_SUCCESS;

        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg( args, bool * ) = false;
            return VLC_SUCCESS;

        case STREAM_GET_CONTENT_TYPE:
        {
            std::string type = me->getContentType();
            if(!type.empty())
            {
                *va_arg( args, char ** ) = strdup(type.c_str());
                return VLC_SUCCESS;
            }
        }
        break;

        case STREAM_GET_PTS_DELAY:
            *(va_arg( args, uint64_t * )) = DEFAULT_PTS_DELAY;
            return VLC_SUCCESS;

        default:
            break;
    }
    return VLC_EGENERIC;
}

void ChunksSourceStream::delete_Callback(stream_t *)
{
}

BufferedChunksSourceStream::BufferedChunksSourceStream(vlc_object_t *p_obj_, ChunksSource *source_)
    : ChunksSourceStream( p_obj_, source_ )
{
    i_global_offset = 0;
    i_bytestream_offset = 0;
    block_BytestreamInit( &bs );
}

BufferedChunksSourceStream::~BufferedChunksSourceStream()
{
    block_BytestreamEmpty( &bs );
}

void BufferedChunksSourceStream::Reset()
{
    block_BytestreamEmpty( &bs );
    i_bytestream_offset = 0;
    i_global_offset = 0;
    b_eof = false;
}

ssize_t BufferedChunksSourceStream::Read(uint8_t *buf, size_t size)
{
    size_t i_copied = 0;
    size_t i_toread = size;

    while(i_toread && !b_eof)
    {
        size_t i_remain = block_BytestreamRemaining(&bs) - i_bytestream_offset;

        if(i_remain < i_toread)
        {
            block_t *p_add = source->readNextBlock();
            if(!p_add)
            {
                b_eof = true;
                break;
            }
            i_remain += p_add->i_buffer;
            block_BytestreamPush(&bs, p_add);
        }

        size_t i_read;
        if(i_remain >= i_toread)
            i_read = i_toread;
        else
            i_read = i_remain;

        if(buf)
            block_PeekOffsetBytes(&bs, i_bytestream_offset, &buf[i_copied], i_read);
        i_bytestream_offset += i_read;
        i_copied += i_read;
        i_toread -= i_read;
    }

    if(i_bytestream_offset > MAX_BACKEND)
    {
        const size_t i_drop = i_bytestream_offset - MAX_BACKEND;
        if(i_drop >= MIN_BACKEND_CLEANUP) /* Dont flush for few bytes */
        {
            block_GetBytes(&bs, NULL, i_drop);
            block_BytestreamFlush(&bs);
            i_bytestream_offset -= i_drop;
            i_global_offset += i_drop;
        }
    }

    return i_copied;
}

int BufferedChunksSourceStream::Seek(uint64_t i_seek)
{
    if(i_seek < i_global_offset ||
       i_seek - i_global_offset > block_BytestreamRemaining(&bs))
        return VLC_EGENERIC;
    i_bytestream_offset = i_seek - i_global_offset;
    return VLC_SUCCESS;
}
