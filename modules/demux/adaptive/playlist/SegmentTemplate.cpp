/*****************************************************************************
 * SegmentTemplate.cpp: Implement the UrlTemplate element.
 *****************************************************************************
 * Copyright (C) 1998-2007 VLC authors and VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#include "SegmentTemplate.h"
#include "SegmentTimeline.h"
#include "SegmentInformation.hpp"
#include "BasePlaylist.hpp"
#include <limits>

using namespace adaptive::playlist;

SegmentTemplateSegment::SegmentTemplateSegment( ICanonicalUrl *parent ) :
    Segment( parent )
{
    debugName = "SegmentTemplateSegment";
    templated = true;
    templ = nullptr;
}

SegmentTemplateSegment::~SegmentTemplateSegment()
{

}

void SegmentTemplateSegment::setSourceUrl(const std::string &url)
{
    sourceUrl = Url(Url::Component(url, templ));
}

void SegmentTemplateSegment::setParentTemplate( SegmentTemplate *templ_ )
{
    templ = templ_;
}

SegmentTemplate::SegmentTemplate( SegmentTemplateSegment *seg, SegmentInformation *parent ) :
    AbstractMultipleSegmentBaseType( parent, AbstractAttr::Type::SegmentTemplate )
{
    initialisationSegment.Set( nullptr );
    parentSegmentInformation = parent;
    virtualsegment = seg;
    virtualsegment->setParent( parentSegmentInformation );
    virtualsegment->setParentTemplate( this );
}

SegmentTemplate::~SegmentTemplate()
{
    delete virtualsegment;
}

void SegmentTemplate::setSourceUrl( const std::string &url )
{
    virtualsegment->setSourceUrl(url);
}

void SegmentTemplate::pruneByPlaybackTime(vlc_tick_t time)
{
    AbstractAttr *p = getAttribute(Type::Timeline);
    if(p)
        return static_cast<SegmentTimeline *> (p)->pruneByPlaybackTime(time);
}

size_t SegmentTemplate::pruneBySequenceNumber(uint64_t number)
{
    AbstractAttr *p = getAttribute(Type::Timeline);
    if(p)
        return static_cast<SegmentTimeline *> (p)->pruneBySequenceNumber(number);
    return 0;
}

uint64_t SegmentTemplate::getLiveTemplateNumber(vlc_tick_t playbacktime, bool abs) const
{
    uint64_t number = inheritStartNumber();
    /* live streams / templated */
    const stime_t dur = inheritDuration();
    if(dur)
    {
        /* compute, based on current time */
        /* N = (T - AST - PS - D)/D + sSN */
        const Timescale timescale = inheritTimescale();
        if(abs)
        {
            vlc_tick_t streamstart =
                    parentSegmentInformation->getPlaylist()->availabilityStartTime.Get();
            streamstart += parentSegmentInformation->getPeriodStart();
            playbacktime -= streamstart;
        }
        stime_t elapsed = timescale.ToScaled(playbacktime) - dur;
        if(elapsed > 0)
            number += elapsed / dur;
    }

    return number;
}

void SegmentTemplate::debug(vlc_object_t *obj, int indent) const
{
    AbstractSegmentBaseType::debug(obj, indent);
    if(virtualsegment)
        virtualsegment->debug(obj, indent);
    const AbstractAttr *p = getAttribute(Type::Timeline);
    if(p)
        static_cast<const SegmentTimeline *> (p)->debug(obj, indent + 1);
}

vlc_tick_t SegmentTemplate::getMinAheadTime(uint64_t number) const
{
    SegmentTimeline *timeline = inheritSegmentTimeline();
    if( timeline )
    {
        const Timescale timescale = timeline->inheritTimescale();
        return timescale.ToTime(timeline->getMinAheadScaledTime(number));
    }
    else
    {
        const Timescale timescale = inheritTimescale();
        uint64_t current = getLiveTemplateNumber(vlc_tick_from_sec(time(nullptr)));
        stime_t i_length = (current - number) * inheritDuration();
        return timescale.ToTime(i_length);
    }
}

Segment * SegmentTemplate::getMediaSegment(uint64_t number) const
{
    const SegmentTimeline *tl = inheritSegmentTimeline();
    if(tl == nullptr || (tl->maxElementNumber() >= number && tl->minElementNumber() <= number))
        return virtualsegment;
    return nullptr;
}

