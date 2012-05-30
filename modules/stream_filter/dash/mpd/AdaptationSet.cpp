/*
 * AdaptationSet.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
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

#include "AdaptationSet.h"

#include <vlc_common.h>
#include <vlc_arrays.h>

#include "SegmentInfoDefault.h"

using namespace dash::mpd;

AdaptationSet::AdaptationSet() :
    subsegmentAlignmentFlag( false ),
    segmentInfoDefault( NULL ),
    isBitstreamSwitching( false )
{
}

AdaptationSet::~AdaptationSet   ()
{
    delete this->segmentInfoDefault;
    vlc_delete_all( this->representations );
}

bool                AdaptationSet::getSubsegmentAlignmentFlag() const
{
    return this->subsegmentAlignmentFlag;
}

void AdaptationSet::setSubsegmentAlignmentFlag(bool alignment)
{
    this->subsegmentAlignmentFlag = alignment;
}

std::vector<Representation*>    AdaptationSet::getRepresentations       ()
{
    return this->representations;
}

const Representation *AdaptationSet::getRepresentationById(const std::string &id) const
{
    std::vector<Representation*>::const_iterator    it = this->representations.begin();
    std::vector<Representation*>::const_iterator    end = this->representations.end();

    while ( it != end )
    {
        if ( (*it)->getId() == id )
            return *it;
        ++it;
    }
    return NULL;
}

const SegmentInfoDefault *AdaptationSet::getSegmentInfoDefault() const
{
    return this->segmentInfoDefault;
}

void AdaptationSet::setSegmentInfoDefault(const SegmentInfoDefault *seg)
{
    if ( seg != NULL )
        this->segmentInfoDefault = seg;
}

void                            AdaptationSet::addRepresentation        (Representation *rep)
{
    this->representations.push_back(rep);
}

void AdaptationSet::setBitstreamSwitching  (bool value)
{
    this->isBitstreamSwitching = value;
}

bool AdaptationSet::getBitstreamSwitching  () const
{
    return this->isBitstreamSwitching;
}
