/*****************************************************************************
 *
 *****************************************************************************
 * Copyright (C) 2022 VideoLabs, VideoLAN and VLC Authors
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

#include "../SegmentTracker.hpp"
#include "../SharedResources.hpp"
#include "../logic/AbstractAdaptationLogic.h"
#include "../logic/BufferingLogic.hpp"
#include "../Time.hpp"
#include "../playlist/BasePlaylist.hpp"
#include "../playlist/BasePeriod.h"
#include "../playlist/BaseAdaptationSet.h"
#include "../playlist/BaseRepresentation.h"
#include "../../hls/playlist/HLSRepresentation.hpp"
#include "../playlist/SegmentList.h"
#include "../playlist/Segment.h"
#include "../http/HTTPConnectionManager.h"

#include "test.hpp"

#include <vlc_block.h>

#include <limits>
#include <vector>
#include <map>
#include <set>
#include <cassert>
#include <cstring>

using namespace adaptive;
using namespace adaptive::http;
using namespace hls::playlist;

class DummyLogic : public AbstractAdaptationLogic
{
    public:
        DummyLogic() : AbstractAdaptationLogic(nullptr), repindex(0) {}
        virtual ~DummyLogic() = default;
        virtual BaseRepresentation* getNextRepresentation(BaseAdaptationSet *set,
                                                          BaseRepresentation *) override
        {
            if(set->getRepresentations().size() <= repindex)
                return nullptr;
            return set->getRepresentations().at(repindex);
        }
        unsigned repindex;
};

class DummyChunkSource : public AbstractChunkSource
{
    public:
        DummyChunkSource(ChunkType t, const BytesRange &range, const std::vector<uint8_t> &v,
                         const std::string &content)
            : AbstractChunkSource(t, range), data(v), offset(0), contentType(content) {}
        virtual ~DummyChunkSource() = default;
        virtual void recycle() override { delete this; }
        virtual std::string getContentType  () const override
        {
            return contentType;
        }
        virtual RequestStatus getRequestStatus() const override
        {
            return data.size() ? RequestStatus::Success : RequestStatus::GenericError;
        }
        virtual block_t *   readBlock       ()  override
        {
            std::size_t remain = data.size() - offset;
            block_t *b = block_Alloc(remain);
            if(b)
                std::memcpy(b->p_buffer, &data[offset], remain);
            offset += remain;
            return b;
        }
        virtual block_t *   read            (size_t sz)  override
        {
            std::size_t remain = data.size() - offset;
            if(remain < sz)
                sz = remain;
            block_t *b = block_Alloc(sz);
            if(b)
                std::memcpy(b->p_buffer, &data[offset], sz);
            offset += sz;
            return b;
        }

        virtual bool        hasMoreData     () const  override { return offset < data.size(); }
        virtual size_t      getBytesRead    () const  override { return offset; }

    private:
        std::vector<uint8_t> data;
        std::size_t offset;
        std::string contentType;
};

class DummyConnectionManager : public AbstractConnectionManager
{
    public:
        DummyConnectionManager() : AbstractConnectionManager(nullptr) {}
        virtual ~DummyConnectionManager() = default;
        virtual void closeAllConnections () override {}
        virtual AbstractConnection * getConnection(ConnectionParams &) override { return nullptr; }
        virtual AbstractChunkSource *makeSource(const std::string &uri,
                                                const ID &, ChunkType t,
                                                const BytesRange &br) override
        {
            DummyChunkSource *d;
            auto it = data.find(uri);
            if(it == data.end())
                d = new DummyChunkSource(t, br, std::vector<uint8_t>(), uri);
            else
                d = new DummyChunkSource(t, br, it->second, uri);
            return d;
        }
        virtual void recycleSource(AbstractChunkSource *) override {}
        virtual void start(AbstractChunkSource *) override {}
        virtual void cancel(AbstractChunkSource *) override {}

        std::map<std::string, std::vector<uint8_t>> data;
};

using mapentry = std::pair<std::string, std::vector<uint8_t>>;

class SegmentTrackerListener : public SegmentTrackerListenerInterface
{
    public:
        SegmentTrackerListener() : SegmentTrackerListenerInterface()
        {
            reset();
        }
        virtual ~SegmentTrackerListener() = default;
        virtual void trackerEvent(const TrackerEvent &event) override
        {
            switch(event.getType())
            {
                case TrackerEvent::Type::SegmentChange:
                {
                    const SegmentChangedEvent *e =
                            static_cast<const SegmentChangedEvent *>(&event);
                    segmentchanged.displaytime = e->displaytime;
                    segmentchanged.duration = e->duration;
                    segmentchanged.sequence = e->sequence;
                    segmentchanged.starttime = e->starttime;
                }
                    break;
                case TrackerEvent::Type::FormatChange:
                {
                    const FormatChangedEvent *e =
                            static_cast<const FormatChangedEvent *>(&event);
                    formatchanged.format = *e->format;
                }
                    break;
                case TrackerEvent::Type::RepresentationSwitch:
                {
                    const RepresentationSwitchEvent *e =
                            static_cast<const RepresentationSwitchEvent *>(&event);
                    representationchanged.prev = e->prev;
                    representationchanged.next = e->next;
                }
                    break;
                case TrackerEvent::Type::Discontinuity:
                {
                    discontinuity = true;
                }
                    break;
                case TrackerEvent::Type::PositionChange:
                {
                    const PositionChangedEvent *e =
                            static_cast<const PositionChangedEvent *>(&event);
                    positionchanged.resumeTime = e->resumeTime;
                }
                    break;
                default:
                    return;
            }
            events.insert(event.getType());
        }

        bool occured(TrackerEvent::Type t) const
        {
            return events.find(t) != events.end();
        }

        void reset()
        {
            events.clear();
            segmentchanged.sequence = std::numeric_limits<uint64_t>::max() - 1;
            segmentchanged.displaytime = -1;
            segmentchanged.starttime = -1;
            segmentchanged.duration = -1;
            formatchanged.format = StreamFormat();
            representationchanged.prev = (BaseRepresentation *) 0xdeadbeef;
            representationchanged.next = (BaseRepresentation *) 0xdeadbeef;
            discontinuity = false;
            positionchanged.resumeTime = -1;
        }

        std::set<TrackerEvent::Type> events;
        struct
        {
            uint64_t sequence;
            vlc_tick_t displaytime;
            vlc_tick_t starttime;
            vlc_tick_t duration;
        } segmentchanged;
        struct
        {
            StreamFormat format;
        } formatchanged;
        struct
        {
            const BaseRepresentation *prev;
            const BaseRepresentation *next;
        } representationchanged;
        bool discontinuity;
        struct
        {
             vlc_tick_t resumeTime;
        } positionchanged;
};

class DummyRepresentation : public BaseRepresentation
{
    public:
        DummyRepresentation(BaseAdaptationSet *set) : BaseRepresentation(set) {}
        virtual ~DummyRepresentation() = default;
        virtual StreamFormat getStreamFormat() const override { return StreamFormat::Type::Unknown; }
};

class DummyHLSRepresentation : public HLSRepresentation
{
    public:
        DummyHLSRepresentation(BaseAdaptationSet *set, time_t duration) :
            HLSRepresentation(set) { targetDuration = duration; }
        virtual ~DummyHLSRepresentation() = default;
        virtual StreamFormat getStreamFormat() const override { return StreamFormat::Type::Unknown; }
        virtual bool needsUpdate(uint64_t) const override { return false; }
        virtual bool runLocalUpdates(SharedResources *) override { return false; }
};

static BaseAdaptationSet *CreatePlaylistPeriodAdaptationSet()
{
    BaseAdaptationSet *set = nullptr;
    BasePlaylist *pl= nullptr;
    try
    {
        pl = new BasePlaylist(nullptr);
        BasePeriod *period = new BasePeriod(pl);
        pl->addPeriod(period);
        set = new BaseAdaptationSet(period);
        period->addAdaptationSet(set);
    } catch( ... ) {
        delete pl;
    }
    return set;
}

static int SegmentTracker_check_formats(BaseAdaptationSet *adaptSet,
                                        DummyLogic *,
                                        SegmentTracker *tracker,
                                        SegmentTrackerListener &events)
{
    const stime_t START = 1337;
    Timescale timescale(100);

    ChunkInterface *currentChunk = nullptr;
    try
    {
        DummyRepresentation *rep0 = new DummyRepresentation(adaptSet);
        adaptSet->addRepresentation(rep0);
        rep0->setID(ID("0"));

        SegmentList *segmentList = nullptr;
        try
        {
            segmentList = new SegmentList(rep0);
            segmentList->addAttribute(new TimescaleAttr(timescale));
            for(int i=0; i<3; i++)
            {
                Segment *seg = new Segment(rep0);
                seg->setSequenceNumber(123 + i);
                seg->setDiscontinuitySequenceNumber(456);
                seg->startTime.Set(START + 100 * i);
                seg->duration.Set(100);
                seg->setSourceUrl(i < 2 ? "sample/aac" : "sample/ac3");
                segmentList->addSegment(seg);
            }
        } catch (...) {
            delete segmentList;
            std::rethrow_exception(std::current_exception());
        }
        rep0->addAttribute(segmentList);

        /* No start pos */
        SegmentTracker::Position pos = tracker->getStartPosition();
        Expect(pos.isValid());
        Expect(pos.number == 123);
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk == nullptr);

        /* Start pos ok */
        events.reset();
        Expect(tracker->setStartPosition() == true);
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::SegmentChange) == true);
        Expect(events.occured(TrackerEvent::Type::FormatChange) == true);
        Expect(events.segmentchanged.starttime == timescale.ToTime(START) + VLC_TICK_0);
        Expect(events.segmentchanged.duration == timescale.ToTime(100));
        Expect(events.segmentchanged.sequence == 456);
        Expect(events.formatchanged.format == StreamFormat::Type::PackedAAC);
        delete currentChunk;
        currentChunk = nullptr;

        events.reset();
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.segmentchanged.starttime == timescale.ToTime(START + 100 * 1) + VLC_TICK_0);
        Expect(events.segmentchanged.duration == timescale.ToTime(100));
        Expect(events.occured(TrackerEvent::Type::FormatChange) == false);
        Expect(events.occured(TrackerEvent::Type::SegmentChange) == true);
        delete currentChunk;
        currentChunk = nullptr;

        events.reset();
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.segmentchanged.starttime == timescale.ToTime(START + 100 * 2) + VLC_TICK_0);
        Expect(events.segmentchanged.duration == timescale.ToTime(100));
        Expect(events.occured(TrackerEvent::Type::FormatChange) == true);
        Expect(events.occured(TrackerEvent::Type::SegmentChange) == true);
        Expect(events.formatchanged.format == StreamFormat::Type::PackedAC3);
        delete currentChunk;
        currentChunk = nullptr;

        /* Should fail */
        events.reset();
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk == nullptr);
        Expect(events.occured(TrackerEvent::Type::SegmentChange) == false);

    } catch( ... ) {
        delete currentChunk;
        return 1;
    }

    return 0;
}

