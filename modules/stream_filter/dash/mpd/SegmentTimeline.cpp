/*****************************************************************************
 * SegmentTimeline.cpp: Implement the SegmentTimeline tag.
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

#include "SegmentTimeline.h"

#include <vlc_common.h>
#include <vlc_arrays.h>

#include <iostream>

using namespace dash::mpd;

SegmentTimeline::SegmentTimeline() :
    timescale( -1 )
{
}

SegmentTimeline::~SegmentTimeline()
{
    vlc_delete_all( this->elements );
}

int dash::mpd::SegmentTimeline::getTimescale() const
{
    return this->timescale;
}

void dash::mpd::SegmentTimeline::setTimescale(int timescale)
{
    this->timescale = timescale;
}

void dash::mpd::SegmentTimeline::addElement(dash::mpd::SegmentTimeline::Element *e)
{
    this->elements.push_back( e );
}

const SegmentTimeline::Element*    SegmentTimeline::getElement( mtime_t dts ) const
{
    if ( this->elements.size() == 0 )
        return NULL;
    int64_t     targetT = dts * this->timescale / 1000000;
    targetT -= this->elements.front()->t;

    std::list<Element*>::const_iterator     it = this->elements.begin();
    std::list<Element*>::const_iterator     end = this->elements.end();
    const Element*  res = NULL;

    while ( it != end )
    {
        if ( (*it)->t > targetT )
            return res;
        res = *it;
        ++it;
    }
    std::cerr << "No more element to be used." << std::endl;
    return NULL;
}

dash::mpd::SegmentTimeline::Element::Element() :
    r( 0 )
{
}
