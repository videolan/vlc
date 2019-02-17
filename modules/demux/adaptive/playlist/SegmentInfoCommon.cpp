/*****************************************************************************
 * SegmentInfoCommon.cpp: Implement the common part for both SegmentInfoDefault
 *                        and SegmentInfo
 *****************************************************************************
 * Copyright (C) 1998-2007 VLC authors and VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

using namespace adaptive::playlist;

SegmentInfoCommon::SegmentInfoCommon( ICanonicalUrl *parent ) :
    ICanonicalUrl( parent ),
    startIndex( 0 )
{
    duration.Set(0);
}

SegmentInfoCommon::~SegmentInfoCommon()
{
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

bool SegmentInfoCommon::getSegmentNumberByScaledTime(const std::vector<ISegment *> &segments,
                                                      stime_t time, uint64_t *ret)
{
    if(segments.empty() || (segments.size() > 1 && segments[1]->startTime.Get() == 0) )
        return false;

    *ret = 0;

    std::vector<ISegment *>::const_iterator it = segments.begin();
    while(it != segments.end())
    {
        const ISegment *seg = *it;
        if(seg->startTime.Get() > time)
        {
            if(it == segments.begin())
                return false;
            else
                break;
        }

        *ret = seg->getSequenceNumber();
        it++;
    }

    return true;
}
