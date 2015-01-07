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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "SegmentTimeline.h"

using namespace dash::mpd;

SegmentTimeline::SegmentTimeline()
{
}

SegmentTimeline::~SegmentTimeline()
{
    std::list<Element *>::iterator it;
    for(it = elements.begin(); it != elements.end(); it++)
        delete *it;
}

void SegmentTimeline::addElement(mtime_t d, uint64_t r, mtime_t t)
{
    Element *element = new (std::nothrow) Element(d, r, t);
    if(element)
        elements.push_back(element);
}

SegmentTimeline::Element::Element(mtime_t d_, uint64_t r_, mtime_t t_)
{
    d = d_;
    t = t_;
    r = r_;
}