/****** check position/alignment ******/
static int SegmentTracker_check_seeks(BaseAdaptationSet *adaptSet,
                                      DummyLogic *,
                                      SegmentTracker *tracker,
                                      SegmentTrackerListener &events)
{
    const stime_t START = 1337;
    Timescale timescale(100);

    ChunkInterface *currentChunk = nullptr;
    try
    {
        DummyRepresentation *rep0 = new DummyRepresentation(adaptSet);
        adaptSet->addRepresentation(rep0);
        rep0->setID(ID("0"));

        SegmentList *segmentList = nullptr;
        try
        {
            segmentList = new SegmentList(rep0);
            segmentList->addAttribute(new TimescaleAttr(timescale));
            for(int i=0; i<10; i++)
            {
                Segment *seg = new Segment(rep0);
                seg->setSequenceNumber(123 + i);
                seg->setDiscontinuitySequenceNumber(456);
                seg->startTime.Set(START + 100 * i);
                seg->duration.Set(100);
                seg->setSourceUrl("sample/aac");
                segmentList->addSegment(seg);
            }
        } catch (...) {
            delete segmentList;
            std::rethrow_exception(std::current_exception());
        }
        rep0->addAttribute(segmentList);

        events.reset();
        Expect(tracker->setPositionByTime(VLC_TICK_0 + timescale.ToTime(START + 250), false, true) == true);
        Expect(events.occured(TrackerEvent::Type::PositionChange) == false);
        Expect(tracker->getPlaybackTime() == 0);

        events.reset();
        Expect(tracker->setPositionByTime(VLC_TICK_0 + timescale.ToTime(START + 250), false, false) == true);
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::PositionChange) == true);
        Expect(events.positionchanged.resumeTime == timescale.ToTime(START + 200));
        Expect(tracker->getPlaybackTime() == timescale.ToTime(START + 200));
        delete currentChunk;
        currentChunk = nullptr;

        /* past playlist, align to end */
        events.reset();
        Expect(tracker->setPositionByTime(VLC_TICK_0 + timescale.ToTime(START + 9999), false, false) == true);
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::PositionChange) == true);
        delete currentChunk;
        currentChunk = nullptr;

        /* out of playlist, we need to fail */
        Expect(tracker->setPositionByTime(VLC_TICK_0 + timescale.ToTime(START / 2), false, true) == false);
        Expect(tracker->setPositionByTime(VLC_TICK_0 + timescale.ToTime(START / 2), false, false) == false);

        /* restart playlist from startpos */
        events.reset();
        SegmentTracker::Position startpos = tracker->getStartPosition();
        Expect(startpos.isValid());
        tracker->setPosition(startpos, false);
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::PositionChange) == true);
        Expect(events.positionchanged.resumeTime == timescale.ToTime(START));
        Expect(tracker->getPlaybackTime() == timescale.ToTime(START));
        delete currentChunk;
        currentChunk = nullptr;

        /* go to unaligned pos */
        events.reset();
        Expect(tracker->setPositionByTime(VLC_TICK_0 + timescale.ToTime(START + 250), false, false) == true);
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::PositionChange) == true);
        Expect(events.positionchanged.resumeTime == timescale.ToTime(START + 2*100));
        Expect(tracker->getPlaybackTime() == timescale.ToTime(START + 2*100));
        delete currentChunk;
        currentChunk = nullptr;

    } catch( ... ) {
        delete currentChunk;
        return 1;
    }

    return 0;
}

