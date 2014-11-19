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
#include "mpd/MPD.h"

using namespace dash::mpd;

Representation::Representation  ( MPD *mpd ) :
                ICanonicalUrl   ( mpd ),
                bandwidth       (0),
                qualityRanking  ( -1 ),
                segmentInfo     ( NULL ),
                trickModeType   ( NULL ),
                parentGroup     ( NULL ),
                segmentBase     ( NULL ),
                segmentList     ( NULL ),
                baseUrl         ( NULL ),
                width           (0),
                height          (0)
{
}

Representation::~Representation ()
{
    delete(this->segmentInfo);
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

SegmentInfo*        Representation::getSegmentInfo() const
{
    return this->segmentInfo;
}

TrickModeType*      Representation::getTrickModeType        () const
{
    return this->trickModeType;
}

void                Representation::setTrickMode        (TrickModeType *trickModeType)
{
    this->trickModeType = trickModeType;
}

const AdaptationSet *Representation::getParentGroup() const
{
    return this->parentGroup;
}

void Representation::setParentGroup(const AdaptationSet *group)
{
    if ( group != NULL )
        this->parentGroup = group;
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

std::vector<Segment *> Representation::getSegments() const
{
    std::vector<Segment *>  retSegments;

    if ( segmentInfo )
    {
        retSegments.push_back( segmentInfo->getInitialisationSegment() );

        if ( !segmentInfo->getSegments().empty() )
            retSegments.insert( retSegments.end(),
                                segmentInfo->getSegments().begin(),
                                segmentInfo->getSegments().end() );
    }
    else
    {
        if( segmentBase && segmentBase->getInitSegment() )
            retSegments.push_back( segmentBase->getInitSegment() );

        if ( segmentList )
            retSegments.insert( retSegments.end(),
                                segmentList->getSegments().begin(),
                                segmentList->getSegments().end() );
    }

    return retSegments;
}

void                Representation::setSegmentList          (SegmentList *list)
{
    this->segmentList = list;
}

void                Representation::setSegmentBase          (SegmentBase *base)
{
    this->segmentBase = base;
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
    ret.push_back(std::string("    InitSeg url=")
                  .append(segmentBase->getInitSegment()->getSourceUrl()));
    if (segmentList)
    {
        std::vector<Segment *>::const_iterator l;
        for(l = segmentList->getSegments().begin();
            l < segmentList->getSegments().end(); l++)
        {
            ret.push_back(std::string("    Segment url=")
                          .append((*l)->getSourceUrl()));
        }
    }
    return ret;
}

std::string Representation::getUrlSegment() const
{
    std::string ret = getParentUrlSegment();
    if (baseUrl)
        ret.append(baseUrl->getUrl());
    return ret;
}
