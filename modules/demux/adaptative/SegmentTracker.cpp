/*
 * SegmentTracker.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC authors
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
#include "SegmentTracker.hpp"
#include "playlist/AbstractPlaylist.hpp"
#include "playlist/BaseRepresentation.h"
#include "playlist/BaseAdaptationSet.h"
#include "playlist/Segment.h"
#include "logic/AbstractAdaptationLogic.h"

using namespace adaptative;
using namespace adaptative::logic;
using namespace adaptative::playlist;

SegmentTracker::SegmentTracker(AbstractAdaptationLogic *logic_, BaseAdaptationSet *adaptSet)
{
    count = 0;
    initializing = true;
    indexed = false;
    prevRepresentation = NULL;
    setAdaptationLogic(logic_);
    adaptationSet = adaptSet;
}

SegmentTracker::~SegmentTracker()
{

}

void SegmentTracker::setAdaptationLogic(AbstractAdaptationLogic *logic_)
{
    logic = logic_;
}

void SegmentTracker::resetCounter()
{
    count = 0;
    prevRepresentation = NULL;
}

SegmentChunk * SegmentTracker::getNextChunk(bool switch_allowed)
{
    BaseRepresentation *rep;
    ISegment *segment;

    if(!adaptationSet)
        return NULL;

    if( !switch_allowed ||
       (prevRepresentation && prevRepresentation->getSwitchPolicy() == SegmentInformation::SWITCH_UNAVAILABLE) )
        rep = prevRepresentation;
    else
        rep = logic->getCurrentRepresentation(adaptationSet);

    if ( rep == NULL )
            return NULL;

    if(rep != prevRepresentation)
    {
        prevRepresentation = rep;
        initializing = true;
    }

    if(initializing)
    {
        initializing = false;
        segment = rep->getSegment(BaseRepresentation::INFOTYPE_INIT);
        if(segment)
            return segment->toChunk(count, rep);
    }

    if(!indexed)
    {
        indexed = true;
        segment = rep->getSegment(BaseRepresentation::INFOTYPE_INDEX);
        if(segment)
            return segment->toChunk(count, rep);
    }

    segment = rep->getSegment(BaseRepresentation::INFOTYPE_MEDIA, count);
    if(!segment)
    {
        resetCounter();
        return NULL;
    }

    SegmentChunk *chunk = segment->toChunk(count, rep);
    if(chunk)
        count++;

    return chunk;
}

bool SegmentTracker::setPosition(mtime_t time, bool restarted, bool tryonly)
{
    uint64_t segcount;
    if(prevRepresentation &&
       prevRepresentation->getSegmentNumberByTime(time, &segcount))
    {
        if(!tryonly)
        {
            if(restarted)
                initializing = true;
            count = segcount;
        }
        return true;
    }
    return false;
}

mtime_t SegmentTracker::getSegmentStart() const
{
    if(prevRepresentation)
        return prevRepresentation->getPlaybackTimeBySegmentNumber(count);
    else
        return 0;
}

void SegmentTracker::pruneFromCurrent()
{
    AbstractPlaylist *playlist = adaptationSet->getPlaylist();
    if(playlist->isLive())
        playlist->pruneBySegmentNumber(count);
}
