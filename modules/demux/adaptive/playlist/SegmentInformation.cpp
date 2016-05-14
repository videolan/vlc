/*
 * SegmentInformation.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC Authors
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

#include "SegmentInformation.hpp"

#include "Segment.h"
#include "SegmentBase.h"
#include "SegmentList.h"
#include "SegmentTemplate.h"
#include "SegmentTimeline.h"
#include "AbstractPlaylist.hpp"
#include "BaseRepresentation.h"

#include <algorithm>
#include <cassert>

using namespace adaptive::playlist;

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
    switchpolicy = SWITCH_UNKNOWN;
}

SegmentInformation::~SegmentInformation()
{
    delete segmentBase;
    delete segmentList;
    delete mediaSegmentTemplate;
    delete baseUrl.Get();
}

AbstractPlaylist * SegmentInformation::getPlaylist() const
{
    if(parent)
        return parent->getPlaylist();
    else
        return NULL;
}

std::size_t SegmentInformation::getSegments(SegmentInfoType type, std::vector<ISegment *> &retSegments) const
{
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
                std::vector<ISegment *>::const_iterator it;
                for(it=segmentList->getSegments().begin();
                    it!=segmentList->getSegments().end(); ++it)
                {
                    std::vector<ISegment *> list = (*it)->subSegments();
                    retSegments.insert( retSegments.end(), list.begin(), list.end() );
                }
            }
            else if( segmentBase )
            {
                std::vector<ISegment *> list = segmentBase->subSegments();
                retSegments.insert( retSegments.end(), list.begin(), list.end() );
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
    {
        return parent->getSegments( type, retSegments );
    }
    else
    {
        return retSegments.size();
    }
}

std::size_t SegmentInformation::getAllSegments(std::vector<ISegment *> &retSegments) const
{
    for(int i=0; i<InfoTypeCount; i++)
    {
        std::vector<ISegment *> segs;
        if( getSegments(static_cast<SegmentInfoType>(i), segs) )
            retSegments.insert( retSegments.end(), segs.begin(), segs.end() );
    }
    return retSegments.size();
}

uint64_t SegmentInformation::getLiveStartSegmentNumber(uint64_t def) const
{
    if( mediaSegmentTemplate )
    {
        SegmentTimeline *timeline = mediaSegmentTemplate->segmentTimeline.Get();
        if(timeline)
        {
            const uint64_t start = timeline->minElementNumber();
            const uint64_t end = timeline->maxElementNumber();
            if(end > 2 && (end - start >= 2))
                return end - 2;
            else
                return start;
        }
        return mediaSegmentTemplate->startNumber.Get();
    }
    else if ( segmentList && !segmentList->getSegments().empty() )
    {
        const std::vector<ISegment *> list = segmentList->getSegments();
        if(list.size() > 3)
            return list.at(list.size() - 3)->getSequenceNumber();
        else if(!list.empty())
            return list.front()->getSequenceNumber();
        else
            return segmentList->getStartIndex();
    }
    else if( segmentBase )
    {
        const std::vector<ISegment *> list = segmentBase->subSegments();
        if(list.size() > 3)
            return list.at(list.size() - 3)->getSequenceNumber();
        else if(!list.empty())
            return list.front()->getSequenceNumber();
        else
            return segmentBase->getSequenceNumber();
    }

    if(parent)
        return parent->getLiveStartSegmentNumber(def);
    else
        return def;
}

/* Returns wanted segment, or next in sequence if not found */
ISegment * SegmentInformation::getNextSegment(SegmentInfoType type, uint64_t i_pos,
                                              uint64_t *pi_newpos, bool *pb_gap) const
{
    *pb_gap = false;
    *pi_newpos = i_pos;
    if( type != INFOTYPE_MEDIA )
        return NULL;

    std::vector<ISegment *> retSegments;
    const size_t size = getSegments( type, retSegments );
    if( size )
    {
        std::vector<ISegment *>::const_iterator it;
        for(it = retSegments.begin(); it != retSegments.end(); ++it)
        {
            ISegment *seg = *it;
            if(seg->isTemplate()) /* we don't care about seq number */
            {
                /* Check if we don't exceed timeline */
                MediaSegmentTemplate *templ = dynamic_cast<MediaSegmentTemplate*>(retSegments[0]);
                SegmentTimeline *timeline = (templ) ? templ->segmentTimeline.Get() : NULL;
                if(timeline)
                {
                    *pi_newpos = std::max(timeline->minElementNumber(), i_pos);
                    if(timeline->maxElementNumber() < i_pos)
                        return NULL;
                }
                else
                {
                    *pi_newpos = i_pos;
                    /* start number */
                    *pi_newpos = std::max((uint64_t)templ->startNumber.Get(), i_pos);
                }
                return seg;
            }
            else if(seg->getSequenceNumber() >= i_pos)
            {
                *pi_newpos = seg->getSequenceNumber();
                *pb_gap = (*pi_newpos != i_pos);
                return seg;
            }
        }
    }

    return NULL;
}

