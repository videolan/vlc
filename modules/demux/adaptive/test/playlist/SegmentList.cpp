/*****************************************************************************
 *
 *****************************************************************************
 * Copyright (C) 2020 VideoLabs, VideoLAN and VLC Authors
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

#include "../../playlist/Segment.h"
#include "../../playlist/SegmentList.h"
#include "../../playlist/SegmentTimeline.h"
#include "../../playlist/BaseAdaptationSet.h"
#include "../../playlist/BaseRepresentation.h"

#include "../test.hpp"

#include <limits>
#include <memory>

using namespace adaptive;
using namespace adaptive::playlist;

int SegmentList_test() try
{
    std::unique_ptr<SegmentList> segmentList;
    std::unique_ptr<SegmentList> segmentList2;
    segmentList = std::make_unique<SegmentList>(nullptr);
    Timescale timescale(100);
    segmentList->addAttribute(new TimescaleAttr(timescale));
    /* Check failures */
    Expect(segmentList->getStartSegmentNumber() == std::numeric_limits<uint64_t>::max());
    Expect(segmentList->getTotalLength() == 0);
    uint64_t number; bool discont;
    Expect(segmentList->getSegmentNumberByTime(1, &number) == false);
    Expect(segmentList->getMediaSegment(0) == nullptr);
    Expect(segmentList->getNextMediaSegment(0, &number, &discont) == nullptr);
    Expect(segmentList->getMinAheadTime(0) == 0);
    vlc_tick_t time, duration;
    Expect(segmentList->getPlaybackTimeDurationBySegmentNumber(0, &time, &duration) == false);

    /* Simple elements list */
    const stime_t START = 1337;
    std::unique_ptr<Segment> seg = std::make_unique<Segment>(nullptr);
    seg->setSequenceNumber(123);
    seg->startTime.Set(START);
    seg->duration.Set(100);
    segmentList->addSegment(seg.release());

    Expect(segmentList->getTotalLength() == 100);
    Expect(segmentList->getSegmentNumberByTime(timescale.ToTime(0), &number) == false);
    Expect(segmentList->getSegmentNumberByTime(timescale.ToTime(START), &number) == true);
    Expect(number == 123);
    Expect(segmentList->getPlaybackTimeDurationBySegmentNumber(123, &time, &duration) == true);
    Expect(time == timescale.ToTime(START));
    Expect(duration == timescale.ToTime(100));
    Segment *segptr = segmentList->getMediaSegment(123);
    Expect(segptr);
    Expect(segptr->getSequenceNumber() == 123);
    Expect(segptr->startTime.Get() == START);
    segptr = segmentList->getNextMediaSegment(123, &number, &discont);
    Expect(segptr);
    Expect(number == 123);
    Expect(!discont);
    segptr = segmentList->getNextMediaSegment(122, &number, &discont);
    Expect(segptr);
    Expect(number == 123);
    Expect(discont);
    Expect(segmentList->getMinAheadTime(0) == timescale.ToTime(100));
    Expect(segmentList->getMinAheadTime(123) == timescale.ToTime(0));

    for(int i=1; i<10; i++)
    {
        std::unique_ptr<Segment> seg = std::make_unique<Segment>(nullptr);
        seg->setSequenceNumber(123 + i);
        seg->startTime.Set(START + 100 * i);
        seg->duration.Set(100);
        segmentList->addSegment(seg.release());
    }

    Expect(segmentList->getTotalLength() == 100 * 10);
    Expect(segmentList->getMinAheadTime(123) == timescale.ToTime(100 * 9));
    Expect(segmentList->getSegmentNumberByTime(timescale.ToTime(START + 100*9 - 1), &number) == true);
    Expect(number == 123 + 8);
    Expect(segmentList->getPlaybackTimeDurationBySegmentNumber(123 + 8, &time, &duration) == true);
    Expect(time == timescale.ToTime(START + 100 * 8));
    Expect(duration == timescale.ToTime(100));
    Expect(segmentList->getMinAheadTime(123+8) == timescale.ToTime(100));

    /* merge */
    segmentList2 = std::make_unique<SegmentList>(nullptr);
    for(int i=5; i<20; i++)
    {
        std::unique_ptr<Segment> seg = std::make_unique<Segment>(nullptr);
        seg->setSequenceNumber(123 + i);
        seg->startTime.Set(START + 100 * i);
        seg->duration.Set(100);
        segmentList2->addSegment(seg.release());
    }
    segmentList->updateWith(segmentList2.get());
    Expect(segmentList->getStartSegmentNumber() == 123 + 5);
    Expect(segmentList->getTotalLength() == 100 * 15);

    for(int i=5; i<20; i++)
    {
        segptr = segmentList->getMediaSegment(123 + i);
        Expect(segptr);
        Expect(segptr->getSequenceNumber() == (uint64_t) 123 + i);
        Expect(segptr->startTime.Get() == START + 100 * i);
        Expect(segptr->duration.Get() == 100);
    }

    /* prune */
    segmentList->pruneByPlaybackTime(timescale.ToTime(START+100*6));
    Expect(segmentList->getStartSegmentNumber() == 123 + 6);
    Expect(segmentList->getTotalLength() == 100 * 14);

    segmentList->pruneBySegmentNumber(123+10);
    Expect(segmentList->getStartSegmentNumber() == 123 + 10);
    Expect(segmentList->getTotalLength() == 100 * 10);

    segmentList.reset();
    segmentList2.reset();

    /* overlapping updates, relative timings */
    segmentList = std::make_unique<SegmentList>(nullptr, true);
    segmentList->addAttribute(new TimescaleAttr(timescale));
    segmentList->addAttribute(new DurationAttr(100));
    Expect(segmentList->inheritDuration());
    for(int i=0; i<2; i++)
    {
        seg = std::make_unique<Segment>(nullptr);
        seg->setSequenceNumber(123 + i);
        seg->startTime.Set(START + 100 * i);
        seg->duration.Set(100);
        segmentList->addSegment(seg.release());
    }
    segmentList2 = std::make_unique<SegmentList>(nullptr, true);
    for(int i=0; i<3; i++)
    {
        seg = std::make_unique<Segment>(nullptr);
        seg->setSequenceNumber(123 + i);
        seg->startTime.Set(START + 100 * i);
        seg->duration.Set(100);
        segmentList2->addSegment(seg.release());
    }
    segmentList->updateWith(segmentList2.get());
    Expect(segmentList->getSegments().size() == 3);
    Expect(segmentList->getSegments().at(0)->getSequenceNumber() == 123);
    Expect(segmentList->getSegments().at(1)->getSequenceNumber() == 124);
    Expect(segmentList->getSegments().at(2)->getSequenceNumber() == 125);

    segmentList.reset();
    segmentList2.reset();

    /* gap updates, relative timings */
    segmentList = std::make_unique<SegmentList>(nullptr, true);
    segmentList->addAttribute(new TimescaleAttr(timescale));
    segmentList->addAttribute(new DurationAttr(100));
    Expect(segmentList->inheritDuration());
    for(int i=0; i<2; i++)
    {
        seg = std::make_unique<Segment>(nullptr);
        seg->setSequenceNumber(123 + i);
        seg->startTime.Set(START + 100 * i);
        seg->duration.Set(100);
        segmentList->addSegment(seg.release());
    }
    segmentList2 = std::make_unique<SegmentList>(nullptr, true);
    for(int i=0; i<2; i++)
    {
        seg = std::make_unique<Segment>(nullptr);
        seg->setSequenceNumber(128 + i);
        seg->startTime.Set(START + 100 * i);
        seg->duration.Set(100);
        segmentList2->addSegment(seg.release());
    }
    segmentList->updateWith(segmentList2.get());
    Expect(segmentList->getStartSegmentNumber() == 128);
    Expect(segmentList->getSegments().size() == 2);
    Expect(segmentList->getSegments().at(0)->getSequenceNumber() == 128);
    Expect(segmentList->getSegments().at(1)->getSequenceNumber() == 129);
    Expect(segmentList->getSegments().at(0)->startTime.Get() == START + 100 * (128 - 123));
    Expect(segmentList->getSegments().at(1)->startTime.Get() == START + 100 * (129 - 123));

    segmentList.reset();
    segmentList2.reset();

    /* gap updates, absolute media timings */
    segmentList = std::make_unique<SegmentList>(nullptr, false);
    segmentList->addAttribute(new TimescaleAttr(timescale));
    segmentList->addAttribute(new DurationAttr(999));
    Expect(segmentList->inheritDuration());
    for(int i=0; i<2; i++)
    {
        seg = std::make_unique<Segment>(nullptr);
        seg->setSequenceNumber(123 + i);
        seg->startTime.Set(START + 100 * i);
        seg->duration.Set(100);
        segmentList->addSegment(seg.release());
    }
    segmentList2 = std::make_unique<SegmentList>(nullptr, false);
    for(int i=5; i<7; i++)
    {
        seg = std::make_unique<Segment>(nullptr);
        seg->setSequenceNumber(123 + i);
        seg->startTime.Set(START + 100 * i);
        seg->duration.Set(100);
        segmentList2->addSegment(seg.release());
    }
    segmentList->updateWith(segmentList2.get());
    Expect(segmentList->getStartSegmentNumber() == 128);
    Expect(segmentList->getSegments().size() == 2);
    Expect(segmentList->getSegments().at(0)->getSequenceNumber() == 128);
    Expect(segmentList->getSegments().at(1)->getSequenceNumber() == 129);
    Expect(segmentList->getSegments().at(0)->startTime.Get() == START + 100 * (128 - 123));
    Expect(segmentList->getSegments().at(1)->startTime.Get() == START + 100 * (129 - 123));

    segmentList.reset();
    segmentList2.reset();

    /* Tricky now, check timelined */
    segmentList = std::make_unique<SegmentList>(nullptr);
    segmentList->addAttribute(new TimescaleAttr(timescale));
    for(int i=0; i<10; i++)
    {
        seg = std::make_unique<Segment>(nullptr);
        seg->setSequenceNumber(123 + i);
        seg->startTime.Set(START + 100 * i);
        seg->duration.Set(100);
        segmentList->addSegment(seg.release());
    }
    const std::vector<Segment*>&allsegments = segmentList->getSegments();

    SegmentTimeline *timeline = new SegmentTimeline(nullptr);
    segmentList->addAttribute(timeline);
    timeline->addElement(44, 100, 4, START);
    Expect(timeline->getTotalLength() == 5 * 100);
    Expect(segmentList->getStartSegmentNumber() == 44);
    Expect(segmentList->getTotalLength() == timeline->getTotalLength());
    segptr = segmentList->getMediaSegment(44 + 2);
    Expect(segptr);
    Expect(segptr == allsegments.at(0));
    Expect(segmentList->getMediaSegment(44 + 6) == nullptr); /* restricted window */

    timeline->addElement(44 + 5, 100, 1, START + 5*100);
    Expect(timeline->getTotalLength() == 7 * 100);
    segptr = segmentList->getMediaSegment(44 + 6);
    Expect(segptr);
    Expect(segptr == allsegments.at(1));

    segmentList.reset();

    return 0;
} catch (...) {
    return 1;
}
