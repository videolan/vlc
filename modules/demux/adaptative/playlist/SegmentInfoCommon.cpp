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

using namespace adaptative::playlist;

Timelineable::Timelineable()
{
    segmentTimeline.Set(NULL);
}

Timelineable::~Timelineable()
{
    delete segmentTimeline.Get();
}

TimescaleAble::TimescaleAble(TimescaleAble *parent)
{
    timescale.Set(0);
    parentTimescale = parent;
}

TimescaleAble::~TimescaleAble()
{
}

uint64_t TimescaleAble::inheritTimescale() const
{
    if(timescale.Get())
        return timescale.Get();
    else if(parentTimescale)
        return parentTimescale->inheritTimescale();
    else
        return 1;
}

SegmentInfoCommon::SegmentInfoCommon( ICanonicalUrl *parent ) :
    ICanonicalUrl( parent ), Initializable(), Indexable(),
    duration( 0 ),
    startIndex( 0 )
{
}

SegmentInfoCommon::~SegmentInfoCommon()
{
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

void SegmentInfoCommon::appendBaseURL(const std::string &url)
{
    this->baseURLs.push_back( url );
}

Url SegmentInfoCommon::getUrlSegment() const
{
    Url ret = getParentUrlSegment();
    if (!baseURLs.empty())
        ret.append(baseURLs.front());
    return ret;
}
