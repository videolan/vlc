/*
 * SegmentTracker.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC authors
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

#include "SegmentTracker.hpp"
#include "playlist/AbstractPlaylist.hpp"
#include "playlist/BaseRepresentation.h"
#include "playlist/BaseAdaptationSet.h"
#include "playlist/Segment.h"
#include "playlist/SegmentChunk.hpp"
#include "logic/AbstractAdaptationLogic.h"
#include "logic/BufferingLogic.hpp"

#include <cassert>
#include <limits>

using namespace adaptive;
using namespace adaptive::logic;
using namespace adaptive::playlist;

SegmentTrackerEvent::SegmentTrackerEvent(SegmentChunk *s)
{
    type = DISCONTINUITY;
    u.discontinuity.sc = s;
}

SegmentTrackerEvent::SegmentTrackerEvent(BaseRepresentation *prev, BaseRepresentation *next)
{
    type = SWITCHING;
    u.switching.prev = prev;
    u.switching.next = next;
}

SegmentTrackerEvent::SegmentTrackerEvent(const StreamFormat *fmt)
{
    type = FORMATCHANGE;
    u.format.f = fmt;
}

SegmentTrackerEvent::SegmentTrackerEvent(const ID &id, bool enabled)
{
    type = BUFFERING_STATE;
    u.buffering.enabled = enabled;
    u.buffering.id = &id;
}

SegmentTrackerEvent::SegmentTrackerEvent(const ID &id, vlc_tick_t min, vlc_tick_t current, vlc_tick_t target)
{
    type = BUFFERING_LEVEL_CHANGE;
    u.buffering_level.minimum = min;
    u.buffering_level.current = current;
    u.buffering_level.target = target;
    u.buffering.id = &id;
}

SegmentTrackerEvent::SegmentTrackerEvent(const ID &id, vlc_tick_t duration)
{
    type = SEGMENT_CHANGE;
    u.segment.duration = duration;
    u.segment.id = &id;
}

SegmentTracker::SegmentTracker(SharedResources *res,
        AbstractAdaptationLogic *logic_,
        const AbstractBufferingLogic *bl,
        BaseAdaptationSet *adaptSet)
{
    resources = res;
    first = true;
    curNumber = next = std::numeric_limits<uint64_t>::max();
    initializing = true;
    index_sent = false;
    init_sent = false;
    curRepresentation = NULL;
    bufferingLogic = bl;
    setAdaptationLogic(logic_);
    adaptationSet = adaptSet;
    format = StreamFormat::UNKNOWN;
}

SegmentTracker::~SegmentTracker()
{
    reset();
}

void SegmentTracker::setAdaptationLogic(AbstractAdaptationLogic *logic_)
{
    logic = logic_;
    registerListener(logic);
}

StreamFormat SegmentTracker::getCurrentFormat() const
{
    BaseRepresentation *rep = curRepresentation;
    if(!rep)
        rep = logic->getNextRepresentation(adaptationSet, NULL);
    if(rep)
    {
        /* Ensure ephemere content is updated/loaded */
        if(rep->needsUpdate(next))
            (void) rep->runLocalUpdates(resources);
        return rep->getStreamFormat();
    }
    return StreamFormat();
}

std::list<std::string> SegmentTracker::getCurrentCodecs() const
{
    BaseRepresentation *rep = curRepresentation;
    if(!rep)
        rep = logic->getNextRepresentation(adaptationSet, NULL);
    if(rep)
        return rep->getCodecs();
    return std::list<std::string>();
}

const std::string & SegmentTracker::getStreamDescription() const
{
    return adaptationSet->description.Get();
}

const std::string & SegmentTracker::getStreamLanguage() const
{
    return adaptationSet->getLang();
}

const Role & SegmentTracker::getStreamRole() const
{
    return adaptationSet->getRole();
}

void SegmentTracker::reset()
{
    notify(SegmentTrackerEvent(curRepresentation, NULL));
    curRepresentation = NULL;
    init_sent = false;
    index_sent = false;
    initializing = true;
    format = StreamFormat::UNKNOWN;
}

