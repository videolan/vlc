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
#include "../encryption/CommonEncryption.hpp"

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
                std::vector<ISegment *> list = segmentList->getSegments();
                retSegments.insert( retSegments.end(), list.begin(), list.end() );
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
                const SegmentTimeline *timeline = (templ) ? templ->inheritSegmentTimeline() : NULL;
                if(timeline)
                {
                    *pi_newpos = std::max(timeline->minElementNumber(), i_pos);
                    if(timeline->maxElementNumber() < i_pos)
                        return NULL;
                }
                else
                {
                    /* check template upper bound */
                    if(!getPlaylist()->isLive())
                    {
                        const Timescale timescale = templ->inheritTimescale();
                        const stime_t segmentduration = templ->inheritDuration();
                        vlc_tick_t totalduration = getPeriodDuration();
                        if(totalduration == 0)
                            totalduration = getPlaylist()->duration.Get();
                        if(totalduration && segmentduration)
                        {
                            uint64_t endnum = templ->inheritStartNumber() +
                                    (timescale.ToScaled(totalduration) + segmentduration - 1) / segmentduration;
                            if(i_pos >= endnum)
                            {
                                *pi_newpos = i_pos;
                                return NULL;
                            }
                        }
                    }
                    *pi_newpos = i_pos;
                    /* start number */
                    *pi_newpos = std::max(templ->inheritStartNumber(), i_pos);
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
            const SegmentTimeline *tl = templ->inheritSegmentTimeline();
            if(!templ || tl == NULL || tl->maxElementNumber() > pos)
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

void SegmentInformation::updateWith(SegmentInformation *updated)
{
    /* Support Segment List for now */
    if(segmentList && updated->segmentList)
        segmentList->updateWith(updated->segmentList);

    if(mediaSegmentTemplate && updated->mediaSegmentTemplate)
        mediaSegmentTemplate->updateWith(updated->mediaSegmentTemplate);

    std::vector<SegmentInformation *>::const_iterator it;
    for(it=childs.begin(); it!=childs.end(); ++it)
    {
        SegmentInformation *child = *it;
        SegmentInformation *updatedChild = updated->getChildByID(child->getID());
        if(updatedChild)
            child->updateWith(updatedChild);
    }
    /* FIXME: handle difference */
}

void SegmentInformation::mergeWithTimeline(SegmentTimeline *updated)
{
    MediaSegmentTemplate *templ = inheritSegmentTemplate();
    if(templ)
    {
        SegmentTimeline *timeline = templ->inheritSegmentTimeline();
        if(timeline)
            timeline->updateWith(*updated);
    }
}

void SegmentInformation::pruneByPlaybackTime(vlc_tick_t time)
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

const CommonEncryption & SegmentInformation::intheritEncryption() const
{
    if(parent && commonEncryption.method == CommonEncryption::Method::NONE)
        return parent->intheritEncryption();
    return commonEncryption;
}

void SegmentInformation::setEncryption(const CommonEncryption &enc)
{
    commonEncryption = enc;
}

vlc_tick_t SegmentInformation::getPeriodStart() const
{
    if(parent)
        return parent->getPeriodStart();
    else
        return 0;
}

vlc_tick_t SegmentInformation::getPeriodDuration() const
{
    if(parent)
        return parent->getPeriodDuration();
    else
        return 0;
}

void SegmentInformation::updateSegmentList(SegmentList *list, bool restamp)
{
    if(segmentList && restamp)
    {
        segmentList->updateWith(list, restamp);
        delete list;
    }
    else
    {
        delete segmentList;
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
        mediaSegmentTemplate->updateWith(templ);
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
            static_cast<Segment *>(segment)->addSubSegment(subsegment);
            break;
        }
    }
}

void SegmentInformation::SplitUsingIndex(std::vector<SplitPoint> &splitlist)
{
    SegmentBase *segmentBase = inheritSegmentBase();
    if(!segmentBase)
        return;

    std::vector<ISegment *> seglist;
    seglist.push_back( segmentBase );

    size_t prevstart = 0;
    stime_t prevtime = 0;

    SplitPoint split = {0,0,0};
    std::vector<SplitPoint>::const_iterator splitIt;
    for(splitIt = splitlist.begin(); splitIt < splitlist.end(); ++splitIt)
    {
        split = *splitIt;
        if(splitIt != splitlist.begin())
        {
            /* do previous splitpoint */
            insertIntoSegment(seglist, prevstart, split.offset - 1, prevtime, split.duration);
        }
        prevstart = split.offset;
        prevtime = split.time;
    }

    if(splitlist.size() == 1)
    {
        insertIntoSegment(seglist, prevstart, 0, prevtime, split.duration);
    }
    else if(splitlist.size() > 1)
    {
        insertIntoSegment(seglist, prevstart, split.offset - 1, prevtime, split.duration);
    }
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

void SegmentInformation::setAvailabilityTimeOffset(vlc_tick_t t)
{
    availabilityTimeOffset = t;
}

void SegmentInformation::setAvailabilityTimeComplete(bool b)
{
    availabilityTimeComplete = b;
}

vlc_tick_t SegmentInformation::inheritAvailabilityTimeOffset() const
{
    for(const SegmentInformation *p = this; p; p = p->parent)
    {
        if(availabilityTimeOffset.isSet())
            return availabilityTimeOffset.value();
    }
    return getPlaylist()->getAvailabilityTimeOffset();
}

bool SegmentInformation::inheritAvailabilityTimeComplete() const
{
    for(const SegmentInformation *p = this; p; p = p->parent)
    {
        if(availabilityTimeComplete.isSet())
            return availabilityTimeComplete.value();
    }
    return getPlaylist()->getAvailabilityTimeComplete();
}
