/*****************************************************************************
 * SegmentTemplate.cpp: Implement the UrlTemplate element.
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

#include "SegmentTemplate.h"
#include "SegmentTimeline.h"
#include "Representation.h"
#include "AdaptationSet.h"
#include "SegmentInfoDefault.h"

using namespace dash::mpd;

SegmentTemplate::SegmentTemplate( ICanonicalUrl *parent ) :
    Segment( parent ),
    startIndex( 0 )
{
    debugName = "SegmentTemplate";
    classId = Segment::CLASSID_SEGMENT;
    duration.Set(0);
    timescale.Set(0);
}

Url SegmentTemplate::getUrlSegment() const
{
    Url ret = getParentUrlSegment();
    if (!sourceUrl.empty())
    {
        ret.append(Url::Component(sourceUrl, this));
    }
    return ret;
}

size_t SegmentTemplate::getStartIndex() const
{
    return startIndex;
}

void SegmentTemplate::setStartIndex(size_t i)
{
    startIndex = i;
}

bool            SegmentTemplate::isSingleShot() const
{
    return false;
}

InitSegmentTemplate::InitSegmentTemplate( ICanonicalUrl *parent ) :
    SegmentTemplate(parent)
{
    debugName = "InitSegmentTemplate";
    classId = InitSegment::CLASSID_INITSEGMENT;
}
