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

const AbstractSegmentBaseType * SegmentInformation::inheritSegmentProfile() const
{
    const AbstractSegmentBaseType *profile = inheritSegmentTemplate();
    if(!profile)
        profile = inheritSegmentList();
    if(!profile)
        profile = inheritSegmentBase();
    return profile;
}


/* Returns wanted segment, or next in sequence if not found */
Segment *  SegmentInformation::getNextMediaSegment(uint64_t i_pos,uint64_t *pi_newpos,
                                                   bool *pb_gap) const
{
    const AbstractSegmentBaseType *profile = inheritSegmentProfile();
    if(!profile)
        return NULL;
    return profile->getNextMediaSegment(i_pos, pi_newpos, pb_gap);
}

InitSegment * SegmentInformation::getInitSegment() const
{
    const AbstractSegmentBaseType *profile = inheritSegmentProfile();
    if(!profile)
        return NULL;
    return profile->getInitSegment();
}

IndexSegment *SegmentInformation::getIndexSegment() const
{
    const AbstractSegmentBaseType *profile = inheritSegmentProfile();
    if(!profile)
        return NULL;
    return profile->getIndexSegment();
}

Segment * SegmentInformation::getMediaSegment(uint64_t pos) const
{
    const AbstractSegmentBaseType *profile = inheritSegmentProfile();
    if(!profile)
        return NULL;
    return profile->getMediaSegment(pos);
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

SegmentInformation * SegmentInformation::getParent() const
{
    return parent;
}

AbstractSegmentBaseType * SegmentInformation::getProfile() const
{
    if(mediaSegmentTemplate)
        return mediaSegmentTemplate;
    else if(segmentList)
        return segmentList;
    else if(segmentBase)
        return segmentBase;
    else
        return NULL;
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

void SegmentInformation::setSegmentTemplate(SegmentTemplate *templ)
{
    if(mediaSegmentTemplate)
    {
        mediaSegmentTemplate->updateWith(templ);
        delete templ;
    }
    else
        mediaSegmentTemplate = templ;
}

static void insertIntoSegment(std::vector<Segment *> &seglist, size_t start,
                              size_t end, stime_t time, stime_t duration)
{
    std::vector<Segment *>::iterator segIt;
    for(segIt = seglist.begin(); segIt < seglist.end(); ++segIt)
    {
        Segment *segment = *segIt;
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
    SegmentBase *segmentBase = inheritSegmentBase();
    if(!segmentBase)
        return;

    std::vector<Segment *> seglist;
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

SegmentTemplate * SegmentInformation::inheritSegmentTemplate() const
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
