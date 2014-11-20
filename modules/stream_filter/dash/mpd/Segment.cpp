/*
 * Segment.cpp
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

#include "Segment.h"
#include "Representation.h"

#include <cassert>

using namespace dash::mpd;
using namespace dash::http;

Segment::Segment(const Representation *parent, bool isinit) :
        ICanonicalUrl( parent ),
        startByte  (0),
        endByte    (0),
        parentRepresentation( parent ),
        init( isinit )
{
    assert( parent != NULL );
    if ( parent->getSegmentInfo() != NULL && parent->getSegmentInfo()->getDuration() >= 0 )
        this->size = parent->getBandwidth() * parent->getSegmentInfo()->getDuration();
    else
        this->size = -1;
}

void                    Segment::setSourceUrl   ( const std::string &url )
{
    if ( url.empty() == false )
        this->sourceUrl = url;
}
bool                    Segment::isSingleShot   () const
{
    return true;
}
void                    Segment::done           ()
{
    //Only used for a SegmentTemplate.
}

void                    Segment::setByteRange   (int start, int end)
{
    this->startByte = start;
    this->endByte   = end;
}

dash::http::Chunk*      Segment::toChunk        ()
{
    Chunk *chunk = new Chunk();

    if(startByte != endByte)
    {
        chunk->setStartByte(startByte);
        chunk->setEndByte(endByte);
    }

    chunk->setUrl( getUrlSegment() );

    chunk->setBitrate(this->parentRepresentation->getBandwidth());

    return chunk;
}

const Representation *Segment::getParentRepresentation() const
{
    return this->parentRepresentation;
}

std::string Segment::getUrlSegment() const
{
    std::string ret = getParentUrlSegment();
    if (!sourceUrl.empty())
        ret.append(sourceUrl);
    return ret;
}

bool Segment::isInit() const
{
    return init;
}
