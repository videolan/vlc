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
#include "BaseRepresentation.h"
#include "SegmentChunk.hpp"
#include <cassert>

using namespace adaptative::http;
using namespace adaptative::playlist;

ISegment::ISegment(const ICanonicalUrl *parent):
    ICanonicalUrl( parent ),
    startByte  (0),
    endByte    (0)
{
    debugName = "Segment";
    classId = CLASSID_ISEGMENT;
    startTime.Set(0);
    duration.Set(0);
    chunksuse.Set(0);
}

ISegment::~ISegment()
{
    assert(chunksuse.Get() == 0);
}

SegmentChunk * ISegment::getChunk(const std::string &url)
{
    return new (std::nothrow) SegmentChunk(this, url);
}

void ISegment::onChunkDownload(block_t **, SegmentChunk *, BaseRepresentation *)
{

}

SegmentChunk* ISegment::toChunk(size_t index, BaseRepresentation *ctxrep)
{
    SegmentChunk *chunk;
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

    chunk->setRepresentation(ctxrep);

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

void ISegment::debug(vlc_object_t *obj, int indent) const
{
    std::stringstream ss;
    ss << std::string(indent, ' ') << debugName << " url=" << getUrlSegment().toString();
    if(startByte!=endByte)
        ss << " @" << startByte << ".." << endByte;
    msg_Dbg(obj, "%s", ss.str().c_str());
}

bool ISegment::contains(size_t byte) const
{
    if (startByte == endByte)
        return false;
    return (byte >= startByte &&
            (!endByte || byte <= endByte) );
}

int ISegment::compare(ISegment *other) const
{
    if(duration.Get())
    {
        int64_t diff = startTime.Get() - other->startTime.Get();
        if(diff)
            return diff / diff;
    }

    size_t diff = startByte - other->startByte;
    if(diff)
        return diff / diff;

    diff = endByte - other->endByte;
    if(diff)
        return diff / diff;

    return 0;
}

int ISegment::getClassId() const
{
    return classId;
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
    for(it=subsegments.begin();it!=subsegments.end();++it)
        delete *it;
}

void                    Segment::setSourceUrl   ( const std::string &url )
{
    if ( url.empty() == false )
        this->sourceUrl = Url(url);
}

void Segment::debug(vlc_object_t *obj, int indent) const
{
    if (subsegments.empty())
    {
        ISegment::debug(obj, indent);
    }
    else
    {
        std::string text(indent, ' ');
        text.append("Segment");
        msg_Dbg(obj, "%s", text.c_str());
        std::vector<SubSegment *>::const_iterator l;
        for(l = subsegments.begin(); l != subsegments.end(); ++l)
            (*l)->debug(obj, indent + 1);
    }
}

Url Segment::getUrlSegment() const
{
    if(sourceUrl.hasScheme())
    {
        return sourceUrl;
    }
    else
    {
        Url ret = getParentUrlSegment();
        if (!sourceUrl.empty())
            ret.append(sourceUrl);
        return ret;
    }
}

SegmentChunk* Segment::toChunk(size_t index, BaseRepresentation *ctxrep)
{
    SegmentChunk *chunk = ISegment::toChunk(index, ctxrep);
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
        for(it=subsegments.begin();it!=subsegments.end();++it)
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

SubSegment::SubSegment(ISegment *main, size_t start, size_t end) :
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

void SubSegment::addSubSegment(SubSegment *)
{

}
