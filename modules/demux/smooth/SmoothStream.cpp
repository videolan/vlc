/*
 * SmoothStream.cpp
 *****************************************************************************
 * Copyright (C) 2015 - VideoLAN and VLC authors
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

#include "SmoothStream.hpp"
#include <vlc_demux.h>

using namespace smooth;

SmoothStream::SmoothStream(demux_t *demux)
    :AbstractStream(demux)
{
}

AbstractDemuxer *SmoothStream::newDemux(demux_t *p_realdemux, const StreamFormat &format,
                                        es_out_t *out, AbstractSourceStream *source) const
{
    if((unsigned)format != StreamFormat::MP4)
        return NULL;
    return AbstractStream::newDemux(p_realdemux, format, out, source);
}

block_t * SmoothStream::checkBlock(block_t *p_block, bool)
{
    return p_block;
}

AbstractStream * SmoothStreamFactory::create(demux_t *realdemux, const StreamFormat &format,
                                             SegmentTracker *tracker, AbstractConnectionManager *manager) const
{
    SmoothStream *stream = new (std::nothrow) SmoothStream(realdemux);
    if(stream && !stream->init(format,tracker, manager))
    {
        delete stream;
        return NULL;
    }
    return stream;
}