/****** test with variants ******/
static int SegmentTracker_check_switches(BaseAdaptationSet *adaptSet,
                                         DummyLogic *logic,
                                         SegmentTracker *tracker,
                                         SegmentTrackerListener &events)
{
    const stime_t START = 1337;
    Timescale timescale(100);

    ChunkInterface *currentChunk = nullptr;
    try
    {
        DummyRepresentation *rep0 = new DummyRepresentation(adaptSet);
        adaptSet->addRepresentation(rep0);
        rep0->setID(ID("0"));

        SegmentList *segmentList = nullptr;
        try
        {
            segmentList = new SegmentList(rep0);
            segmentList->addAttribute(new TimescaleAttr(timescale));
            for(int i=0; i<5; i++)
            {
                Segment *seg = new Segment(rep0);
                seg->setSequenceNumber(123 + i);
                seg->setDiscontinuitySequenceNumber(456);
                seg->startTime.Set(START + 100 * i);
                seg->duration.Set(100);
                seg->setSourceUrl("sample/aac");
                segmentList->addSegment(seg);
            }
        } catch (...) {
            delete segmentList;
            std::rethrow_exception(std::current_exception());
        }
        rep0->addAttribute(segmentList);
        segmentList->getSegments().at(4)->discontinuity = true;
        segmentList->getSegments().at(4)->setDiscontinuitySequenceNumber(457);

        DummyRepresentation *rep1 = new DummyRepresentation(adaptSet);
        adaptSet->addRepresentation(rep1);
        rep1->setID(ID("1"));

        try
        {
            segmentList = new SegmentList(rep1);
            segmentList->addAttribute(new TimescaleAttr(timescale));
            for(int i=0; i<5; i++)
            {
                Segment *seg = new Segment(rep1);
                seg->setSequenceNumber(123 + i);
                seg->setDiscontinuitySequenceNumber(456);
                seg->startTime.Set(START + 100 * i);
                seg->duration.Set(100);
                seg->setSourceUrl("sample/aac");
                segmentList->addSegment(seg);
            }
        } catch (...) {
            delete segmentList;
            std::rethrow_exception(std::current_exception());
        }
        rep1->addAttribute(segmentList);

        /* have some init segment on rep1 */
        InitSegment *initSegment = new InitSegment(rep1);
        initSegment->setSourceUrl("sample/aacinit");
        segmentList->initialisationSegment.Set(initSegment);

        Expect(adaptSet->isSegmentAligned());
        Expect(adaptSet->getRepresentations().size() == 2);

        /* initial switch notification */
        events.reset();
        Expect(tracker->setStartPosition() == true);
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::RepresentationSwitch) == true);
        Expect(events.representationchanged.prev == nullptr);
        Expect(events.representationchanged.next == rep0);
        delete currentChunk;
        currentChunk = nullptr;

        /* ask logic to switch */
        logic->repindex = 1;
        /* switch event should not trigger here */
        events.reset();
        currentChunk = tracker->getNextChunk(false);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::RepresentationSwitch) == false);
        delete currentChunk;
        currentChunk = nullptr;

        events.reset();
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::RepresentationSwitch) == true);
        Expect(events.representationchanged.prev == rep0);
        Expect(events.representationchanged.next == rep1);
        /* check returned init */
        Expect(currentChunk->getContentType() == "sample/aacinit");
        Expect(events.segmentchanged.starttime == timescale.ToTime(START + 100 * 2) + VLC_TICK_0);
        Expect(events.segmentchanged.duration == timescale.ToTime(100));
        Expect(events.segmentchanged.sequence == 456);
        delete currentChunk;
        currentChunk = nullptr;

        /* check segment */
        events.reset();
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::RepresentationSwitch) == false);
        Expect(currentChunk->getContentType() == "sample/aac");
        /* time should remain the same */
        Expect(events.segmentchanged.starttime == timescale.ToTime(START + 100 * 2) + VLC_TICK_0);
        Expect(events.segmentchanged.duration == timescale.ToTime(100));
        Expect(events.segmentchanged.sequence == 456);
        delete currentChunk;
        currentChunk = nullptr;

        /* ask logic to switch back */
        logic->repindex = 0;
        events.reset();
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::RepresentationSwitch) == true);
        Expect(events.representationchanged.prev == rep1);
        Expect(events.representationchanged.next == rep0);
        delete currentChunk;
        currentChunk = nullptr;

        /* check continuity & sequence handling */
        events.reset();
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::Discontinuity) == true);
        Expect(events.segmentchanged.sequence == 457);
        Expect(events.discontinuity == true);
        delete currentChunk;
        currentChunk = nullptr;

        /* reset playlist to rep1 */
        logic->repindex = 1;
        events.reset();
        SegmentTracker::Position startpos = tracker->getStartPosition();
        Expect(startpos.isValid());
        tracker->setPosition(startpos, false);
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::RepresentationSwitch) == true);
        Expect(events.representationchanged.prev == nullptr);
        Expect(events.representationchanged.next == rep1);
        Expect(currentChunk->getContentType() == "sample/aacinit");
        delete currentChunk;
        currentChunk = nullptr;

        /* We must not allow repA init to unmatched repB X */
        logic->repindex = 0;
        events.reset();
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::RepresentationSwitch) == false);
        Expect(currentChunk->getContentType() == "sample/aac");
        delete currentChunk;
        currentChunk = nullptr;

        /* should now allow switch on next segment */
        events.reset();
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::RepresentationSwitch) == true);
        delete currentChunk;
        currentChunk = nullptr;

    } catch( ... ) {
        delete currentChunk;
        return 1;
    }

    return 0;
}

