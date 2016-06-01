/*
 * SegmentChunk.cpp
 *****************************************************************************
 * Copyright (C) 2014 - 2015 VideoLAN Authors
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

#include "SegmentChunk.hpp"
#include "Segment.h"
#include "BaseRepresentation.h"
#include <cassert>

using namespace adaptive::playlist;
using namespace adaptive;

SegmentChunk::SegmentChunk(ISegment *segment_, AbstractChunkSource *source,
                           BaseRepresentation *rep_) :
    AbstractChunk(source)
{
    segment = segment_;
    segment->chunksuse.Set(segment->chunksuse.Get() + 1);
    rep = rep_;
    discontinuity = segment_->discontinuity;
}

SegmentChunk::~SegmentChunk()
{
    assert(segment->chunksuse.Get() > 0);
    segment->chunksuse.Set(segment->chunksuse.Get() - 1);
}

void SegmentChunk::onDownload(block_t **pp_block)
{
    segment->onChunkDownload(pp_block, this, rep);
}

StreamFormat SegmentChunk::getStreamFormat() const
{
    if(rep)
        return rep->getStreamFormat();
    else
        return StreamFormat();
}

