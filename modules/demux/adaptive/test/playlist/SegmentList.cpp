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
#include "../../playlist/BasePeriod.h"
#include "../../playlist/BaseAdaptationSet.h"
#include "../../playlist/BaseRepresentation.h"

#include "../test.hpp"

#include <limits>

using namespace adaptive;
using namespace adaptive::playlist;

int SegmentList_test()
{
    SegmentList *segmentList = nullptr;
    SegmentList *segmentList2 = nullptr;
    try
    {
        segmentList = new SegmentList(nullptr);
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
        Segment *seg = new Segment(nullptr);
        seg->setSequenceNumber(123);
        seg->startTime.Set(START);
        seg->duration.Set(100);
        segmentList->addSegment(seg);

        Expect(segmentList->getTotalLength() == 100);
        Expect(segmentList->getSegmentNumberByTime(timescale.ToTime(0), &number) == false);
        Expect(segmentList->getSegmentNumberByTime(timescale.ToTime(START), &number) == true);
        Expect(number == 123);
        Expect(segmentList->getPlaybackTimeDurationBySegmentNumber(123, &time, &duration) == true);
        Expect(time == timescale.ToTime(START));
        Expect(duration == timescale.ToTime(100));
        seg = segmentList->getMediaSegment(123);
        Expect(seg);
        Expect(seg->getSequenceNumber() == 123);
        Expect(seg->startTime.Get() == START);
        seg = segmentList->getNextMediaSegment(123, &number, &discont);
        Expect(seg);
        Expect(number == 123);
        Expect(!discont);
        seg = segmentList->getNextMediaSegment(122, &number, &discont);
        Expect(seg);
        Expect(number == 123);
        Expect(discont);
        Expect(segmentList->getMinAheadTime(0) == timescale.ToTime(100));
        Expect(segmentList->getMinAheadTime(123) == timescale.ToTime(0));

        for(int i=1; i<10; i++)
        {
            seg = new Segment(nullptr);
            seg->setSequenceNumber(123 + i);
            seg->startTime.Set(START + 100 * i);
            seg->duration.Set(100);
            segmentList->addSegment(seg);
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
        segmentList2 = new SegmentList(nullptr);
        for(int i=5; i<20; i++)
        {
            seg = new Segment(nullptr);
            seg->setSequenceNumber(123 + i);
            seg->startTime.Set(START + 100 * i);
            seg->duration.Set(100);
            segmentList2->addSegment(seg);
        }
        segmentList->updateWith(segmentList2);
        Expect(segmentList->getStartSegmentNumber() == 123 + 5);
        Expect(segmentList->getTotalLength() == 100 * 15);

        for(int i=5; i<20; i++)
        {
            seg = segmentList->getMediaSegment(123 + i);
            Expect(seg);
            Expect(seg->getSequenceNumber() == (uint64_t) 123 + i);
            Expect(seg->startTime.Get() == START + 100 * i);
            Expect(seg->duration.Get() == 100);
        }

        /* prune */
        segmentList->pruneByPlaybackTime(timescale.ToTime(START+100*6));
        Expect(segmentList->getStartSegmentNumber() == 123 + 6);
        Expect(segmentList->getTotalLength() == 100 * 14);

        segmentList->pruneBySegmentNumber(123+10);
        Expect(segmentList->getStartSegmentNumber() == 123 + 10);
        Expect(segmentList->getTotalLength() == 100 * 10);

        delete segmentList;
        delete segmentList2;
        segmentList2 = nullptr;

        /* gap updates, relative timings */
        segmentList = new SegmentList(nullptr, true);
        segmentList->addAttribute(new TimescaleAttr(timescale));
        segmentList->addAttribute(new DurationAttr(100));
        Expect(segmentList->inheritDuration());
        for(int i=0; i<2; i++)
        {
            seg = new Segment(nullptr);
            seg->setSequenceNumber(123 + i);
            seg->startTime.Set(START + 100 * i);
            seg->duration.Set(100);
            segmentList->addSegment(seg);
        }
        segmentList2 = new SegmentList(nullptr, true);
        for(int i=0; i<2; i++)
        {
            seg = new Segment(nullptr);
            seg->setSequenceNumber(128 + i);
            seg->startTime.Set(START + 100 * i);
            seg->duration.Set(100);
            segmentList2->addSegment(seg);
        }
        segmentList->updateWith(segmentList2);
        Expect(segmentList->getStartSegmentNumber() == 128);
        Expect(segmentList->getSegments().size() == 2);
        Expect(segmentList->getSegments().at(0)->getSequenceNumber() == 128);
        Expect(segmentList->getSegments().at(1)->getSequenceNumber() == 129);
        Expect(segmentList->getSegments().at(0)->startTime.Get() == START + 100 * (128 - 123));
        Expect(segmentList->getSegments().at(1)->startTime.Get() == START + 100 * (129 - 123));

        delete segmentList;
        delete segmentList2;
        segmentList2 = nullptr;

	/* overlapping updates, relative timings */
	segmentList = new SegmentList(nullptr, true);
	segmentList->addAttribute(new TimescaleAttr(timescale));
	segmentList->addAttribute(new DurationAttr(100));
	Expect(segmentList->inheritDuration());
	for(int i=0; i<2; i++)
	{
		seg = new Segment(nullptr);
		seg->setSequenceNumber(123 + i);
		seg->startTime.Set(START + 100 * i);
		seg->duration.Set(100);
		segmentList->addSegment(seg);
	}
	segmentList2 = new SegmentList(nullptr, true);
	for(int i=0; i<3; i++)
	{
		seg = new Segment(nullptr);
		seg->setSequenceNumber(123 + i);
		seg->startTime.Set(START + 100 * i);
		seg->duration.Set(100);
		segmentList2->addSegment(seg);
	}
	segmentList->updateWith(segmentList2);
	Expect(segmentList->getSegments().size() == 3);
	Expect(segmentList->getSegments().at(0)->getSequenceNumber() == 123);
	Expect(segmentList->getSegments().at(1)->getSequenceNumber() == 124);
	Expect(segmentList->getSegments().at(2)->getSequenceNumber() == 125);

	delete segmentList;
	delete segmentList2;
        segmentList2 = nullptr;

        /* gap updates, absolute media timings */
        segmentList = new SegmentList(nullptr, false);
        segmentList->addAttribute(new TimescaleAttr(timescale));
        segmentList->addAttribute(new DurationAttr(999));
        Expect(segmentList->inheritDuration());
        for(int i=0; i<2; i++)
        {
            seg = new Segment(nullptr);
            seg->setSequenceNumber(123 + i);
            seg->startTime.Set(START + 100 * i);
            seg->duration.Set(100);
            segmentList->addSegment(seg);
        }
        segmentList2 = new SegmentList(nullptr, false);
        for(int i=5; i<7; i++)
        {
            seg = new Segment(nullptr);
            seg->setSequenceNumber(123 + i);
            seg->startTime.Set(START + 100 * i);
            seg->duration.Set(100);
            segmentList2->addSegment(seg);
        }
        segmentList->updateWith(segmentList2);
        Expect(segmentList->getStartSegmentNumber() == 128);
        Expect(segmentList->getSegments().size() == 2);
        Expect(segmentList->getSegments().at(0)->getSequenceNumber() == 128);
        Expect(segmentList->getSegments().at(1)->getSequenceNumber() == 129);
        Expect(segmentList->getSegments().at(0)->startTime.Get() == START + 100 * (128 - 123));
        Expect(segmentList->getSegments().at(1)->startTime.Get() == START + 100 * (129 - 123));

        delete segmentList;
        delete segmentList2;
        segmentList2 = nullptr;

        /* Tricky now, check timelined */
        segmentList = new SegmentList(nullptr);
        segmentList->addAttribute(new TimescaleAttr(timescale));
        for(int i=0; i<10; i++)
        {
            seg = new Segment(nullptr);
            seg->setSequenceNumber(123 + i);
            seg->startTime.Set(START + 100 * i);
            seg->duration.Set(100);
            segmentList->addSegment(seg);
        }
        const std::vector<Segment*>&allsegments = segmentList->getSegments();

        SegmentTimeline *timeline = new SegmentTimeline(nullptr);
        segmentList->addAttribute(timeline);
        timeline->addElement(44, 100, 4, START);
        Expect(timeline->getTotalLength() == 5 * 100);
        Expect(segmentList->getStartSegmentNumber() == 44);
        Expect(segmentList->getTotalLength() == timeline->getTotalLength());
        seg = segmentList->getMediaSegment(44 + 2);
        Expect(seg);
        Expect(seg == allsegments.at(0));
        Expect(segmentList->getMediaSegment(44 + 6) == nullptr); /* restricted window */

        timeline->addElement(44 + 5, 100, 1, START + 5*100);
        Expect(timeline->getTotalLength() == 7 * 100);
        seg = segmentList->getMediaSegment(44 + 6);
        Expect(seg);
        Expect(seg == allsegments.at(1));

        delete segmentList;

    } catch (...) {
        delete segmentList;
        delete segmentList2;
        return 1;
    }

    return 0;
}
