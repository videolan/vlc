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
#include "playlist/BasePlaylist.hpp"
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

TrackerEvent::TrackerEvent(Type t)
{
    type = t;
}

TrackerEvent::~TrackerEvent()
{

}

TrackerEvent::Type TrackerEvent::getType() const
{
    return type;
}

DiscontinuityEvent::DiscontinuityEvent()
    : TrackerEvent(Type::Discontinuity)
{

}

RepresentationSwitchEvent::RepresentationSwitchEvent(BaseRepresentation *prev,
                                                     BaseRepresentation *next)
    : TrackerEvent(Type::RepresentationSwitch)
{
    this->prev = prev;
    this->next = next;
}

FormatChangedEvent::FormatChangedEvent(const StreamFormat *f)
    : TrackerEvent(Type::FormatChange)
{
    this->format = f;
}

SegmentChangedEvent::SegmentChangedEvent(const ID &id, vlc_tick_t duration)
    : TrackerEvent(Type::SegmentChange)
{
    this->id = &id;
    this->duration = duration;
}

BufferingStateUpdatedEvent::BufferingStateUpdatedEvent(const ID &id, bool enabled)
    : TrackerEvent(Type::BufferingStateUpdate)
{
    this->id = &id;
    this->enabled = enabled;
}

BufferingLevelChangedEvent::BufferingLevelChangedEvent(const ID &id, vlc_tick_t minimum,
                                                       vlc_tick_t current, vlc_tick_t target)
    : TrackerEvent(Type::BufferingLevelChange)
{
    this->id = &id;
    this->minimum = minimum;
    this->current = current;
    this->target = target;
}

SegmentTracker::SegmentTracker(SharedResources *res,
        AbstractAdaptationLogic *logic_,
        const AbstractBufferingLogic *bl,
        BaseAdaptationSet *adaptSet)
{
    resources = res;
    first = true;
    initializing = true;
    bufferingLogic = bl;
    setAdaptationLogic(logic_);
    adaptationSet = adaptSet;
    format = StreamFormat::UNKNOWN;
}

SegmentTracker::~SegmentTracker()
{
    reset();
}

SegmentTracker::Position::Position()
{
    number = std::numeric_limits<uint64_t>::max();
    rep = nullptr;
    init_sent = false;
    index_sent = false;
}

SegmentTracker::Position::Position(BaseRepresentation *rep, uint64_t number)
{
    this->rep = rep;
    this->number = number;
    init_sent = false;
    index_sent = false;
}

bool SegmentTracker::Position::isValid() const
{
    return number != std::numeric_limits<uint64_t>::max() &&
           rep != nullptr;
}

std::string SegmentTracker::Position::toString() const
{
    std::stringstream ss;
    ss.imbue(std::locale("C"));
    if(isValid())
        ss << "seg# " << number
           << " " << init_sent
           << ":" << index_sent
           << " " << rep->getID().str();
    else
        ss << "invalid";
    return ss.str();
}

SegmentTracker::Position & SegmentTracker::Position::operator ++()
{
    if(isValid())
    {
        if(index_sent)
            ++number;
        else if(init_sent)
            index_sent = true;
        else
            init_sent = true;
        return *this;
    }
    return *this;
}

void SegmentTracker::setAdaptationLogic(AbstractAdaptationLogic *logic_)
{
    logic = logic_;
    registerListener(logic);
}

StreamFormat SegmentTracker::getCurrentFormat() const
{
    BaseRepresentation *rep = current.rep;
    if(!rep)
        rep = logic->getNextRepresentation(adaptationSet, nullptr);
    if(rep)
    {
        /* Ensure ephemere content is updated/loaded */
        if(rep->needsUpdate(next.number))
            (void) rep->runLocalUpdates(resources);
        return rep->getStreamFormat();
    }
    return StreamFormat();
}

std::list<std::string> SegmentTracker::getCurrentCodecs() const
{
    BaseRepresentation *rep = current.rep;
    if(!rep)
        rep = logic->getNextRepresentation(adaptationSet, nullptr);
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
    notify(RepresentationSwitchEvent(current.rep, nullptr));
    current = Position();
    next = Position();
    initializing = true;
    format = StreamFormat::UNKNOWN;
}

