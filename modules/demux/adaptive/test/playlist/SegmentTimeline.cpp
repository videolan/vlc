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
#include "../../playlist/SegmentTimeline.h"
#include "../../playlist/BaseAdaptationSet.h"
#include "../../playlist/BaseRepresentation.h"

#include "../test.hpp"

#include <limits>

using namespace adaptive;
using namespace adaptive::playlist;

int Timeline_test()
{
    SegmentTimeline *timeline = nullptr;
    SegmentTimeline *timeline2 = nullptr;
    try
    {
        timeline = new SegmentTimeline(nullptr);
        /* Check failures */
        Expect(timeline->getTotalLength() == 0);
        Expect(timeline->getElementIndexBySequence(123) == std::numeric_limits<uint64_t>::max());
        Expect(timeline->getElementNumberByScaledPlaybackTime(123) == 0);
        Expect(timeline->getMinAheadScaledTime(0) == 0);
        Expect(timeline->minElementNumber() == 0);
        Expect(timeline->maxElementNumber() == 0);

        /* Simple elements list */
        const stime_t START = 1337;
        timeline->addElement(11, 100, 0, START);
        timeline->addElement(12, 50, 0, 0);
        timeline->addElement(13, 25, 0, 0);

        Expect(timeline->minElementNumber() == 11);
        Expect(timeline->maxElementNumber() == 13);
        Expect(timeline->getTotalLength() == 175);
        uint64_t idx = timeline->getElementIndexBySequence(0);
        Expect(idx == std::numeric_limits<uint64_t>::max());
        idx = timeline->getElementIndexBySequence(100);
        Expect(idx == std::numeric_limits<uint64_t>::max());
        idx = timeline->getElementIndexBySequence(11);
        Expect(idx == 0);
        idx = timeline->getElementIndexBySequence(13);
        Expect(idx == 2);

        Expect(timeline->getMinAheadScaledTime(11) == 75);
        Expect(timeline->getMinAheadScaledTime(12) == 25);
        Expect(timeline->getMinAheadScaledTime(14) == 0);
        Expect(timeline->getElementIndexBySequence(13) == 2);
        Expect(timeline->getScaledPlaybackTimeByElementNumber(11) == START+0);
        Expect(timeline->getScaledPlaybackTimeByElementNumber(13) == START+150);
        stime_t time, duration;
        Expect(timeline->getScaledPlaybackTimeDurationBySegmentNumber(12, &time, &duration));
        Expect(time == START+100);
        Expect(duration == 50);
        Expect(timeline->getElementNumberByScaledPlaybackTime(START-1) == 11);
        Expect(timeline->getElementNumberByScaledPlaybackTime(START+9) == 11);
        Expect(timeline->getElementNumberByScaledPlaybackTime(START+151) == 13);

        /* Check repeats */
        timeline->addElement(14, 100, 1, 0);
        Expect(timeline->minElementNumber() == 11);
        Expect(timeline->maxElementNumber() == 14 + 1);
        Expect(timeline->getTotalLength() == 175 + 100*2);
        Expect(timeline->getElementIndexBySequence(14) == 3);
        Expect(timeline->getElementIndexBySequence(15) == 3);
        timeline->addElement(16, 20, 9, 0);
        Expect(timeline->maxElementNumber() == 16 + 9);
        Expect(timeline->getTotalLength() == 175 + 100*2 + 20*10);

        Expect(timeline->getMinAheadScaledTime(14) == 100 + 20 * 10);
        Expect(timeline->getMinAheadScaledTime(15) == 20 * 10);
        Expect(timeline->getMinAheadScaledTime(20) == 20 * 5);

        Expect(timeline->getScaledPlaybackTimeByElementNumber(15) == START + 175 + 100);
        Expect(timeline->getScaledPlaybackTimeByElementNumber(21) == START + 175 + 100*2 + 20*5);

        Expect(timeline->getElementNumberByScaledPlaybackTime(START + 175 + 100 + 10) == 15);

        /* Check discontinuity */
        timeline->addElement(40, 33, 1, START + 1000);
        Expect(timeline->maxElementNumber() == 41);
        Expect(timeline->getTotalLength() == 175 + 100*2 + 20*10 + 66);
        Expect(timeline->getElementIndexBySequence(39) == std::numeric_limits<uint64_t>::max());
        Expect(timeline->getElementIndexBySequence(41) == 5);

        /* Pruning */
        Expect(timeline->pruneBySequenceNumber(24) == 5+8);
        Expect(timeline->minElementNumber() == 24);
        Expect(timeline->getTotalLength() == 20 * 2 + 33 * 2);

        Timescale timescale(100);
        timeline->addAttribute(new TimescaleAttr(timescale));
        timeline->pruneByPlaybackTime(timescale.ToTime(START + 175 + 100*2 + 20*9 + 2));
        Expect(timeline->minElementNumber() == 25);
        Expect(timeline->getTotalLength() == 20 + 33 * 2);
        timeline->pruneByPlaybackTime(timescale.ToTime(START + 175 + 100*2 + 20*11));
        Expect(timeline->minElementNumber() == 25); /* tried to expurge before discontinuity time */
        timeline->pruneByPlaybackTime(timescale.ToTime(START + 1000 + 2));
        Expect(timeline->minElementNumber() == 40);
        Expect(timeline->getTotalLength() == 33 * 2);
        Expect(timeline->pruneBySequenceNumber(50) == 2);
        Expect(timeline->getTotalLength() == 0);

        /* Merging */
        timeline->addElement(1, 1000, 0, START);
        timeline->addElement(2, 2000, 1, 0);
        Expect(timeline->minElementNumber() == 1);
        Expect(timeline->maxElementNumber() == 2+1);
        Expect(timeline->getTotalLength() == 1000 + 2000 * 2);

        timeline2 = new SegmentTimeline(nullptr);
        timeline2->addAttribute(new TimescaleAttr(timescale));
        timeline2->addElement(1, 1000, 0, START);
        timeline2->addElement(2, 2000, 1, 0);
        Expect(timeline2->minElementNumber() == 1);
        Expect(timeline->maxElementNumber() == 2+1);

        timeline->updateWith(*timeline2); /* should do no change */
        Expect(timeline->minElementNumber() == 1);
        Expect(timeline->maxElementNumber() == 2+1);
        Expect(timeline->getTotalLength() == 1000 + 2000 * 2);

        delete timeline2;
        timeline2 = new SegmentTimeline(nullptr);
        timeline2->addElement(1, 1000, 0, START);
        timeline2->addElement(2, 2000, 1, 0);
        timeline2->addElement(4, 2, 99, 0);
        Expect(timeline2->maxElementNumber() == 4+99);
        Expect(timeline2->getTotalLength() == 1000 + 2000 * 2 + 2 * 100);

        timeline->updateWith(*timeline2); /* should append missing content */
        Expect(timeline->maxElementNumber() == 4+99);
        Expect(timeline->getTotalLength() == 1000 + 2000 * 2 + 2 * 100);

        delete timeline2;
        timeline2 = new SegmentTimeline(nullptr);
        /* add 0 more times at 1 inside offset, should not change anything */
        timeline2->addElement(4+1, 2, 99-1, START+1000 + 2000 * 2 + 2 * 1);
        timeline->updateWith(*timeline2);
        Expect(timeline->maxElementNumber() == 4+99);

        delete timeline2;
        timeline2 = new SegmentTimeline(nullptr);
        /* add 10 more times at 1 inside offset, should extend by 10 repeats */
        timeline2->addElement(4+1, 2, 99-1+10, START+1000 + 2000 * 2 + 2 * 1);
        timeline->updateWith(*timeline2);
        Expect(timeline->maxElementNumber() == 4+99+10);

        delete timeline;
        delete timeline2;

    } catch (...) {
        delete timeline;
        delete timeline2;
        return 1;
    }

    return 0;
}
