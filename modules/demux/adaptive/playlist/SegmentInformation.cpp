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
    const mtime_t i_max_buffering = getPlaylist()->getMaxBuffering() +
                                    /* FIXME: add dynamic pts-delay */ CLOCK_FREQ;

    /* Try to never buffer up to really end */
    const uint64_t OFFSET_FROM_END = 3;

    if( mediaSegmentTemplate )
    {
        uint64_t start = 0;
        uint64_t end = 0;
        const Timescale timescale = mediaSegmentTemplate->inheritTimescale();

        SegmentTimeline *timeline = mediaSegmentTemplate->segmentTimeline.Get();
        if( timeline )
        {
            start = timeline->minElementNumber();
            end = timeline->maxElementNumber();
            /* Try to never buffer up to really end */
            end = end - std::min(end - start, OFFSET_FROM_END);
            stime_t endtime, duration;
            timeline->getScaledPlaybackTimeDurationBySegmentNumber( end, &endtime, &duration );

            if( endtime + duration <= timescale.ToScaled( i_max_buffering ) )
                return start;

            uint64_t number = timeline->getElementNumberByScaledPlaybackTime(
                                        endtime + duration - timescale.ToScaled( i_max_buffering ) );
            if( number < start )
                number = start;
            return number;
        }
        /* Else compute, current time and timeshiftdepth based */
        else if( mediaSegmentTemplate->duration.Get() )
        {
            mtime_t i_delay = getPlaylist()->suggestedPresentationDelay.Get();

            if( i_delay == 0 || i_delay > getPlaylist()->timeShiftBufferDepth.Get() )
                 i_delay = getPlaylist()->timeShiftBufferDepth.Get();

            if( i_delay < getPlaylist()->getMinBuffering() )
                i_delay = getPlaylist()->getMinBuffering();

            const uint64_t startnumber = mediaSegmentTemplate->startNumber.Get();
            end = mediaSegmentTemplate->getCurrentLiveTemplateNumber();

            const uint64_t count = timescale.ToScaled( i_delay ) / mediaSegmentTemplate->duration.Get();
            if( startnumber + count >= end )
                start = startnumber;
            else
                start = end - count;

            const uint64_t bufcount = ( OFFSET_FROM_END + timescale.ToScaled(i_max_buffering) /
                                        mediaSegmentTemplate->duration.Get() );

            return ( end - start > bufcount ) ? end - bufcount : start;
        }
    }
    else if ( segmentList && !segmentList->getSegments().empty() )
    {
        const Timescale timescale = segmentList->inheritTimescale();
        const std::vector<ISegment *> list = segmentList->getSegments();

        const ISegment *back = list.back();
        const stime_t bufferingstart = back->startTime.Get() + back->duration.Get() - timescale.ToScaled( i_max_buffering );
        uint64_t number;
        if( !segmentList->getSegmentNumberByScaledTime( bufferingstart, &number ) )
            return list.front()->getSequenceNumber();
        if( number > list.front()->getSequenceNumber() + OFFSET_FROM_END )
            number -= OFFSET_FROM_END;
        else
            number = list.front()->getSequenceNumber();
        return number;
    }
    else if( segmentBase )
    {
        const std::vector<ISegment *> list = segmentBase->subSegments();
        if(!list.empty())
            return segmentBase->getSequenceNumber();

        const Timescale timescale = inheritTimescale();
        const ISegment *back = list.back();
        const stime_t bufferingstart = back->startTime.Get() -
                (OFFSET_FROM_END * back->duration.Get())- timescale.ToScaled( i_max_buffering );
        uint64_t number;
        if( !SegmentInfoCommon::getSegmentNumberByScaledTime( list, bufferingstart, &number ) )
            return list.front()->getSequenceNumber();
        return number;
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
        const Timescale timescale = mediaSegmentTemplate->inheritTimescale();

        SegmentTimeline *timeline = mediaSegmentTemplate->segmentTimeline.Get();
        if(timeline)
        {
            time = timescale.ToScaled(time);
            *ret = timeline->getElementNumberByScaledPlaybackTime(time);
            return true;
        }

        const stime_t duration = mediaSegmentTemplate->duration.Get();
        if( duration )
        {
            if( getPlaylist()->isLive() )
            {
                *ret = getLiveStartSegmentNumber( mediaSegmentTemplate->startNumber.Get() );
            }
            else
            {
                *ret = mediaSegmentTemplate->startNumber.Get();
                *ret += timescale.ToScaled(time) / duration;
            }
            return true;
        }
    }
    else if ( segmentList && !segmentList->getSegments().empty() )
    {
        const Timescale timescale = segmentList->inheritTimescale();
        time = timescale.ToScaled(time);
        return segmentList->getSegmentNumberByScaledTime(time, ret);
    }
    else if( segmentBase )
    {
        const Timescale timescale = inheritTimescale();
        time = timescale.ToScaled(time);
        *ret = 0;
        const std::vector<ISegment *> list = segmentBase->subSegments();
        return SegmentInfoCommon::getSegmentNumberByScaledTime(list, time, ret);
    }

    if(parent)
        return parent->getSegmentNumberByTime(time, ret);
    else
        return false;
}

