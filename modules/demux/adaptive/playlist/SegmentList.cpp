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

using namespace adaptive::playlist;

SegmentList::SegmentList( SegmentInformation *parent ):
    SegmentInfoCommon( parent ), TimescaleAble( parent )
{
    totalLength = 0;
}
SegmentList::~SegmentList()
{
    std::vector<ISegment *>::iterator it;
    for(it = segments.begin(); it != segments.end(); ++it)
        delete(*it);
}

const std::vector<ISegment*>& SegmentList::getSegments() const
{
    return segments;
}

ISegment * SegmentList::getSegmentByNumber(uint64_t number)
{
    std::vector<ISegment *>::const_iterator it = segments.begin();
    for(it = segments.begin(); it != segments.end(); ++it)
    {
        ISegment *seg = *it;
        if(seg->getSequenceNumber() == number)
        {
            return seg;
        }
        else if (seg->getSequenceNumber() > number)
        {
            break;
        }
    }
    return NULL;
}

void SegmentList::addSegment(ISegment *seg)
{
    seg->setParent(this);
    segments.push_back(seg);
    totalLength += seg->duration.Get();
}

void SegmentList::updateWith(SegmentList *updated, bool b_restamp)
{
    const ISegment * lastSegment = (segments.empty()) ? NULL : segments.back();
    const ISegment * prevSegment = lastSegment;

    if(updated->segments.empty())
        return;

    uint64_t firstnumber = updated->segments.front()->getSequenceNumber();

    std::vector<ISegment *>::iterator it;
    for(it = updated->segments.begin(); it != updated->segments.end(); ++it)
    {
        ISegment *cur = *it;
        if(!lastSegment || lastSegment->compare(cur) < 0)
        {
            if(b_restamp && prevSegment)
            {
                stime_t starttime = prevSegment->startTime.Get() + prevSegment->duration.Get();
                if(starttime != cur->startTime.Get() && !cur->discontinuity)
                {
                    cur->startTime.Set(starttime);
                }

                prevSegment = cur;
            }
            addSegment(cur);
        }
        else
            delete cur;
    }
    updated->segments.clear();

    pruneBySegmentNumber(firstnumber);
}

void SegmentList::pruneByPlaybackTime(vlc_tick_t time)
{
    uint64_t num;
    const Timescale timescale = inheritTimescale();
    if(getSegmentNumberByScaledTime(timescale.ToScaled(time), &num))
        pruneBySegmentNumber(num);
}

void SegmentList::pruneBySegmentNumber(uint64_t tobelownum)
{
    std::vector<ISegment *>::iterator it = segments.begin();
    while(it != segments.end())
    {
        ISegment *seg = *it;

        if(seg->getSequenceNumber() >= tobelownum)
            break;

        totalLength -= (*it)->duration.Get();
        delete *it;
        it = segments.erase(it);
    }
}

bool SegmentList::getSegmentNumberByScaledTime(stime_t time, uint64_t *ret) const
{
    std::vector<ISegment *> allsubsegments;
    std::vector<ISegment *>::const_iterator it;
    for(it=segments.begin(); it!=segments.end(); ++it)
    {
        std::vector<ISegment *> list = (*it)->subSegments();
        allsubsegments.insert( allsubsegments.end(), list.begin(), list.end() );
    }

    return SegmentInfoCommon::getSegmentNumberByScaledTime(allsubsegments, time, ret);
}

bool SegmentList::getPlaybackTimeDurationBySegmentNumber(uint64_t number,
                                                         vlc_tick_t *time, vlc_tick_t *dur) const
{
    *time = *dur = VLC_TICK_INVALID;

    if(segments.empty())
        return false;

    const Timescale timescale = inheritTimescale();
    const ISegment *first = segments.front();
    if(first->getSequenceNumber() > number)
        return false;

    bool found = false;
    stime_t seg_start = first->startTime.Get();
    stime_t seg_dura = 0;
    std::vector<ISegment *>::const_iterator it = segments.begin();
    for(it = segments.begin(); it != segments.end(); ++it)
    {
        const ISegment *seg = *it;

        if(seg->duration.Get())
            seg_dura = seg->duration.Get();
        else
            seg_dura = duration.Get();

        /* Assuming there won't be any discontinuity in sequence */
        if(seg->getSequenceNumber() == number)
        {
            found = true;
            break;
        }

        seg_start += seg_dura;
    }

    if(!found)
        return false;

    *time = VLC_TICK_0 + timescale.ToTime(seg_start);
    *dur = VLC_TICK_0 + timescale.ToTime(seg_dura);
    return true;
}

stime_t SegmentList::getTotalLength() const
{
    return totalLength;
}
