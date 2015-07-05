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

#include "AdaptationSet.h"
#include "Representation.h"
#include "Period.h"
#include "../DASHStreamFormat.hpp"

using namespace dash::mpd;

AdaptationSet::AdaptationSet(Period *period) :
    adaptative::playlist::BaseAdaptationSet( period ),
    DASHCommonAttributesElements(),
    subsegmentAlignmentFlag( false )
{
}

AdaptationSet::~AdaptationSet()
{
}

StreamFormat AdaptationSet::getStreamFormat() const
{
    if(!getMimeType().empty())
    {
        return DASHStreamFormat::mimeToFormat(getMimeType());
    }
    else if (!representations.empty())
    {
        return representations.front()->getStreamFormat();
    }
    else
    {
        return StreamFormat();
    }
}

bool AdaptationSet::getSubsegmentAlignmentFlag() const
{
    return subsegmentAlignmentFlag;
}

void AdaptationSet::setSubsegmentAlignmentFlag(bool alignment)
{
    subsegmentAlignmentFlag = alignment;
}

const Representation *AdaptationSet::getRepresentationById(const std::string &id) const
{
    std::vector<BaseRepresentation*>::const_iterator it = representations.begin();
    std::vector<BaseRepresentation*>::const_iterator end = representations.end();

    while ( it != end )
    {
        Representation *rep = dynamic_cast<Representation *>(*it);
        if ( rep->getId() == id )
            return rep;
        ++it;
    }
    return NULL;
}