bool SegmentInformation::getPlaybackTimeDurationBySegmentNumber(uint64_t number,
                                                                mtime_t *time, mtime_t *duration) const
{
    SegmentList *segList;
    MediaSegmentTemplate *mediaTemplate;

    if( (mediaTemplate = inheritSegmentTemplate()) )
    {
        const Timescale timescale = mediaTemplate->inheritTimescale();

        stime_t stime, sduration;
        if(mediaTemplate->segmentTimeline.Get())
        {
            mediaTemplate->segmentTimeline.Get()->
                getScaledPlaybackTimeDurationBySegmentNumber(number, &stime, &sduration);
        }
        else
        {
            stime = number * mediaTemplate->duration.Get();
            sduration = mediaTemplate->duration.Get();
        }
        *time = timescale.ToTime(stime);
        *duration = timescale.ToTime(sduration);
        return true;
    }
    else if ( (segList = inheritSegmentList()) )
    {
        return segList->getPlaybackTimeDurationBySegmentNumber(number, time, duration);
    }
    else
    {
        const Timescale timescale = inheritTimescale();
        const ISegment *segment = getSegment(INFOTYPE_MEDIA, number);
        if( segment )
        {
            *time = timescale.ToTime(segment->startTime.Get());
            *duration = timescale.ToTime(segment->duration.Get());
            return true;
        }
    }

    return false;
}

SegmentInformation * SegmentInformation::getChildByID(const adaptive::ID &id)
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
    mtime_t time, duration;
    if( from->getPlaybackTimeDurationBySegmentNumber(num, &time, &duration) )
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

void SegmentInformation::appendSegmentList(SegmentList *list, bool restamp)
{
    if(segmentList)
    {
        segmentList->mergeWith(list, restamp);
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
                              size_t end, stime_t time, stime_t duration)
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
            subsegment->duration.Set(duration);
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
    const Timescale timescale = inheritTimescale();

    SplitPoint split = {0,0,0};
    std::vector<SplitPoint>::const_iterator splitIt;
    for(splitIt = splitlist.begin(); splitIt < splitlist.end(); ++splitIt)
    {
        split = *splitIt;
        if(splitIt != splitlist.begin())
        {
            /* do previous splitpoint */
            const stime_t duration = timescale.ToScaled(split.duration);
            insertIntoSegment(seglist, prevstart, split.offset - 1, prevtime, duration);
        }
        prevstart = split.offset;
        prevtime = timescale.ToScaled(split.time);
    }

    if(splitlist.size() == 1)
    {
        const stime_t duration = timescale.ToScaled(split.duration);
        insertIntoSegment(seglist, prevstart, 0, prevtime, duration);
    }
    else if(splitlist.size() > 1)
    {
        const stime_t duration = timescale.ToScaled(split.duration);
        insertIntoSegment(seglist, prevstart, split.offset - 1, prevtime, duration);
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
