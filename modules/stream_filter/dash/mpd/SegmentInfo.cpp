/*
 * SegmentInfo.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "SegmentInfo.h"

using namespace dash::mpd;
using namespace dash::exception;

SegmentInfo::SegmentInfo(std::map<std::string,std::string> attr)
{
    this->attributes    = attr;
    this->initSeg       = NULL;
}
SegmentInfo::~SegmentInfo   ()
{
    for(size_t i = 0; i < this->segments.size(); i++)
        delete(this->segments.at(i));

    delete(this->initSeg);
}

InitSegment*            SegmentInfo::getInitSegment     () throw(ElementNotPresentException)
{
    if(this->initSeg == NULL)
        throw ElementNotPresentException();

    return this->initSeg;
}
std::vector<Segment*>   SegmentInfo::getSegments        ()
{
    return this->segments;
}
void                    SegmentInfo::addSegment         (Segment *seg)
{
    this->segments.push_back(seg);
}
void                    SegmentInfo::setInitSegment     (InitSegment *initSeg)
{
    this->initSeg = initSeg;
}