/****** check position/alignment with segment translation ******/
static int SegmentTracker_check_HLSseeks(BaseAdaptationSet *adaptSet,
                                         DummyLogic *logic,
                                         SegmentTracker *tracker,
                                         SegmentTrackerListener &events)
{
    const stime_t START = 1337;
    Timescale timescale(100);

    ChunkInterface *currentChunk = nullptr;
    try
    {
        DummyHLSRepresentation *rep0 = new DummyHLSRepresentation(adaptSet, 1);
        adaptSet->addRepresentation(rep0);
        rep0->setID(ID("0"));

        SegmentList *segmentList = nullptr;
        try
        {
            segmentList = new SegmentList(rep0);
            segmentList->addAttribute(new TimescaleAttr(timescale));
            for(int i=0; i<10; i++)
            {
                Segment *seg = new Segment(rep0);
                seg->setSequenceNumber(123 + i);
                seg->startTime.Set(START + 100 * i);
                seg->duration.Set(100);
                seg->setSourceUrl("sample/aac");
                segmentList->addSegment(seg);
            }
        } catch (...) {
            delete segmentList;
            std::rethrow_exception(std::current_exception());
        }
        rep0->addAttribute(segmentList);

        DummyHLSRepresentation *rep1 = new DummyHLSRepresentation(adaptSet, 2.5);
        adaptSet->addRepresentation(rep1);
        rep1->setID(ID("1"));

        segmentList = nullptr;
        try
        {
            segmentList = new SegmentList(rep1);
            segmentList->addAttribute(new TimescaleAttr(timescale));
            for(int i=0; i<4; i++)
            {
                Segment *seg = new Segment(rep1);
                seg->setSequenceNumber(123 + i);
                seg->startTime.Set(START + 250 * i);
                seg->duration.Set(250);
                seg->setSourceUrl("sample/aac");
                segmentList->addSegment(seg);
            }
        } catch (...) {
            delete segmentList;
            std::rethrow_exception(std::current_exception());
        }
        rep1->addAttribute(segmentList);

        /* on rep0 */
        Expect(tracker->setPositionByTime(VLC_TICK_0 + timescale.ToTime(START + 300), false, false) == true);
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::PositionChange) == true);
        Expect(events.positionchanged.resumeTime == timescale.ToTime(START + 300));
        Expect(tracker->getPlaybackTime() == timescale.ToTime(START + 300));
        delete currentChunk;
        currentChunk = nullptr;

        /* on rep1 */
        logic->repindex = 1;
        events.reset();
        currentChunk = tracker->getNextChunk(true);
        Expect(currentChunk);
        Expect(events.occured(TrackerEvent::Type::RepresentationSwitch) == true);
        Expect(tracker->getPlaybackTime() == timescale.ToTime(START + 250)); // FIXME
        delete currentChunk;
        currentChunk = nullptr;

    } catch( ... ) {
        delete currentChunk;
        return 1;
    }

    return 0;
}

