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
#define __STDC_CONSTANT_MACROS

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Segment.h"
#include "Representation.h"
#include "MPD.h"
#include "mp4/AtomsReader.hpp"

#include <cassert>

using namespace dash::mpd;
using namespace dash::http;

ISegment::ISegment(const ICanonicalUrl *parent):
    ICanonicalUrl( parent ),
    startByte  (0),
    endByte    (0)
{
    debugName = "Segment";
    classId = CLASSID_ISEGMENT;
    startTime.Set(VLC_TS_INVALID);
    duration.Set(0);
}

dash::http::Chunk * ISegment::getChunk(const std::string &url)
{
    return new (std::nothrow) SegmentChunk(this, url);
}

dash::http::Chunk* ISegment::toChunk(size_t index, Representation *ctxrep)
{
    Chunk *chunk;
    try
    {
        chunk = getChunk(getUrlSegment().toString(index, ctxrep));
        if (!chunk)
            return NULL;
    }
    catch (int)
    {
        return NULL;
    }

    if(startByte != endByte)
    {
        chunk->setStartByte(startByte);
        chunk->setEndByte(endByte);
    }

    return chunk;
}

void ISegment::setByteRange(size_t start, size_t end)
{
    startByte = start;
    endByte   = end;
}

size_t ISegment::getOffset() const
{
    return startByte;
}

std::string ISegment::toString(int indent) const
{
    std::stringstream ss;
    ss << std::string(indent, ' ') << debugName << " url=" << getUrlSegment().toString();
    if(startByte!=endByte)
        ss << " @" << startByte << ".." << endByte;
    return ss.str();
}

bool ISegment::contains(size_t byte) const
{
    if (startByte == endByte)
        return false;
    return (byte >= startByte &&
            (!endByte || byte <= endByte) );
}

int ISegment::getClassId() const
{
    return classId;
}

ISegment::SegmentChunk::SegmentChunk(ISegment *segment_, const std::string &url) :
    dash::http::Chunk(url)
{
    segment = segment_;
}

void ISegment::SegmentChunk::onDownload(void *, size_t)
{

}

Segment::Segment(ICanonicalUrl *parent) :
        ISegment(parent)
{
    size = -1;
    classId = CLASSID_SEGMENT;
}

void Segment::addSubSegment(SubSegment *subsegment)
{
    subsegments.push_back(subsegment);
}

Segment::~Segment()
{
    std::vector<SubSegment*>::iterator it;
    for(it=subsegments.begin();it!=subsegments.end();it++)
        delete *it;
}

void                    Segment::setSourceUrl   ( const std::string &url )
{
    if ( url.empty() == false )
        this->sourceUrl = url;
}

std::string Segment::toString(int indent) const
{
    if (subsegments.empty())
    {
        return ISegment::toString(indent);
    }
    else
    {
        std::string ret;
        std::vector<SubSegment *>::const_iterator l;
        for(l = subsegments.begin(); l != subsegments.end(); l++)
        {
            ret.append( (*l)->toString(indent + 1) );
        }
        return ret;
    }
}

Url Segment::getUrlSegment() const
{
    Url ret = getParentUrlSegment();
    if (!sourceUrl.empty())
        ret.append(sourceUrl);
    return ret;
}

dash::http::Chunk* Segment::toChunk(size_t index, Representation *ctxrep)
{
    Chunk *chunk = ISegment::toChunk(index, ctxrep);
    if (chunk && ctxrep)
        chunk->setBitrate(ctxrep->getBandwidth());
    return chunk;
}

std::vector<ISegment*> Segment::subSegments()
{
    std::vector<ISegment*> list;
    if(!subsegments.empty())
    {
        std::vector<SubSegment*>::iterator it;
        for(it=subsegments.begin();it!=subsegments.end();it++)
            list.push_back(*it);
    }
    else
    {
        list.push_back(this);
    }
    return list;
}

InitSegment::InitSegment(ICanonicalUrl *parent) :
    Segment(parent)
{
    debugName = "InitSegment";
    classId = CLASSID_INITSEGMENT;
}

IndexSegment::IndexSegment(ICanonicalUrl *parent) :
    Segment(parent)
{
    debugName = "IndexSegment";
    classId = CLASSID_INDEXSEGMENT;
}

dash::http::Chunk* IndexSegment::toChunk(size_t index, Representation *ctxrep)
{
    IndexSegmentChunk *chunk = dynamic_cast<IndexSegmentChunk *>(Segment::toChunk(index, ctxrep));
    chunk->setIndexRepresentation(ctxrep);
    return chunk;
}

dash::http::Chunk * IndexSegment::getChunk(const std::string &url)
{
    return new IndexSegmentChunk(this, url);
}

IndexSegment::IndexSegmentChunk::IndexSegmentChunk(ISegment *segment, const std::string &url)
    : SegmentChunk(segment, url)
{
    rep = NULL;
}

void IndexSegment::IndexSegmentChunk::setIndexRepresentation(Representation *rep_)
{
    rep = rep_;
}

void IndexSegment::IndexSegmentChunk::onDownload(void *buffer, size_t size)
{
    if(!rep)
        return;

    dash::mp4::AtomsReader br(rep->getMPD()->getVLCObject());
    br.parseBlock(buffer, size, rep);
}

SubSegment::SubSegment(Segment *main, size_t start, size_t end) :
    ISegment(main), parent(main)
{
    setByteRange(start, end);
    debugName = "SubSegment";
    classId = CLASSID_SUBSEGMENT;
}

Url SubSegment::getUrlSegment() const
{
    return getParentUrlSegment();
}

std::vector<ISegment*> SubSegment::subSegments()
{
    std::vector<ISegment*> list;
    list.push_back(this);
    return list;
}