ISegment * SegmentInformation::getSegment(SegmentInfoType type, uint64_t pos) const
{
    std::vector<ISegment *> retSegments;
    const size_t size = getSegments( type, retSegments );
    if( size )
    {
        if(size == 1 && retSegments[0]->isTemplate())
        {
            MediaSegmentTemplate *templ = dynamic_cast<MediaSegmentTemplate*>(retSegments[0]);
            if(!templ || templ->segmentTimeline.Get() == NULL ||
               templ->segmentTimeline.Get()->maxElementNumber() > pos)
                return templ;
        }
        else
        {
            std::vector<ISegment *>::const_iterator it;
            for(it = retSegments.begin(); it != retSegments.end(); ++it)
            {
                ISegment *seg = *it;
                if(seg->getSequenceNumber() >= pos)
                {
                    if(seg->getSequenceNumber() == pos)
                        return seg;
                    else
                        return NULL;
                }
            }
        }
    }

    return NULL;
}

bool SegmentInformation::getSegmentNumberByTime(mtime_t time, uint64_t *ret) const
{
    if( mediaSegmentTemplate )
    {
        const uint64_t timescale = mediaSegmentTemplate->inheritTimescale();

        SegmentTimeline *timeline = mediaSegmentTemplate->segmentTimeline.Get();
        if(timeline)
        {
            time = time * timescale / CLOCK_FREQ;
            *ret = timeline->getElementNumberByScaledPlaybackTime(time);
            return true;
        }

        const mtime_t duration = mediaSegmentTemplate->duration.Get();
        *ret = mediaSegmentTemplate->startNumber.Get();
        if(duration)
        {
            *ret += time / (CLOCK_FREQ * duration / timescale);
            return true;
        }
    }
    else if ( segmentList && !segmentList->getSegments().empty() )
    {
        const uint64_t timescale = segmentList->inheritTimescale();
        time = time * timescale / CLOCK_FREQ;
        return segmentList->getSegmentNumberByScaledTime(time, ret);
    }
    else if( segmentBase )
    {
        const uint64_t timescale = inheritTimescale();
        time = time * timescale / CLOCK_FREQ;
        *ret = 0;
        const std::vector<ISegment *> list = segmentBase->subSegments();
        return SegmentInfoCommon::getSegmentNumberByScaledTime(list, time, ret);
    }

    if(parent)
        return parent->getSegmentNumberByTime(time, ret);
    else
        return false;
}

mtime_t SegmentInformation::getPlaybackTimeBySegmentNumber(uint64_t number) const
{
    SegmentList *segList;
    MediaSegmentTemplate *mediaTemplate;
    mtime_t time = 0;
    if( (mediaTemplate = inheritSegmentTemplate()) )
    {
        uint64_t timescale = mediaTemplate->inheritTimescale();
        if(mediaTemplate->segmentTimeline.Get())
        {
            time = mediaTemplate->segmentTimeline.Get()->
                    getScaledPlaybackTimeByElementNumber(number);
        }
        else
        {
            time = number * mediaTemplate->duration.Get();
        }
        time = CLOCK_FREQ * time / timescale;
    }
    else if ( (segList = inheritSegmentList()) )
    {
        time = segList->getPlaybackTimeBySegmentNumber(number);
    }

    return time;
}

SegmentInformation * SegmentInformation::getChildByID(const ID &id)
{
    std::vector<SegmentInformation *>::const_iterator it;
    for(it=childs.begin(); it!=childs.end(); ++it)
    {
        if( (*it)->getID() == id )
            return *it;
    }
    return NULL;
}

void SegmentInformation::mergeWith(SegmentInformation *updated, mtime_t prunetime)
{
    /* Support Segment List for now */
    if(segmentList && updated->segmentList)
        segmentList->mergeWith(updated->segmentList);

    if(mediaSegmentTemplate && updated->mediaSegmentTemplate)
        mediaSegmentTemplate->mergeWith(updated->mediaSegmentTemplate, prunetime);

    std::vector<SegmentInformation *>::const_iterator it;
    for(it=childs.begin(); it!=childs.end(); ++it)
    {
        SegmentInformation *child = *it;
        SegmentInformation *updatedChild = updated->getChildByID(child->getID());
        if(updatedChild)
            child->mergeWith(updatedChild, prunetime);
    }
    /* FIXME: handle difference */
}

