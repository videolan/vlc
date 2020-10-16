/*
 * SegmentBaseType.cpp
 *****************************************************************************
 * Copyright (C) 2020 VideoLabs, VideoLAN and VLC Authors
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

#include "SegmentBaseType.hpp"
#include "SegmentInformation.hpp"
#include "SegmentTemplate.h"
#include "SegmentTimeline.h"

#include <limits>

using namespace adaptive::playlist;

Segment * AbstractSegmentBaseType::findSegmentByScaledTime(const std::vector<Segment *> &segments,
                                                                 stime_t time)
{
    if(segments.empty() || (segments.size() > 1 && segments[1]->startTime.Get() == 0) )
        return NULL;

    Segment *ret = NULL;
    std::vector<Segment *>::const_iterator it = segments.begin();
    while(it != segments.end())
    {
        Segment *seg = *it;
        if(seg->startTime.Get() > time)
        {
            if(it == segments.begin())
                return NULL;
            else
                break;
        }

        ret = seg;
        it++;
    }

    return ret;
}

uint64_t AbstractSegmentBaseType::findSegmentNumberByScaledTime(const std::vector<Segment *> &segments,
                                                               stime_t time)
{
    Segment *s = findSegmentByScaledTime(segments, time);
    if(!s)
        return std::numeric_limits<uint64_t>::max();
    return s->getSequenceNumber();
}

AbstractSegmentBaseType::AbstractSegmentBaseType(SegmentInformation *parent)
                : TimescaleAble(parent)
{
    this->parent = parent;
}

AbstractSegmentBaseType::~AbstractSegmentBaseType()
{
}

InitSegment *AbstractSegmentBaseType::getInitSegment() const
{
    return initialisationSegment.Get();
}

IndexSegment *AbstractSegmentBaseType::getIndexSegment() const
{
    return indexSegment.Get();
}

Timescale AbstractSegmentBaseType::inheritTimescale() const
{
    if(getTimescale().isValid())
        return getTimescale();
    if(parent)
    {
        if(parent->getTimescale().isValid())
            return parent->getTimescale();
        if(parent->getParent())
        {
            AbstractSegmentBaseType *bt =
                dynamic_cast<AbstractSegmentBaseType *>(parent->getParent()->getProfile());
            if(bt)
                return bt->inheritTimescale();
        }
    }
    return Timescale(1);
}

SegmentInformation *AbstractSegmentBaseType::getParent() const
{
    return parent;
}

void AbstractSegmentBaseType::debug(vlc_object_t *obj, int indent) const
{
    if(initialisationSegment.Get())
        initialisationSegment.Get()->debug(obj, indent);
    if(indexSegment.Get())
        indexSegment.Get()->debug(obj, indent);
}

AbstractMultipleSegmentBaseType::AbstractMultipleSegmentBaseType(SegmentInformation *parent)
                : AbstractSegmentBaseType(parent)
{
    startNumber = std::numeric_limits<uint64_t>::max();
    segmentTimeline = NULL;
    duration.Set(0);
}

AbstractMultipleSegmentBaseType::~AbstractMultipleSegmentBaseType()
{
    delete segmentTimeline;
}

void AbstractMultipleSegmentBaseType::setSegmentTimeline( SegmentTimeline *v )
{
    delete segmentTimeline;
    segmentTimeline = v;
}

SegmentTimeline * AbstractMultipleSegmentBaseType::inheritSegmentTimeline() const
{
    if( segmentTimeline )
        return segmentTimeline;
    const SegmentInformation *ulevel = parent ? parent->getParent() : NULL;
    for( ; ulevel ; ulevel = ulevel->getParent() )
    {
        AbstractMultipleSegmentBaseType *bt =
            dynamic_cast<AbstractMultipleSegmentBaseType *>(ulevel->getProfile());
        if( bt && bt->segmentTimeline )
            return bt->segmentTimeline;
    }
    return NULL;
}

SegmentTimeline * AbstractMultipleSegmentBaseType::getSegmentTimeline() const
{
    return segmentTimeline;
}

void AbstractMultipleSegmentBaseType::setStartNumber( uint64_t v )
{
    startNumber = v;
}

uint64_t AbstractMultipleSegmentBaseType::inheritStartNumber() const
{
    if( startNumber != std::numeric_limits<uint64_t>::max() )
        return startNumber;

    const SegmentInformation *ulevel = parent ? parent->getParent() : NULL;
    for( ; ulevel ; ulevel = ulevel->parent )
    {
        AbstractMultipleSegmentBaseType *bt =
            dynamic_cast<AbstractMultipleSegmentBaseType *>(ulevel->getProfile());
        if( bt && bt->startNumber != std::numeric_limits<uint64_t>::max() )
            return bt->startNumber;
    }
    return std::numeric_limits<uint64_t>::max();
}

stime_t AbstractMultipleSegmentBaseType::inheritDuration() const
{
    if(duration.Get() > 0)
        return duration.Get();
    const SegmentInformation *ulevel = parent ? parent->getParent() : NULL;
    for( ; ulevel ; ulevel = ulevel->parent )
    {
        AbstractMultipleSegmentBaseType *bt =
            dynamic_cast<AbstractMultipleSegmentBaseType *>(ulevel->getProfile());
        if( bt && bt->duration.Get() > 0 )
            return bt->duration.Get();
    }
    return 0;
}

void AbstractMultipleSegmentBaseType::updateWith(AbstractMultipleSegmentBaseType *updated,
                                                 bool)
{
    if(segmentTimeline && updated->segmentTimeline)
        segmentTimeline->updateWith(*updated->segmentTimeline);
}
