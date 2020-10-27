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

AbstractSegmentBaseType::AbstractSegmentBaseType(SegmentInformation *parent, AttrsNode::Type t)
                : AttrsNode(t, parent)
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

void AbstractSegmentBaseType::debug(vlc_object_t *obj, int indent) const
{
    if(initialisationSegment.Get())
        initialisationSegment.Get()->debug(obj, indent);
    if(indexSegment.Get())
        indexSegment.Get()->debug(obj, indent);
}

AbstractMultipleSegmentBaseType::AbstractMultipleSegmentBaseType(SegmentInformation *parent,
                                                                 AttrsNode::Type type)
                : AbstractSegmentBaseType(parent, type)
{
}

AbstractMultipleSegmentBaseType::~AbstractMultipleSegmentBaseType()
{
}

void AbstractMultipleSegmentBaseType::updateWith(AbstractMultipleSegmentBaseType *updated,
                                                 bool)
{
    SegmentTimeline *local = static_cast<SegmentTimeline *>(getAttribute(Type::TIMELINE));
    SegmentTimeline *other = static_cast<SegmentTimeline *>(updated->getAttribute(Type::TIMELINE));
    if(local && other)
        local->updateWith(*other);
}
