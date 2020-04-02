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
#include "AbstractPlaylist.hpp"
#include <limits>

using namespace adaptive::playlist;

BaseSegmentTemplate::BaseSegmentTemplate( ICanonicalUrl *parent ) :
    Segment( parent )
{
}

BaseSegmentTemplate::~BaseSegmentTemplate()
{

}

void BaseSegmentTemplate::setSourceUrl(const std::string &url)
{
    sourceUrl = Url(Url::Component(url, this));
}

MediaSegmentTemplate::MediaSegmentTemplate( SegmentInformation *parent ) :
    BaseSegmentTemplate( parent ),
    TimescaleAble( NULL ) /* we don't want auto inherit */
{
    debugName = "SegmentTemplate";
    classId = Segment::CLASSID_SEGMENT;
    startNumber = std::numeric_limits<uint64_t>::max();
    segmentTimeline = NULL;
    initialisationSegment.Set( NULL );
    templated = true;
    parentSegmentInformation = parent;
}

MediaSegmentTemplate::~MediaSegmentTemplate()
{
    delete segmentTimeline;
}

void MediaSegmentTemplate::updateWith(MediaSegmentTemplate *updated)
{
    SegmentTimeline *timeline = segmentTimeline;
    if(timeline && updated->segmentTimeline)
    {
        timeline->updateWith(*updated->segmentTimeline);
        /*if(prunebarrier)
        {
            const Timescale timescale = timeline->inheritTimescale();
            const uint64_t number =
                    timeline->getElementNumberByScaledPlaybackTime(timescale.ToScaled(prunebarrier));
            timeline->pruneBySequenceNumber(number);
        }*/
    }
}

void MediaSegmentTemplate::pruneByPlaybackTime(vlc_tick_t time)
{
    if(segmentTimeline)
        return segmentTimeline->pruneByPlaybackTime(time);
}

size_t MediaSegmentTemplate::pruneBySequenceNumber(uint64_t number)
{
    if(segmentTimeline)
        return segmentTimeline->pruneBySequenceNumber(number);
    return 0;
}

uint64_t MediaSegmentTemplate::inheritStartNumber() const
{
    const SegmentInformation *ulevel = parentSegmentInformation ? parentSegmentInformation
                                                                : NULL;
    for( ; ulevel ; ulevel = ulevel->parent )
    {
        if( ulevel->mediaSegmentTemplate &&
            ulevel->mediaSegmentTemplate->startNumber !=
                std::numeric_limits<uint64_t>::max() )
            return ulevel->mediaSegmentTemplate->startNumber;
    }
    return 1;
}

Timescale MediaSegmentTemplate::inheritTimescale() const
{
    const SegmentInformation *ulevel = parentSegmentInformation ? parentSegmentInformation
                                                                : NULL;
    for( ; ulevel ; ulevel = ulevel->parent )
    {
        if( ulevel->mediaSegmentTemplate &&
            ulevel->mediaSegmentTemplate->getTimescale().isValid() )
            return ulevel->mediaSegmentTemplate->getTimescale();
        if( ulevel->getTimescale().isValid() )
            return ulevel->getTimescale();
    }
    return Timescale(1);
}

stime_t MediaSegmentTemplate::inheritDuration() const
{
    const SegmentInformation *ulevel = parentSegmentInformation ? parentSegmentInformation
                                                                : NULL;
    for( ; ulevel ; ulevel = ulevel->parent )
    {
        if( ulevel->mediaSegmentTemplate &&
            ulevel->mediaSegmentTemplate->duration.Get() > 0 )
            return ulevel->mediaSegmentTemplate->duration.Get();
    }
    return 0;
}

SegmentTimeline * MediaSegmentTemplate::inheritSegmentTimeline() const
{
    const SegmentInformation *ulevel = parentSegmentInformation ? parentSegmentInformation
                                                          : NULL;
    for( ; ulevel ; ulevel = ulevel->parent )
    {
        if( ulevel->mediaSegmentTemplate &&
            ulevel->mediaSegmentTemplate->segmentTimeline )
            return ulevel->mediaSegmentTemplate->segmentTimeline;
    }
    return NULL;
}

uint64_t MediaSegmentTemplate::getLiveTemplateNumber(vlc_tick_t playbacktime, bool abs) const
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

stime_t MediaSegmentTemplate::getMinAheadScaledTime(uint64_t number) const
{
    if( segmentTimeline )
        return segmentTimeline->getMinAheadScaledTime(number);

    uint64_t current = getLiveTemplateNumber(vlc_tick_from_sec(time(NULL)));
    return (current - number) * inheritDuration();
}

uint64_t MediaSegmentTemplate::getSequenceNumber() const
{
    return inheritStartNumber();
}

void MediaSegmentTemplate::setStartNumber( uint64_t v )
{
    startNumber = v;
}

void MediaSegmentTemplate::setSegmentTimeline( SegmentTimeline *v )
{
    delete segmentTimeline;
    segmentTimeline = v;
}

void MediaSegmentTemplate::debug(vlc_object_t *obj, int indent) const
{
    Segment::debug(obj, indent);
    if(segmentTimeline)
        segmentTimeline->debug(obj, indent + 1);
}

InitSegmentTemplate::InitSegmentTemplate( ICanonicalUrl *parent ) :
    BaseSegmentTemplate(parent)
{
    debugName = "InitSegmentTemplate";
    classId = InitSegment::CLASSID_INITSEGMENT;
}
