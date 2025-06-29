/*
 * SegmentList.cpp
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

#include "SegmentList.h"
#include "Segment.h"
#include "SegmentInformation.hpp"
#include "SegmentTimeline.h"

#include <limits>
#include <cassert>

using namespace adaptive;
using namespace adaptive::playlist;

SegmentList::SegmentList( SegmentInformation *parent_, bool b_relative ):
    AbstractMultipleSegmentBaseType( parent_, AttrsNode::Type::SegmentList )
{
    totalLength = 0;
    b_relative_mediatimes = b_relative;
}
SegmentList::~SegmentList()
{
    std::vector<Segment *>::iterator it;
    for(it = segments.begin(); it != segments.end(); ++it)
        delete(*it);
}

const std::vector<Segment*>& SegmentList::getSegments() const
{
    return segments;
}

Segment * SegmentList::getMediaSegment(uint64_t number) const
{
    const SegmentTimeline *timeline = inheritSegmentTimeline();
    if(timeline)
    {
        uint64_t listindex = timeline->getElementIndexBySequence(number);
        if(listindex >= segments.size())
            return nullptr;
        return segments.at(listindex);
    }

    std::vector<Segment *>::const_iterator it = segments.begin();
    for(it = segments.begin(); it != segments.end(); ++it)
    {
        Segment *seg = *it;
        if(seg->getSequenceNumber() == number)
        {
            return seg;
        }
        else if (seg->getSequenceNumber() > number)
        {
            break;
        }
    }
    return nullptr;
}

void SegmentList::addSegment(Segment *seg)
{
    seg->setParent(AbstractSegmentBaseType::parent);
    segments.push_back(seg);
    totalLength += seg->duration;
}

void SegmentList::updateWith(AbstractMultipleSegmentBaseType *updated_,
                             bool b_restamp)
{
    stime_t duration = inheritDuration();

    AbstractMultipleSegmentBaseType::updateWith(updated_);

    SegmentList *updated = dynamic_cast<SegmentList *>(updated_);
    if(!updated || updated->segments.empty())
        return;

    b_restamp = b_relative_mediatimes;

    if(!b_restamp || segments.empty())
    {
        if(!segments.empty())
            pruneBySegmentNumber(std::numeric_limits<uint64_t>::max());
        assert(segments.empty());
        for(auto seg : updated->segments)
            addSegment(seg);
        updated->segments.clear();
    }
    else
    {
        const Segment * prevSegment = segments.back();
        const uint64_t oldest = updated->segments.front()->getSequenceNumber();

        /* filter out known segments from the update */
        updated->pruneBySegmentNumber(prevSegment->getSequenceNumber() + 1);

        if(updated->segments.empty())
            return;

        /* merge update with current list */
        for(auto it = updated->segments.begin(); it != updated->segments.end(); ++it)
        {
            Segment *cur = *it;
            cur->startTime = prevSegment->startTime + prevSegment->duration;
            /* not continuous */
            if(cur->getSequenceNumber() != prevSegment->getSequenceNumber() + 1)
            {
                assert(prevSegment->getSequenceNumber() < cur->getSequenceNumber());
                assert(duration);
                uint64_t gap = cur->getSequenceNumber() - prevSegment->getSequenceNumber() - 1;
                cur->startTime = cur->startTime + duration * gap;
            }
            prevSegment = cur;
            addSegment(cur);
        }
        updated->segments.clear();

        /* prune previous list using update window start */
        pruneBySegmentNumber(oldest);
    }
}

void SegmentList::pruneByPlaybackTime(vlc_tick_t time)
{
    const Timescale timescale = inheritTimescale();
    uint64_t num = findSegmentNumberByScaledTime(segments, timescale.ToScaled(time));
    if(num != std::numeric_limits<uint64_t>::max())
        pruneBySegmentNumber(num);
}

void SegmentList::pruneBySegmentNumber(uint64_t tobelownum)
{
    std::vector<Segment *>::iterator it = segments.begin();
    while(it != segments.end())
    {
        Segment *seg = *it;

        if(seg->getSequenceNumber() >= tobelownum)
            break;

        totalLength -= (*it)->duration;
        delete *it;
        it = segments.erase(it);
    }
}

