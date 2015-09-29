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
#include "SourceStream.hpp"

#include "../ChunksSource.hpp"
#include <vlc_stream.h>
#include <vlc_demux.h>

using namespace adaptative;

ChunksSourceStream::ChunksSourceStream(vlc_object_t *p_obj, ChunksSource *source_)
{
    p_block = NULL;
    b_eof = false;

    custom_stream = stream_CustomNew( p_obj, delete_Callback );
    if(!custom_stream)
        throw VLC_EGENERIC;

    custom_stream->pf_control = control_Callback;
    custom_stream->pf_read = read_Callback;
    custom_stream->pf_readdir = NULL;
    custom_stream->pf_seek = seek_Callback;
    custom_stream->p_sys = reinterpret_cast<stream_sys_t*>(this);

    source = source_;
}

ChunksSourceStream::~ChunksSourceStream()
{
    if (custom_stream)
        stream_Delete(custom_stream);
    Reset();
}

void ChunksSourceStream::Reset()
{
    if(p_block)
        block_Release(p_block);
    p_block = NULL;
    b_eof = false;
}

stream_t * ChunksSourceStream::getStream()
{
    return custom_stream;
}

ssize_t ChunksSourceStream::Read(uint8_t *buf, size_t size)
{
    size_t i_copied = 0;
    size_t i_toread = size;

    while(i_toread && !b_eof)
    {
        if(!p_block && !(p_block = source->readNextBlock(i_toread)))
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

ssize_t ChunksSourceStream::read_Callback(stream_t *s, void *buf, size_t size)
{
    ChunksSourceStream *me = reinterpret_cast<ChunksSourceStream *>(s->p_sys);
    return me->Read(reinterpret_cast<uint8_t *>(buf), size);
}

int ChunksSourceStream::seek_Callback(stream_t *, uint64_t)
{
    return VLC_EGENERIC;
}

int ChunksSourceStream::control_Callback(stream_t *, int i_query, va_list args)
{
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

        case STREAM_GET_PTS_DELAY:
            *(va_arg( args, uint64_t * )) = DEFAULT_PTS_DELAY;
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}

void ChunksSourceStream::delete_Callback(stream_t *)
{
}