SegmentChunk * SegmentTracker::getNextChunk(bool switch_allowed,
                                            AbstractConnectionManager *connManager)
{
    BaseRepresentation *rep = NULL, *prevRep = NULL;
    ISegment *segment;

    if(!adaptationSet)
        return NULL;

    /* Ensure we don't keep chaining init/index without data */
    if( initializing )
    {
        if( curRepresentation )
            switch_allowed = false;
        else
            switch_allowed = true;
    }

    if( !switch_allowed ||
       (curRepresentation && !curRepresentation->getAdaptationSet()->isSegmentAligned()) )
        rep = curRepresentation;
    else
        rep = logic->getNextRepresentation(adaptationSet, curRepresentation);

    if ( rep == NULL )
            return NULL;


    if(rep != curRepresentation)
    {
        notify(SegmentTrackerEvent(curRepresentation, rep));
        prevRep = curRepresentation;
        curRepresentation = rep;
        init_sent = false;
        index_sent = false;
        initializing = true;
    }

    bool b_updated = false;
    /* Ensure ephemere content is updated/loaded */
    if(rep->needsUpdate(next))
        b_updated = rep->runLocalUpdates(resources);

    if(curNumber == std::numeric_limits<uint64_t>::max())
    {
        next = bufferingLogic->getStartSegmentNumber(rep);
        if(next == std::numeric_limits<uint64_t>::max())
            return NULL;
    }
    else if(prevRep && !rep->consistentSegmentNumber())
    {
        /* Convert our segment number */
        next = rep->translateSegmentNumber(next, prevRep);
    }

    curRepresentation->scheduleNextUpdate(next, b_updated);

    if(rep->getStreamFormat() != format)
    {
        /* Initial format ? */
        if(format == StreamFormat(StreamFormat::UNKNOWN))
        {
            format = rep->getStreamFormat();
        }
        else
        {
            format = rep->getStreamFormat();
            notify(SegmentTrackerEvent(&format)); /* Notify new demux format */
            return NULL; /* Force current demux to end */
        }
    }
    else if(format == StreamFormat(StreamFormat::UNKNOWN) && prevRep && prevRep != rep)
    {
        /* Handle the corner case when only the demuxer can know the format and
         * demuxer starts after the format change (Probe != buffering) */
        notify(SegmentTrackerEvent(&format)); /* Notify new demux format */
        return NULL; /* Force current demux to end */
    }

    if(format == StreamFormat(StreamFormat::UNSUPPORTED))
    {
        return NULL; /* Can't return chunk because no demux will be created */
    }

    if(!init_sent)
    {
        init_sent = true;
        segment = rep->getSegment(BaseRepresentation::INFOTYPE_INIT);
        if(segment)
            return segment->toChunk(resources, connManager, next, rep);
    }

    if(!index_sent)
    {
        index_sent = true;
        segment = rep->getSegment(BaseRepresentation::INFOTYPE_INDEX);
        if(segment)
            return segment->toChunk(resources, connManager, next, rep);
    }

    bool b_gap = false;
    segment = rep->getNextSegment(BaseRepresentation::INFOTYPE_MEDIA, next, &next, &b_gap);
    if(!segment)
    {
        return NULL;
    }

    if(initializing)
    {
        b_gap = false;
        /* stop initializing after 1st chunk */
        initializing = false;
    }

    SegmentChunk *chunk = segment->toChunk(resources, connManager, next, rep);

    /* Notify new segment length for stats / logic */
    if(chunk)
    {
        const Timescale timescale = rep->inheritTimescale();
        notify(SegmentTrackerEvent(rep->getAdaptationSet()->getID(),
                                   timescale.ToTime(segment->duration.Get())));
    }

    /* We need to check segment/chunk format changes, as we can't rely on representation's (HLS)*/
    if(chunk && format != chunk->getStreamFormat())
    {
        format = chunk->getStreamFormat();
        notify(SegmentTrackerEvent(&format));
    }

    /* Handle both implicit and explicit discontinuities */
    if( (b_gap && next) || (chunk && chunk->discontinuity) )
    {
        notify(SegmentTrackerEvent(chunk));
    }

    if(chunk)
    {
        curNumber = next;
        next++;
    }

    return chunk;
}