typedef decltype(SegmentTracker_check_formats) testfunc;

static int Prepare_test(testfunc func)
{
    DummyConnectionManager *connManager = nullptr;
    try
    {
        connManager = new DummyConnectionManager;
    } catch( ... ) { return 1; }

    connManager->data.insert(mapentry("sample/aac", std::vector<uint8_t>({ 0xFF, 0xF1, 0, 0 })));
    connManager->data.insert(mapentry("sample/ac3", std::vector<uint8_t>({ 0x0b, 0x77, 0, 0, 0, 0 })));
    connManager->data.insert(mapentry("sample/aacinit", std::vector<uint8_t>({ 0xFF, 0xF1, 0, 0 })));

    SharedResources sharedRes(nullptr, nullptr, connManager);
    DefaultBufferingLogic bufLogic;
    SynchronizationReferences syncRefs;

    BaseAdaptationSet *adaptSet = CreatePlaylistPeriodAdaptationSet();
    if(!adaptSet)
        return 1;

    BasePlaylist *playlist = adaptSet->getPlaylist();

    SegmentTracker *tracker;
    DummyLogic *logic = nullptr;
    try
    {
        logic = new DummyLogic();
        tracker = new SegmentTracker(&sharedRes, logic, &bufLogic, adaptSet, &syncRefs);
    } catch(...) {
        delete playlist;
        delete logic;
        return 1;
    }

    SegmentTrackerListener events;
    tracker->registerListener(&events);

    int ret = func(adaptSet, logic, tracker, events);

    delete tracker;
    delete logic;
    delete playlist;

    return ret;
}

int SegmentTracker_test()
{
    return
        Prepare_test(SegmentTracker_check_formats) ||
        Prepare_test(SegmentTracker_check_seeks) ||
        Prepare_test(SegmentTracker_check_switches) ||
        Prepare_test(SegmentTracker_check_HLSseeks) ||
        0;
}
