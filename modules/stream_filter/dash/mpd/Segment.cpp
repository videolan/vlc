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

Segment::Segment(const Representation *parent) :
        startByte  (-1),
        endByte    (-1),
        parentRepresentation( parent )
{
    assert( parent != NULL );
    if ( parent->getSegmentInfo() != NULL && parent->getSegmentInfo()->getDuration() >= 0 )
        this->size = parent->getBandwidth() * parent->getSegmentInfo()->getDuration();
    else
        this->size = -1;
}

std::string             Segment::getSourceUrl   () const
{
    return this->sourceUrl;
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
void                    Segment::addBaseUrl     (BaseUrl *url)
{
    this->baseUrls.push_back(url);
}
const std::vector<BaseUrl *>&  Segment::getBaseUrls    () const
{
    return this->baseUrls;
}
void                    Segment::setByteRange   (int start, int end)
{
    this->startByte = start;
    this->endByte   = end;
}
int                     Segment::getStartByte   () const
{
    return this->startByte;
}
int                     Segment::getEndByte     () const
{
    return this->endByte;
}
dash::http::Chunk*      Segment::toChunk        ()
{
    Chunk *chunk = new Chunk();

    if(this->startByte != -1 && this->endByte != -1)
    {
        chunk->setUseByteRange(true);
        chunk->setStartByte(this->startByte);
        chunk->setEndByte(this->endByte);
    }

    if(this->baseUrls.size() > 0)
    {
        std::stringstream ss;
        ss << this->baseUrls.at(0)->getUrl() << this->sourceUrl;
        chunk->setUrl(ss.str());
        ss.clear();

        for(size_t i = 1; i < this->baseUrls.size(); i++)
        {
            ss << this->baseUrls.at(i)->getUrl() << this->sourceUrl;
            chunk->addOptionalUrl(ss.str());
            ss.clear();
        }
    }
    else
    {
        chunk->setUrl(this->sourceUrl);
    }

    chunk->setBitrate(this->parentRepresentation->getBandwidth());

    return chunk;
}

const Representation *Segment::getParentRepresentation() const
{
    return this->parentRepresentation;
}

int Segment::getSize() const
{
    return this->size;
}
