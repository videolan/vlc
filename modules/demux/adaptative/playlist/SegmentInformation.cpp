/*
 * SegmentInformation.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN Authors
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
#include "SegmentInformation.hpp"

#include "Segment.h"
#include "SegmentBase.h"
#include "SegmentList.h"
#include "SegmentTemplate.h"
#include "SegmentTimeline.h"
#include "AbstractPlaylist.hpp"

using namespace adaptative::playlist;
using namespace std;

SegmentInformation::SegmentInformation(SegmentInformation *parent_) :
    ICanonicalUrl( parent_ ),
    TimescaleAble( parent_ )
{
    parent = parent_;
    init();
}

SegmentInformation::SegmentInformation(AbstractPlaylist * parent_) :
    ICanonicalUrl(parent_),
    TimescaleAble()
{
    parent = NULL;
    init();
}

void SegmentInformation::init()
{
    baseUrl.Set(NULL);
    segmentBase = NULL;
    segmentList = NULL;
    mediaSegmentTemplate = NULL;
    bitswitch_policy = BITSWITCH_INHERIT;
}

SegmentInformation::~SegmentInformation()
{
    delete segmentBase;
    delete segmentList;
    delete mediaSegmentTemplate;
}

vector<ISegment *> SegmentInformation::getSegments(SegmentInfoType type) const
{
    vector<ISegment *> retSegments;

    switch (type)
    {
        case INFOTYPE_INIT:
        {
            /* init segments are always single segment */
            if( segmentBase && segmentBase->initialisationSegment.Get() )
            {
                retSegments.push_back( segmentBase->initialisationSegment.Get() );
            }
            else if( segmentList && segmentList->initialisationSegment.Get() )
            {
                retSegments.push_back( segmentList->initialisationSegment.Get() );
            }
            else if( mediaSegmentTemplate && mediaSegmentTemplate->initialisationSegment.Get() )
            {
                retSegments.push_back( mediaSegmentTemplate->initialisationSegment.Get() );
            }
        }
        break;

        case INFOTYPE_MEDIA:
        {
            if( mediaSegmentTemplate )
            {
                retSegments.push_back( mediaSegmentTemplate );
            }
            else if ( segmentList && !segmentList->getSegments().empty() )
            {
                std::vector<Segment *>::const_iterator it;
                for(it=segmentList->getSegments().begin();
                    it!=segmentList->getSegments().end(); ++it)
                {
                    std::vector<ISegment *> list = (*it)->subSegments();
                    retSegments.insert( retSegments.end(), list.begin(), list.end() );
                }
            }
            else if( segmentBase )
            {
                retSegments.push_back( segmentBase );
            }
        }
        break;

        case INFOTYPE_INDEX:
        {
            /* index segments are always single segment */
            if( segmentBase && segmentBase->indexSegment.Get() )
            {
                retSegments.push_back( segmentBase->indexSegment.Get() );
            }
            else if( segmentList && segmentList->indexSegment.Get() )
            {
                retSegments.push_back( segmentList->indexSegment.Get() );
            }
            // templated index ?
        }

        default:
        break;
    }

    if( retSegments.empty() && parent )
        return parent->getSegments( type );
    else
        return retSegments;
}

vector<ISegment *> SegmentInformation::getSegments() const
{
    vector<ISegment *> retSegments;
    for(int i=0; i<InfoTypeCount; i++)
    {
        vector<ISegment *> segs = getSegments(static_cast<SegmentInfoType>(i));
        retSegments.insert( retSegments.end(), segs.begin(), segs.end() );
    }
    return retSegments;
}

ISegment * SegmentInformation::getSegment(SegmentInfoType type, uint64_t pos) const
{
    ISegment *segment = NULL;

    vector<ISegment *> retSegments = getSegments( type );
    const size_t size = retSegments.size();
    if( size )
    {
        /* check if that's a template (fixme: find a better way) */
        MediaSegmentTemplate *templ;
        if( size == 1 && (templ = dynamic_cast<MediaSegmentTemplate*>(retSegments[0])) )
        {
            if(templ->segmentTimeline.Get() == NULL ||
               templ->segmentTimeline.Get()->maxElementNumber() > pos)
                return templ;
        }
        else if( pos < size )
        {
            segment = retSegments[pos];
        }
    }

    return segment;
}

bool SegmentInformation::getSegmentNumberByTime(mtime_t time, uint64_t *ret) const
{
    SegmentList *segList;
    MediaSegmentTemplate *mediaTemplate;
    uint64_t timescale = 1;
    mtime_t duration = 0;

    if( (mediaTemplate = inheritSegmentTemplate()) )
    {
        timescale = mediaTemplate->inheritTimescale();
        duration = mediaTemplate->duration.Get();
    }
    else if ( (segList = inheritSegmentList()) )
    {
        timescale = segList->inheritTimescale();
        duration = segList->getDuration();
    }

    if(duration)
    {
        *ret = time / (CLOCK_FREQ * duration / timescale);
        return true;
    }

    return false;
}