SegmentChunk * SegmentTracker::getNextChunk(bool switch_allowed,
                                            AbstractConnectionManager *connManager)
{
    ISegment *segment;

    if(!adaptationSet)
        return nullptr;

    bool b_updated = false;
    bool b_switched = false;

    /* starting */
    if(!next.isValid())
    {
        next = getStartPosition();
        b_switched = true;
    }
    else /* continuing, or seek */
    {
        if(!current.isValid() || !adaptationSet->isSegmentAligned() || initializing)
            switch_allowed = false;

        if(switch_allowed)
        {
            Position temp;
            temp.rep = logic->getNextRepresentation(adaptationSet, next.rep);
            if(temp.rep && temp.rep != next.rep)
            {
                /* Ensure ephemere content is updated/loaded */
                if(temp.rep->needsUpdate(next.number))
                    b_updated = temp.rep->runLocalUpdates(resources);
                /* if we need to translate pos */
                if(!temp.rep->consistentSegmentNumber())
                {
                    /* Convert our segment number */
                    temp.number = temp.rep->translateSegmentNumber(next.number, next.rep);
                }
                else temp.number = next.number;
            }
            if(temp.isValid())
            {
                next = temp;
                b_switched = current.isValid();
            }
        }
    }

    if(!next.isValid())
        return nullptr;

    if(b_switched)
    {
        notify(RepresentationSwitchEvent(current.rep, next.rep));
        initializing = true;
        assert(!next.index_sent);
        assert(!next.init_sent);
    }

    next.rep->scheduleNextUpdate(next.number, b_updated);
    current = next;

    if(current.rep->getStreamFormat() != format)
    {
        /* Initial format ? */
        if(format == StreamFormat(StreamFormat::UNKNOWN))
        {
            format = current.rep->getStreamFormat();
        }
        else
        {
            format = current.rep->getStreamFormat();
            notify(FormatChangedEvent(&format)); /* Notify new demux format */
            return nullptr; /* Force current demux to end */
        }
    }
    else if(format == StreamFormat(StreamFormat::UNKNOWN) && b_switched)
    {
        /* Handle the corner case when only the demuxer can know the format and
         * demuxer starts after the format change (Probe != buffering) */
        notify(FormatChangedEvent(&format)); /* Notify new demux format */
        return nullptr; /* Force current demux to end */
    }

    if(format == StreamFormat(StreamFormat::UNSUPPORTED))
    {
        return nullptr; /* Can't return chunk because no demux will be created */
    }

    if(!current.init_sent)
    {
        ++next;
        segment = current.rep->getInitSegment();
        if(segment)
            return segment->toChunk(resources, connManager, current.number, current.rep);
        current = next;
    }

    if(!current.index_sent)
    {
        ++next;
        if(current.rep->needsIndex())
        {
            segment = current.rep->getIndexSegment();
            if(segment)
                return segment->toChunk(resources, connManager, current.number, current.rep);
        }
        current = next;
    }

    bool b_gap = false;
    segment = current.rep->getNextMediaSegment(current.number, &current.number, &b_gap);
    if(!segment)
        return nullptr;
    if(b_gap)
        next = current;

    if(initializing)
    {
        b_gap = false;
        /* stop initializing after 1st chunk */
        initializing = false;
    }

    SegmentChunk *chunk = segment->toChunk(resources, connManager, next.number, next.rep);

    /* Notify new segment length for stats / logic */
    if(chunk)
    {
        const Timescale timescale = next.rep->inheritTimescale();
        notify(SegmentChangedEvent(next.rep->getAdaptationSet()->getID(),
                                   timescale.ToTime(segment->duration.Get())));
    }

    /* We need to check segment/chunk format changes, as we can't rely on representation's (HLS)*/
    if(chunk && format != chunk->getStreamFormat())
    {
        format = chunk->getStreamFormat();
        notify(FormatChangedEvent(&format));
    }

    /* Handle both implicit and explicit discontinuities */
    if( (b_gap && next.number) || (chunk && chunk->discontinuity) )
    {
        notify(DiscontinuityEvent());
    }

    if(chunk)
        ++next;

    return chunk;
}

