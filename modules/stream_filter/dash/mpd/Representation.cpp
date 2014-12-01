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

Representation::Representation  ( MPD *mpd_ ) :
                ICanonicalUrl   ( mpd_ ),
                mpd             ( mpd_ ),
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
    delete segmentBase;
    delete segmentList;
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

std::vector<ISegment *> Representation::getSegments() const
{
    std::vector<ISegment *>  retSegments;

    if ( segmentInfo )
    {
        /* init segments are always single segment */
        retSegments.push_back( segmentInfo->getInitialisationSegment() );

        if ( !segmentInfo->getSegments().empty() )
        {
            std::vector<Segment *>::const_iterator it;
            for(it=segmentInfo->getSegments().begin();
                it!=segmentInfo->getSegments().end(); it++)
            {
                std::vector<ISegment *> list = (*it)->subSegments();
                retSegments.insert( retSegments.end(), list.begin(), list.end() );
            }
        }
    }
    else
    {
        /* init segments are always single segment */
        if( segmentBase && segmentBase->getInitSegment() )
            retSegments.push_back( segmentBase->getInitSegment() );

        if ( segmentList && !segmentList->getSegments().empty() )
        {
            std::vector<Segment *>::const_iterator it;
            for(it=segmentList->getSegments().begin();
                it!=segmentList->getSegments().end(); it++)
            {
                std::vector<ISegment *> list = (*it)->subSegments();
                retSegments.insert( retSegments.end(), list.begin(), list.end() );
            }
        }
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
    std::vector<ISegment *> list = getSegments();
    std::vector<ISegment *>::const_iterator l;
    for(l = list.begin(); l < list.end(); l++)
        ret.push_back((*l)->toString());

    return ret;
}

std::string Representation::getUrlSegment() const
{
    std::string ret = getParentUrlSegment();
    if (baseUrl)
        ret.append(baseUrl->getUrl());
    return ret;
}

MPD * Representation::getMPD() const
{
    return mpd;
}

static void insertIntoSegment(std::vector<Segment *> &seglist, size_t start,
                              size_t end, mtime_t time)
{
    std::vector<Segment *>::iterator segIt;
    for(segIt = seglist.begin(); segIt < seglist.end(); segIt++)
    {
        Segment *segment = *segIt;
        if(segment->getClassId() == Segment::CLASSID_SEGMENT &&
           segment->contains(end + segment->getOffset()))
        {
            SubSegment *subsegment = new SubSegment(segment,
                                                    start + segment->getOffset(),
                                                    end + segment->getOffset());
            segment->addSubSegment(subsegment);
            segment->setStartTime(time);
            break;
        }
    }
}

void Representation::SplitUsingIndex(std::vector<SplitPoint> &splitlist)
{
    std::vector<Segment *> seglist = segmentList->getSegments();
    std::vector<SplitPoint>::const_iterator splitIt;
    size_t start = 0, end = 0;
    mtime_t time = 0;

    for(splitIt = splitlist.begin(); splitIt < splitlist.end(); splitIt++)
    {
        start = end;
        SplitPoint split = *splitIt;
        end = split.offset;
        if(splitIt == splitlist.begin() && split.offset == 0)
            continue;
        time = split.time;
        insertIntoSegment(seglist, start, end, time);
        end++;
    }

    if(start != 0)
    {
        start = end;
        end = 0;
        insertIntoSegment(seglist, start, end, time);
    }
}