InitSegment * SegmentTemplate::getInitSegment() const
{
    return initialisationSegment.Get();
}

Segment *  SegmentTemplate::getNextMediaSegment(uint64_t i_pos,uint64_t *pi_newpos,
                                                     bool *pb_gap) const
{
    *pb_gap = false;
    *pi_newpos = i_pos;
    /* Check if we don't exceed timeline */
    const SegmentTimeline *timeline = inheritSegmentTimeline();
    if(timeline)
    {
        *pi_newpos = std::max(timeline->minElementNumber(), i_pos);
        if (timeline->maxElementNumber() < i_pos)
            return nullptr;
    }
    else
    {
        /* check template upper bound */
        const BasePlaylist *playlist = parentSegmentInformation->getPlaylist();
        if(!playlist->isLive())
        {
            const Timescale timescale = inheritTimescale();
            const stime_t segmentduration = inheritDuration();
            vlc_tick_t totalduration = parentSegmentInformation->getPeriodDuration();
            if(totalduration == 0)
                totalduration = playlist->duration.Get();
            if(totalduration && segmentduration)
            {
                uint64_t endnum = inheritStartNumber() +
                        (timescale.ToScaled(totalduration) + segmentduration - 1) / segmentduration;
                if(i_pos >= endnum)
                {
                    *pi_newpos = i_pos;
                    return nullptr;
                }
            }
        }
        *pi_newpos = i_pos;
        /* start number */
        *pi_newpos = std::max(inheritStartNumber(), i_pos);
    }
    return virtualsegment;
}

uint64_t SegmentTemplate::getStartSegmentNumber() const
{
    const SegmentTimeline *timeline = inheritSegmentTimeline();
    return timeline ? timeline->minElementNumber() : inheritStartNumber();
}

bool SegmentTemplate::getSegmentNumberByTime(vlc_tick_t time, uint64_t *ret) const
{
    const SegmentTimeline *timeline = inheritSegmentTimeline();
    if(timeline)
    {
        const Timescale timescale = timeline->inheritTimescale();
        stime_t st = timescale.ToScaled(time);
        *ret = timeline->getElementNumberByScaledPlaybackTime(st);
        return true;
    }

    const stime_t duration = inheritDuration();
    if( duration && parent )
    {
        BasePlaylist *playlist = parent->getPlaylist();
        if( playlist->isLive() )
        {
            vlc_tick_t now = vlc_tick_from_sec(::time(nullptr));
            if(playlist->availabilityStartTime.Get())
            {
                if(time >= playlist->availabilityStartTime.Get() && time < now)
                    *ret = getLiveTemplateNumber(time, true);
                else if(now - playlist->availabilityStartTime.Get() > time)
                    *ret = getLiveTemplateNumber(time, false);
            }
            else return false;
        }
        else
        {
            const Timescale timescale = inheritTimescale();
            *ret = inheritStartNumber();
            *ret += timescale.ToScaled(time) / duration;
        }
        return true;
    }

    return false;
}


bool SegmentTemplate::getPlaybackTimeDurationBySegmentNumber(uint64_t number,
                                                             vlc_tick_t *time,
                                                             vlc_tick_t *duration) const
{
    if(number == std::numeric_limits<uint64_t>::max())
        return false;

    Timescale timescale;
    stime_t stime, sduration;

    const SegmentTimeline * timeline = inheritSegmentTimeline();
    if(timeline)
    {
        timescale = timeline->inheritTimescale();
        if(!timeline->getScaledPlaybackTimeDurationBySegmentNumber(number, &stime, &sduration))
            return false;
    }
    else
    {
        timescale = inheritTimescale();
        uint64_t startNumber = inheritStartNumber();
        if(number < startNumber)
            return false;
        sduration = inheritDuration();
        stime = (number - startNumber) * sduration;
    }

    *time = timescale.ToTime(stime);
    *duration = timescale.ToTime(sduration);
    return true;

}

SegmentTemplateInit::SegmentTemplateInit( SegmentTemplate *templ_,
                                          ICanonicalUrl *parent ) :
    InitSegment(parent)
{
    debugName = "InitSegmentTemplate";
    templ = templ_;
}

SegmentTemplateInit::~SegmentTemplateInit()
{

}

void SegmentTemplateInit::setSourceUrl(const std::string &url)
{
    sourceUrl = Url(Url::Component(url, templ));
}