bool SegmentList::getPlaybackTimeDurationBySegmentNumber(uint64_t number,
                                                         vlc_tick_t *time, vlc_tick_t *dur) const
{
    if(number == std::numeric_limits<uint64_t>::max())
        return false;

    Timescale timescale;
    stime_t stime, sduration;

    const SegmentTimeline * timeline = inheritSegmentTimeline();
    if(timeline)
    {
        timescale = timeline->inheritTimescale();
        if(!timeline->getScaledPlaybackTimeDurationBySegmentNumber(number, &stime, &sduration))
            return false;
    }
    else
    {
        *time = 0;
        *dur = 0;
        timescale = inheritTimescale();

        if(segments.empty())
            return false;

        const ISegment *first = segments.front();
        if(first->getSequenceNumber() > number)
            return false;

        bool found = false;
        stime = first->startTime;
        sduration = 0;
        std::vector<Segment *>::const_iterator it = segments.begin();
        for(it = segments.begin(); it != segments.end(); ++it)
        {
            const Segment *seg = *it;

            if(seg->duration)
                sduration = seg->duration;
            else
                sduration = inheritDuration();

            /* Assuming there won't be any discontinuity in sequence */
            if(seg->getSequenceNumber() == number)
            {
                found = true;
                break;
            }

            stime += sduration;
        }

        if(!found)
            return false;
    }

    *time = timescale.ToTime(stime);
    *dur = timescale.ToTime(sduration);
    return true;
}

stime_t SegmentList::getTotalLength() const
{
    const SegmentTimeline *timeline = inheritSegmentTimeline();
    if(timeline)
        return timeline->getTotalLength();
    return totalLength;
}

bool SegmentList::hasRelativeMediaTimes() const
{
    return b_relative_mediatimes;
}

vlc_tick_t SegmentList::getMinAheadTime(uint64_t curnum) const
{
    const SegmentTimeline *timeline = inheritSegmentTimeline();
    if( timeline )
    {
        const Timescale timescale = timeline->inheritTimescale();
        return timescale.ToTime(timeline->getMinAheadScaledTime(curnum));
    }

    vlc_tick_t minTime = 0;
    const Timescale timescale = inheritTimescale();
    std::vector<Segment *>::const_iterator it;
    for(it = segments.begin(); it != segments.end(); ++it)
    {
        const Segment *seg = *it;
        if(seg->getSequenceNumber() > curnum)
            minTime += timescale.ToTime(seg->duration);
    }
    return minTime;
}

Segment *  SegmentList::getNextMediaSegment(uint64_t i_pos,uint64_t *pi_newpos,
                                            bool *pb_gap) const
{
    *pb_gap = false;
    *pi_newpos = i_pos;

    const SegmentTimeline *timeline = inheritSegmentTimeline();
    if(timeline)
    {
        uint64_t listindex = timeline->getElementIndexBySequence(i_pos);
        if(listindex >= segments.size())
            return nullptr;
        return segments.at(listindex);
    }

    std::vector<Segment *>::const_iterator it;
    for(it = segments.begin(); it != segments.end(); ++it)
    {
        Segment *seg = *it;
        if(seg->getSequenceNumber() >= i_pos)
        {
            *pi_newpos = seg->getSequenceNumber();
            *pb_gap = (*pi_newpos != i_pos);
            return seg;
        }
    }
    return nullptr;
}

uint64_t SegmentList::getStartSegmentNumber() const
{
    const SegmentTimeline *timeline = inheritSegmentTimeline();
    if( timeline )
        return timeline->minElementNumber();
    return !segments.empty() ? segments.front()->getSequenceNumber() : inheritStartNumber();
}

bool SegmentList::getSegmentNumberByTime(vlc_tick_t time, uint64_t *ret) const
{
    const SegmentTimeline *timeline = inheritSegmentTimeline();
    if(timeline)
    {
        const Timescale timescale = timeline->inheritTimescale();
        stime_t st = timescale.ToScaled(time);
        *ret = timeline->getElementNumberByScaledPlaybackTime(st);
        return true;
    }

    const Timescale timescale = inheritTimescale();
    if(!timescale.isValid())
        return false;
    stime_t st = timescale.ToScaled(time);
    *ret = AbstractSegmentBaseType::findSegmentNumberByScaledTime(segments, st);
    return *ret != std::numeric_limits<uint64_t>::max();
}

void SegmentList::debug(vlc_object_t *obj, int indent) const
{
    AbstractSegmentBaseType::debug(obj, indent);
    std::vector<Segment *>::const_iterator it;
    for(it = segments.begin(); it != segments.end(); ++it)
        (*it)->debug(obj, indent);
    const AbstractAttr *p = getAttribute(Type::Timeline);
    if(p)
        static_cast<const SegmentTimeline *> (p)->debug(obj, indent + 1);
}
