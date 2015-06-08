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

#include "SegmentList.h"
#include "Segment.h"
#include "SegmentInformation.hpp"

using namespace adaptative::playlist;

SegmentList::SegmentList( SegmentInformation *parent ):
    SegmentInfoCommon( parent ), TimescaleAble( parent )
{
    pruned = 0;
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

void SegmentList::addSegment(ISegment *seg)
{
    seg->setParent(this);
    segments.push_back(seg);
}

void SegmentList::mergeWith(SegmentList *updated)
{
    const ISegment * lastSegment = (segments.empty()) ? NULL : segments.back();

    std::vector<ISegment *>::iterator it;
    for(it = updated->segments.begin(); it != updated->segments.end(); ++it)
    {
        if( !lastSegment || lastSegment->compare( *it ) < 0 )
            addSegment(*it);
        else
            delete *it;
    }
    updated->segments.clear();
}

void SegmentList::pruneBySegmentNumber(uint64_t tobelownum)
{
    if(tobelownum < pruned)
        return;

    uint64_t current = pruned;
    std::vector<ISegment *>::iterator it = segments.begin();
    while(it != segments.end() && current < tobelownum)
    {
        ISegment *seg = *it;
        if(seg->chunksuse.Get()) /* can't prune from here, still in use */
            break;
        delete *it;
        it = segments.erase(it);

        current++;
        pruned++;
    }
}

bool SegmentList::getSegmentNumberByTime(mtime_t time, uint64_t *ret) const
{
    const uint64_t timescale = inheritTimescale();
    time = time * timescale / CLOCK_FREQ;
    *ret = pruned;
    return SegmentInfoCommon::getSegmentNumberByTime(segments, time, ret);
}

mtime_t SegmentList::getPlaybackTimeBySegmentNumber(uint64_t number)
{
    if(number < pruned || segments.empty())
        return VLC_TS_INVALID;

    uint64_t timescale = inheritTimescale();
    mtime_t time = segments.at(0)->startTime.Get();

    if(segments.at(0)->duration.Get())
    {
        number -= pruned;

        for(size_t i=0; i<number && i<segments.size(); i++)
            time += segments.at(i)->duration.Get();
    }
    else
    {
        time = number * duration.Get();
    }

    return VLC_TS_0 + CLOCK_FREQ * time / timescale;
}

std::size_t SegmentList::getOffset() const
{
    return pruned;
}