void SegmentInformation::mergeWithTimeline(SegmentTimeline *updated)
{
    MediaSegmentTemplate *templ = inheritSegmentTemplate();
    if(templ)
    {
        SegmentTimeline *timeline = templ->segmentTimeline.Get();
        if(timeline)
            timeline->mergeWith(*updated);
    }
}

void SegmentInformation::pruneByPlaybackTime(mtime_t time)
{
    if(segmentList)
        segmentList->pruneByPlaybackTime(time);

    if(mediaSegmentTemplate)
        mediaSegmentTemplate->pruneByPlaybackTime(time);

    std::vector<SegmentInformation *>::const_iterator it;
    for(it=childs.begin(); it!=childs.end(); ++it)
        (*it)->pruneByPlaybackTime(time);
}

void SegmentInformation::pruneBySegmentNumber(uint64_t num)
{
    assert(dynamic_cast<BaseRepresentation *>(this));

    if(segmentList)
        segmentList->pruneBySegmentNumber(num);

    if(mediaSegmentTemplate)
         mediaSegmentTemplate->pruneBySequenceNumber(num);
}

uint64_t SegmentInformation::translateSegmentNumber(uint64_t num, const SegmentInformation *from) const
{
    mtime_t time = from->getPlaybackTimeBySegmentNumber(num);
    getSegmentNumberByTime(time, &num);
    return num;
}

SegmentInformation::SwitchPolicy SegmentInformation::getSwitchPolicy() const
{
    if(switchpolicy == SWITCH_UNKNOWN)
        return (parent) ? parent->getSwitchPolicy() : SWITCH_UNAVAILABLE;
    else
        return switchpolicy;
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
    if(segmentList)
    {
        segmentList->mergeWith(list);
        delete list;
    }
    else
    {
        segmentList = list;
    }
}

void SegmentInformation::setSegmentBase(SegmentBase *base)
{
    if(segmentBase)
        delete segmentBase;
    segmentBase = base;
}

void SegmentInformation::setSegmentTemplate(MediaSegmentTemplate *templ)
{
    if(mediaSegmentTemplate)
    {
        mediaSegmentTemplate->mergeWith(templ, 0);
        delete templ;
    }
    else
        mediaSegmentTemplate = templ;
}

static void insertIntoSegment(std::vector<ISegment *> &seglist, size_t start,
                              size_t end, stime_t time)
{
    std::vector<ISegment *>::iterator segIt;
    for(segIt = seglist.begin(); segIt < seglist.end(); ++segIt)
    {
        ISegment *segment = *segIt;
        if(segment->getClassId() == Segment::CLASSID_SEGMENT &&
           (end == 0 || segment->contains(end)))
        {
            SubSegment *subsegment = new SubSegment(segment, start, (end != 0) ? end : 0);
            subsegment->startTime.Set(time);
            segment->addSubSegment(subsegment);
            break;
        }
    }
}

void SegmentInformation::SplitUsingIndex(std::vector<SplitPoint> &splitlist)
{
    std::vector<ISegment *> seglist;
    getSegments(INFOTYPE_MEDIA, seglist);
    size_t prevstart = 0;
    stime_t prevtime = 0;
    const uint64_t i_timescale = inheritTimescale();

    SplitPoint split = {0,0};
    std::vector<SplitPoint>::const_iterator splitIt;
    for(splitIt = splitlist.begin(); splitIt < splitlist.end(); ++splitIt)
    {
        split = *splitIt;
        if(splitIt != splitlist.begin())
        {
            /* do previous splitpoint */
            insertIntoSegment(seglist, prevstart, split.offset - 1, prevtime);
        }
        prevstart = split.offset;
        prevtime = split.time * i_timescale / CLOCK_FREQ;
    }

    if(splitlist.size() == 1)
    {
        insertIntoSegment(seglist, prevstart, 0, prevtime);
    }
    else if(splitlist.size() > 1)
    {
        insertIntoSegment(seglist, prevstart, split.offset - 1, prevtime);
    }
}

void SegmentInformation::setSwitchPolicy(SegmentInformation::SwitchPolicy policy)
{
    switchpolicy = policy;
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
