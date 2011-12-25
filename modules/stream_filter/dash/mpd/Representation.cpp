/*
 * Representation.cpp
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

#include <cstdlib>

#include "Representation.h"

using namespace dash::mpd;
using namespace dash::exception;

Representation::Representation  (const std::map<std::string, std::string>&  attributes) :
    qualityRanking( -1 ),
    attributes( attributes ),
    segmentInfo( NULL ),
    trickModeType( NULL )
{
}

Representation::~Representation ()
{
    delete(this->segmentInfo);
    delete(this->trickModeType);
}

const std::string&  Representation::getId                   () const
{
    return this->id;
}

void    Representation::setId(const std::string &id)
{
    if ( id.empty() == false )
        this->id = id;
}

int     Representation::getBandwidth            () const
{
    return this->bandwidth;
}

void    Representation::setBandwidth( int bandwidth )
{
    if ( bandwidth >= 0 )
        this->bandwidth = bandwidth;
}

SegmentInfo*        Representation::getSegmentInfo          () const throw(ElementNotPresentException)
{
    if(this->segmentInfo == NULL)
        throw ElementNotPresentException();

    return this->segmentInfo;
}
TrickModeType*      Representation::getTrickModeType        () const throw(ElementNotPresentException)
{
    if(this->segmentInfo == NULL)
        throw ElementNotPresentException();

    return this->trickModeType;
}

void                Representation::setTrickModeType        (TrickModeType *trickModeType)
{
    this->trickModeType = trickModeType;
}

void                Representation::setSegmentInfo          (SegmentInfo *info)
{
    this->segmentInfo = info;
}


int Representation::getQualityRanking() const
{
    return this->qualityRanking;
}

void Representation::setQualityRanking( int qualityRanking )
{
    if ( qualityRanking > 0 )
        this->qualityRanking = qualityRanking;
}

const std::list<const Representation*>&     Representation::getDependencies() const
{
    return this->dependencies;
}

void Representation::addDependency(const Representation *dep)
{
    if ( dep != NULL )
        this->dependencies.push_back( dep );
}