bool SegmentTracker::setPositionByTime(vlc_tick_t time, bool restarted, bool tryonly)
{
    uint64_t segnumber;
    BaseRepresentation *rep = curRepresentation;
    if(!rep)
        rep = logic->getNextRepresentation(adaptationSet, NULL);

    /* Stream might not have been loaded at all (HLS) or expired */
    if(rep && rep->needsUpdate(next) && !rep->runLocalUpdates(resources))
    {
        msg_Err(rep->getAdaptationSet()->getPlaylist()->getVLCObject(),
                "Failed to update Representation %s", rep->getID().str().c_str());
        return false;
    }

    if(rep &&
       rep->getSegmentNumberByTime(time, &segnumber))
    {
        if(!tryonly)
            setPositionByNumber(segnumber, restarted);
        return true;
    }
    return false;
}

void SegmentTracker::setPositionByNumber(uint64_t segnumber, bool restarted)
{
    if(restarted)
    {
        initializing = true;
        index_sent = false;
        init_sent = false;
    }
    curNumber = next = segnumber;
}

vlc_tick_t SegmentTracker::getPlaybackTime(bool b_next) const
{
    vlc_tick_t time, duration;

    BaseRepresentation *rep = curRepresentation;
    if(!rep)
        rep = logic->getNextRepresentation(adaptationSet, NULL);

    if(rep &&
       rep->getPlaybackTimeDurationBySegmentNumber(b_next ? next : curNumber, &time, &duration))
    {
        return time;
    }
    return 0;
}

bool SegmentTracker::getMediaPlaybackRange(vlc_tick_t *start, vlc_tick_t *end,
                                           vlc_tick_t *length) const
{
    if(!curRepresentation)
        return false;
    return curRepresentation->getMediaPlaybackRange(start, end, length);
}

vlc_tick_t SegmentTracker::getMinAheadTime() const
{
    BaseRepresentation *rep = curRepresentation;
    if(!rep)
        rep = logic->getNextRepresentation(adaptationSet, NULL);
    if(rep)
    {
        /* Ensure ephemere content is updated/loaded */
        if(rep->needsUpdate(next))
            (void) rep->runLocalUpdates(resources);

        uint64_t startnumber = curNumber;
        if(startnumber == std::numeric_limits<uint64_t>::max())
            startnumber = bufferingLogic->getStartSegmentNumber(rep);
        if(startnumber != std::numeric_limits<uint64_t>::max())
            return rep->getMinAheadTime(startnumber);
    }
    return 0;
}

void SegmentTracker::notifyBufferingState(bool enabled) const
{
    notify(SegmentTrackerEvent(adaptationSet->getID(), enabled));
}

void SegmentTracker::notifyBufferingLevel(vlc_tick_t min, vlc_tick_t current, vlc_tick_t target) const
{
    notify(SegmentTrackerEvent(adaptationSet->getID(), min, current, target));
}

void SegmentTracker::registerListener(SegmentTrackerListenerInterface *listener)
{
    listeners.push_back(listener);
}

bool SegmentTracker::bufferingAvailable() const
{
    if(adaptationSet->getPlaylist()->isLive())
        return getMinAheadTime() > 0;
    return true;
}

void SegmentTracker::updateSelected()
{
    if(curRepresentation && curRepresentation->needsUpdate(next))
    {
        bool b_updated = curRepresentation->runLocalUpdates(resources);
        curRepresentation->scheduleNextUpdate(curNumber, b_updated);
    }
}

void SegmentTracker::notify(const SegmentTrackerEvent &event) const
{
    std::list<SegmentTrackerListenerInterface *>::const_iterator it;
    for(it=listeners.begin();it != listeners.end(); ++it)
        (*it)->trackerEvent(event);
}
