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

SegmentChangedEvent::SegmentChangedEvent(const ID &id, vlc_tick_t starttime,
                                         vlc_tick_t duration, vlc_tick_t displaytime)
    : TrackerEvent(Type::SegmentChange)
{
    this->id = &id;
    this->duration = duration;
    this->starttime = starttime;
    this->displaytime = displaytime;
}

BufferingStateUpdatedEvent::BufferingStateUpdatedEvent(const ID &id, bool enabled)
    : TrackerEvent(Type::BufferingStateUpdate)
{
    this->id = &id;
    this->enabled = enabled;
}

BufferingLevelChangedEvent::BufferingLevelChangedEvent(const ID &id,
                                                       vlc_tick_t minimum, vlc_tick_t maximum,
                                                       vlc_tick_t current, vlc_tick_t target)
    : TrackerEvent(Type::BufferingLevelChange)
{
    this->id = &id;
    this->minimum = minimum;
    this->maximum = maximum;
    this->current = current;
    this->target = target;
}

PositionChangedEvent::PositionChangedEvent()
    : TrackerEvent(Type::PositionChange)
{

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

void SegmentTracker::getCodecsDesc(CodecDescriptionList *descs) const
{
    BaseRepresentation *rep = current.rep;
    if(!rep)
        rep = logic->getNextRepresentation(adaptationSet, nullptr);
    if(rep)
        rep->getCodecsDesc(descs);
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
    resetChunksSequence();
    initializing = true;
    format = StreamFormat::UNKNOWN;
}

SegmentTracker::ChunkEntry::ChunkEntry()
{
    chunk = nullptr;
}

SegmentTracker::ChunkEntry::ChunkEntry(SegmentChunk *c, Position p, vlc_tick_t s, vlc_tick_t d, vlc_tick_t dt)
{
    chunk = c;
    pos = p;
    duration = d;
    starttime = s;
    displaytime = dt;
}

bool SegmentTracker::ChunkEntry::isValid() const
{
    return chunk && pos.isValid();
}

SegmentTracker::ChunkEntry
SegmentTracker::prepareChunk(bool switch_allowed, Position pos,
                             AbstractConnectionManager *connManager) const
{
    if(!adaptationSet)
        return ChunkEntry();

    bool b_updated = false;

    /* starting */
    if(!pos.isValid())
    {
        pos = getStartPosition();
        if(!pos.isValid())
            return ChunkEntry();
    }
    else /* continuing, or seek */
    {
        if(!adaptationSet->isSegmentAligned() || !pos.init_sent || !pos.index_sent)
            switch_allowed = false;

        if(switch_allowed)
        {
            Position temp;
            temp.rep = logic->getNextRepresentation(adaptationSet, pos.rep);
            if(temp.rep && temp.rep != pos.rep)
            {
                /* Ensure ephemere content is updated/loaded */
                if(temp.rep->needsUpdate(pos.number))
                    b_updated = temp.rep->runLocalUpdates(resources);
                /* if we need to translate pos */
                if(!temp.rep->consistentSegmentNumber())
                {
                    /* Convert our segment number */
                    temp.number = temp.rep->translateSegmentNumber(pos.number, pos.rep);
                }
                else temp.number = pos.number;
            }
            if(temp.isValid())
                pos = temp;
        }
    }

    ISegment *segment = nullptr;

    pos.rep->scheduleNextUpdate(pos.number, b_updated);

    if(!pos.init_sent)
    {
        segment = pos.rep->getInitSegment();
        if(!segment)
            ++pos;
    }

    if(!segment && !pos.index_sent)
    {
        if(pos.rep->needsIndex())
            segment = pos.rep->getIndexSegment();
        if(!segment)
            ++pos;
    }

    bool b_gap = true;
    if(!segment)
        segment = pos.rep->getNextMediaSegment(pos.number, &pos.number, &b_gap);

    if(!segment)
        return ChunkEntry();

    SegmentChunk *segmentChunk = segment->toChunk(resources, connManager, pos.number, pos.rep);
    if(!segmentChunk)
        return ChunkEntry();

    const Timescale timescale = pos.rep->inheritTimescale();
    return ChunkEntry(segmentChunk, pos, timescale.ToTime(segment->startTime.Get()),
                      timescale.ToTime(segment->duration.Get()), segment->getDisplayTime());
}

void SegmentTracker::resetChunksSequence()
{
    while(!chunkssequence.empty())
    {
        delete chunkssequence.front().chunk;
        chunkssequence.pop_front();
    }
}

ChunkInterface * SegmentTracker::getNextChunk(bool switch_allowed,
                                            AbstractConnectionManager *connManager)
{
    if(!adaptationSet || !next.isValid())
        return nullptr;

    if(chunkssequence.empty())
    {
        ChunkEntry chunk = prepareChunk(switch_allowed, next, connManager);
        chunkssequence.push_back(chunk);
    }

    ChunkEntry chunk = chunkssequence.front();
    if(!chunk.isValid())
    {
        chunkssequence.pop_front();
        delete chunk.chunk;
        return nullptr;
    }

    /* here next == wanted chunk pos */
    bool b_gap = (next.number != chunk.pos.number);
    const bool b_switched = (next.rep != chunk.pos.rep);
    const bool b_discontinuity = chunk.chunk->discontinuity;

    if(b_switched)
    {
        notify(RepresentationSwitchEvent(next.rep, chunk.pos.rep));
        initializing = true;
    }

    /* advance or don't trigger duplicate events */
    next = current = chunk.pos;

    /* From this point chunk must be returned */
    ChunkInterface *returnedChunk;
    StreamFormat chunkformat = chunk.chunk->getStreamFormat();

    /* Wrap and probe format */
    if(chunkformat == StreamFormat(StreamFormat::UNKNOWN))
    {
        ProbeableChunk *wrappedck = new ProbeableChunk(chunk.chunk);
        const uint8_t *p_peek;
        size_t i_peek = wrappedck->peek(&p_peek);
        chunkformat = StreamFormat(p_peek, i_peek);
        /* fallback on Mime type */
        if(chunkformat == StreamFormat(StreamFormat::UNKNOWN))
            format = StreamFormat(chunk.chunk->getContentType());
        chunk.chunk->setStreamFormat(chunkformat);
        returnedChunk = wrappedck;
    }
    else returnedChunk = chunk.chunk;

    if(chunkformat != format &&
       chunkformat != StreamFormat(StreamFormat::UNKNOWN))
    {
        format = chunkformat;
        notify(FormatChangedEvent(&format));
    }

    /* pop position and return our chunk */
    chunkssequence.pop_front();
    chunk.chunk = nullptr;

    if(initializing)
    {
        b_gap = false;
        /* stop initializing after 1st chunk */
        initializing = false;
    }

    /* Notify new segment length for stats / logic */
    if(chunk.pos.init_sent && chunk.pos.index_sent)
        notify(SegmentChangedEvent(adaptationSet->getID(), chunk.starttime, chunk.duration, chunk.displaytime));

    /* Handle both implicit and explicit discontinuities */
    if(b_gap || b_discontinuity)
        notify(DiscontinuityEvent());

    if(!b_gap)
        ++next;

    return returnedChunk;
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
    resetChunksSequence();
    notify(PositionChangedEvent());
}

SegmentTracker::Position SegmentTracker::getStartPosition() const
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

void SegmentTracker::notifyBufferingLevel(vlc_tick_t min, vlc_tick_t max,
                                          vlc_tick_t current, vlc_tick_t target) const
{
    notify(BufferingLevelChangedEvent(adaptationSet->getID(), min, max, current, target));
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