mtime_t SegmentInformation::getPlaybackTimeBySegmentNumber(uint64_t number) const
{
    SegmentList *segList;
    MediaSegmentTemplate *mediaTemplate;
    uint64_t timescale = 1;
    mtime_t time = 0;
    if( (mediaTemplate = inheritSegmentTemplate()) )
    {
        timescale = mediaTemplate->inheritTimescale();
        if(mediaTemplate->segmentTimeline.Get())
        {
            time = mediaTemplate->segmentTimeline.Get()->
                    getScaledPlaybackTimeByElementNumber(number);
        }
        else
        {
            time = number * mediaTemplate->duration.Get();
        }
    }
    else if ( (segList = inheritSegmentList()) )
    {
        timescale = segList->inheritTimescale();
        time = number * segList->getDuration();
    }

    if(time)
        time = CLOCK_FREQ * time / timescale;

    return time;
}

void SegmentInformation::collectTimelines(std::vector<SegmentTimeline *> *timelines) const
{
    if(mediaSegmentTemplate && mediaSegmentTemplate->segmentTimeline.Get())
        timelines->push_back(mediaSegmentTemplate->segmentTimeline.Get());

    std::vector<SegmentInformation *>::const_iterator it;
    for(it = childs.begin(); it != childs.end(); ++it)
        (*it)->collectTimelines(timelines);
}

bool SegmentInformation::canBitswitch() const
{
    if(bitswitch_policy == BITSWITCH_INHERIT)
        return (parent) ? parent->canBitswitch() : false;
    else
        return (bitswitch_policy == BITSWITCH_YES);
}

mtime_t SegmentInformation::getPeriodStart() const
{
    if(parent)
        return parent->getPeriodStart();
    else
        return 0;
}

void SegmentInformation::setSegmentList(SegmentList *list)
{
    segmentList = list;
}

void SegmentInformation::setSegmentBase(SegmentBase *base)
{
    segmentBase = base;
}

void SegmentInformation::setSegmentTemplate(MediaSegmentTemplate *templ)
{
    mediaSegmentTemplate = templ;
}

static void insertIntoSegment(std::vector<ISegment *> &seglist, size_t start,
                              size_t end, mtime_t time)
{
    std::vector<ISegment *>::iterator segIt;
    for(segIt = seglist.begin(); segIt < seglist.end(); ++segIt)
    {
        ISegment *segment = *segIt;
        if(segment->getClassId() == Segment::CLASSID_SEGMENT &&
           segment->contains(end + segment->getOffset()))
        {
            SubSegment *subsegment = new SubSegment(segment,
                                                    start + segment->getOffset(),
                                                    (end != 0) ? end + segment->getOffset() : 0);
            segment->addSubSegment(subsegment);
            segment->startTime.Set(time);
            break;
        }
    }
}

void SegmentInformation::SplitUsingIndex(std::vector<SplitPoint> &splitlist)
{
    std::vector<ISegment *> seglist = getSegments(INFOTYPE_MEDIA);
    std::vector<SplitPoint>::const_iterator splitIt;
    size_t start = 0, end = 0;
    mtime_t time = 0;

    for(splitIt = splitlist.begin(); splitIt < splitlist.end(); ++splitIt)
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

void SegmentInformation::setBitstreamSwitching(bool bitswitch)
{
    bitswitch_policy = (bitswitch) ? BITSWITCH_YES : BITSWITCH_NO;
}

Url SegmentInformation::getUrlSegment() const
{
    if(baseUrl.Get() && baseUrl.Get()->hasScheme())
    {
        return *(baseUrl.Get());
    }
    else
    {
        Url ret = getParentUrlSegment();
        if (baseUrl.Get())
            ret.append(*(baseUrl.Get()));
        return ret;
    }
}

SegmentBase * SegmentInformation::inheritSegmentBase() const
{
    if(segmentBase)
        return segmentBase;
    else if (parent)
        return parent->inheritSegmentBase();
    else
        return NULL;
}

SegmentList * SegmentInformation::inheritSegmentList() const
{
    if(segmentList)
        return segmentList;
    else if (parent)
        return parent->inheritSegmentList();
    else
        return NULL;
}

MediaSegmentTemplate * SegmentInformation::inheritSegmentTemplate() const
{
    if(mediaSegmentTemplate)
        return mediaSegmentTemplate;
    else if (parent)
        return parent->inheritSegmentTemplate();
    else
        return NULL;
}
