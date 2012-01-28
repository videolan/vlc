/*****************************************************************************
 * SegmentInfoCommon.cpp: Implement the common part for both SegmentInfoDefault
 *                        and SegmentInfo
 *****************************************************************************
 * Copyright (C) 1998-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Hugo Beauzée-Luyssen <beauze.h@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#include "SegmentInfoCommon.h"

#include "Segment.h"
#include "SegmentTimeline.h"

using namespace dash::mpd;

SegmentInfoCommon::SegmentInfoCommon() :
    duration( -1 ),
    initialisationSegment( NULL ),
    segmentTimeline( NULL )
{
}

SegmentInfoCommon::~SegmentInfoCommon()
{
    delete this->segmentTimeline;
    delete this->initialisationSegment;
}

time_t      SegmentInfoCommon::getDuration() const
{
    return this->duration;
}

void        SegmentInfoCommon::setDuration( time_t duration )
{
    if ( duration >= 0 )
        this->duration = duration;
}

int         SegmentInfoCommon::getStartIndex() const
{
    return this->startIndex;
}

void        SegmentInfoCommon::setStartIndex(int startIndex)
{
    if ( startIndex >= 0 )
        this->startIndex = startIndex;
}

Segment*  SegmentInfoCommon::getInitialisationSegment() const
{
    return this->initialisationSegment;
}

void SegmentInfoCommon::setInitialisationSegment(Segment *seg)
{
    if ( seg != NULL )
        this->initialisationSegment = seg;
}

const std::list<std::string>&   SegmentInfoCommon::getBaseURL() const
{
    return this->baseURLs;
}

void SegmentInfoCommon::appendBaseURL(const std::string &url)
{
    this->baseURLs.push_back( url );
}

const SegmentTimeline *SegmentInfoCommon::getSegmentTimeline() const
{
    return this->segmentTimeline;
}

void SegmentInfoCommon::setSegmentTimeline( const SegmentTimeline *segTl )
{
    if ( segTl != NULL )
        this->segmentTimeline = segTl;
}