bool SegmentTracker::setPositionByTime(vlc_tick_t time, bool restarted, bool tryonly)
{
    Position pos = Position(current.rep, current.number);
    if(!pos.isValid())
        pos.rep = logic->getNextRepresentation(adaptationSet, nullptr);

    if(!pos.rep)
        return false;

    /* Stream might not have been loaded at all (HLS) or expired */
    if(pos.rep->needsUpdate(pos.number) && !pos.rep->runLocalUpdates(resources))
    {
        msg_Err(adaptationSet->getPlaylist()->getVLCObject(),
                "Failed to update Representation %s",
                pos.rep->getID().str().c_str());
        return false;
    }

    if(pos.rep->getSegmentNumberByTime(time, &pos.number))
    {
        if(!tryonly)
            setPosition(pos, restarted);
        return true;
    }
    return false;
}

void SegmentTracker::setPosition(const Position &pos, bool restarted)
{
    if(restarted)
        initializing = true;
    current = Position();
    next = pos;
}

SegmentTracker::Position SegmentTracker::getStartPosition()
{
    Position pos;
    pos.rep = logic->getNextRepresentation(adaptationSet, nullptr);
    if(pos.rep)
    {
        /* Ensure ephemere content is updated/loaded */
        if(pos.rep->needsUpdate(pos.number))
            pos.rep->runLocalUpdates(resources);
        pos.number = bufferingLogic->getStartSegmentNumber(pos.rep);
    }
    return pos;
}

bool SegmentTracker::setStartPosition()
{
    if(next.isValid())
        return true;

    Position pos = getStartPosition();
    if(!pos.isValid())
        return false;

    next = pos;
    return true;
}

vlc_tick_t SegmentTracker::getPlaybackTime(bool b_next) const
{
    vlc_tick_t time, duration;

    BaseRepresentation *rep = current.rep;
    if(!rep)
        rep = logic->getNextRepresentation(adaptationSet, nullptr);

    if(rep &&
       rep->getPlaybackTimeDurationBySegmentNumber(b_next ? next.number : current.number, &time, &duration))
    {
        return time;
    }
    return 0;
}

bool SegmentTracker::getMediaPlaybackRange(vlc_tick_t *start, vlc_tick_t *end,
                                           vlc_tick_t *length) const
{
    if(!current.rep)
        return false;
    return current.rep->getMediaPlaybackRange(start, end, length);
}

vlc_tick_t SegmentTracker::getMinAheadTime() const
{
    BaseRepresentation *rep = current.rep;
    if(!rep)
        rep = logic->getNextRepresentation(adaptationSet, nullptr);
    if(rep)
    {
        /* Ensure ephemere content is updated/loaded */
        if(rep->needsUpdate(next.number))
            (void) rep->runLocalUpdates(resources);

        uint64_t startnumber = current.number;
        if(startnumber == std::numeric_limits<uint64_t>::max())
            startnumber = bufferingLogic->getStartSegmentNumber(rep);
        if(startnumber != std::numeric_limits<uint64_t>::max())
            return rep->getMinAheadTime(startnumber);
    }
    return 0;
}

void SegmentTracker::notifyBufferingState(bool enabled) const
{
    notify(BufferingStateUpdatedEvent(adaptationSet->getID(), enabled));
}

void SegmentTracker::notifyBufferingLevel(vlc_tick_t min, vlc_tick_t current, vlc_tick_t target) const
{
    notify(BufferingLevelChangedEvent(adaptationSet->getID(), min, current, target));
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
    if(current.rep && current.rep->needsUpdate(next.number))
    {
        bool b_updated = current.rep->runLocalUpdates(resources);
        current.rep->scheduleNextUpdate(current.number, b_updated);
    }
}

void SegmentTracker::notify(const TrackerEvent &event) const
{
    std::list<SegmentTrackerListenerInterface *>::const_iterator it;
    for(it=listeners.begin();it != listeners.end(); ++it)
        (*it)->trackerEvent(event);
}
