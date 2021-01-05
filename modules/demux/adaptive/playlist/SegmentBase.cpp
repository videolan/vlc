/*
 * SegmentBase.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2012 Klagenfurt University
 *
 * Created on: Jan 27, 2012
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

#include "SegmentBase.h"
#include "SegmentInformation.hpp"

#include <limits>
#include <algorithm>

using namespace adaptive::playlist;

SegmentBase::SegmentBase(SegmentInformation *parent) :
             Segment(parent),
             AbstractSegmentBaseType(parent, AttrsNode::Type::SegmentBase)
{
    this->parent = parent;
}
SegmentBase::~SegmentBase   ()
{
}

vlc_tick_t SegmentBase::getMinAheadTime(uint64_t curnum) const
{
    if(subsegments.size() == 0 || curnum >= subsegments.size() - 1)
        return 0;

    const Timescale timescale = inheritTimescale();
    if(!timescale.isValid())
        return 0;

    stime_t minTime = 0;
    std::for_each(subsegments.cbegin() + curnum + 1, subsegments.cend(),
        [&minTime,timescale](const Segment * seg){
            minTime += seg->duration.Get();
        });

    return timescale.ToTime(minTime);
}

Segment * SegmentBase::getMediaSegment(uint64_t pos) const
{
    return (pos < subsegments.size()) ? subsegments.at(pos) : nullptr;
}

Segment *  SegmentBase::getNextMediaSegment(uint64_t i_pos,uint64_t *pi_newpos,
                                            bool *pb_gap) const
{
    *pb_gap = false;
    *pi_newpos = i_pos;
    return getMediaSegment(i_pos);
}

uint64_t SegmentBase::getStartSegmentNumber() const
{
    return 0;
}

bool SegmentBase::getSegmentNumberByTime(vlc_tick_t time, uint64_t *ret) const
{
    const Timescale timescale = inheritTimescale();
    if(!timescale.isValid())
        return false;
    stime_t st = timescale.ToScaled(time);
    *ret = AbstractSegmentBaseType::findSegmentNumberByScaledTime(subsegments, st);
    return *ret != std::numeric_limits<uint64_t>::max();
}

bool SegmentBase::getPlaybackTimeDurationBySegmentNumber(uint64_t number,
                                                         vlc_tick_t *time,
                                                         vlc_tick_t *dur) const
{
    const Timescale timescale = inheritTimescale();
    const ISegment *segment = getMediaSegment(number);
    if( segment )
    {
        *time = timescale.ToTime(segment->startTime.Get());
        *dur = timescale.ToTime(segment->duration.Get());
        return true;
    }
    return false;
}

void SegmentBase::debug(vlc_object_t *obj, int indent) const
{
    AbstractSegmentBaseType::debug(obj, indent);
    std::for_each(subsegments.cbegin(), subsegments.cend(),
                  [&](const Segment *seg){seg->debug(obj, indent);});
}
