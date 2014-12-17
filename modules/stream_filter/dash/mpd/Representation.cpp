/*
 * Representation.cpp
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

#include <cstdlib>

#include "Representation.h"
#include "mpd/AdaptationSet.h"
#include "mpd/MPD.h"
#include "mpd/SegmentTemplate.h"

using namespace dash::mpd;

Representation::Representation  ( AdaptationSet *set, MPD *mpd_ ) :
                SegmentInformation( set ),
                mpd             ( mpd_ ),
                adaptationSet   ( set ),
                bandwidth       (0),
                qualityRanking  ( -1 ),
                trickModeType   ( NULL ),
                baseUrl         ( NULL ),
                width           (0),
                height          (0)
{
}

Representation::~Representation ()
{
    delete(this->trickModeType);
    delete baseUrl;
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

uint64_t     Representation::getBandwidth            () const
{
    return this->bandwidth;
}

void    Representation::setBandwidth( uint64_t bandwidth )
{
    this->bandwidth = bandwidth;
}


TrickModeType*      Representation::getTrickModeType        () const
{
    return this->trickModeType;
}

void                Representation::setTrickMode        (TrickModeType *trickModeType)
{
    this->trickModeType = trickModeType;
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


void Representation::setBaseUrl(BaseUrl *base)
{
    baseUrl = base;
}

void                Representation::setWidth                (int width)
{
    this->width = width;
}
int                 Representation::getWidth                () const
{
    return this->width;
}
void                Representation::setHeight               (int height)
{
    this->height = height;
}
int                 Representation::getHeight               () const
{
    return this->height;
}

std::vector<std::string> Representation::toString() const
{
    std::vector<std::string> ret;
    ret.push_back(std::string("  Representation"));
    std::vector<ISegment *> list = getSegments();
    std::vector<ISegment *>::const_iterator l;
    for(l = list.begin(); l < list.end(); l++)
        ret.push_back((*l)->toString());

    return ret;
}

Url Representation::getUrlSegment() const
{
    Url ret = getParentUrlSegment();
    if (baseUrl)
        ret.append(baseUrl->getUrl());
    return ret;
}

MPD * Representation::getMPD() const
{
    return mpd;
}
