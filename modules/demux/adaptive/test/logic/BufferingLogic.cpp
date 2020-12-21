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

#include "../../playlist/SegmentBase.h"
#include "../../playlist/SegmentTimeline.h"
#include "../../playlist/SegmentTemplate.h"
#include "../../playlist/SegmentList.h"
#include "../../playlist/BasePlaylist.hpp"
#include "../../playlist/BasePeriod.h"
#include "../../playlist/BaseAdaptationSet.h"
#include "../../playlist/BaseRepresentation.h"
#include "../../playlist/Segment.h"
#include "../../logic/BufferingLogic.hpp"

#include "../test.hpp"

#include <limits>

using namespace adaptive;
using namespace adaptive::playlist;
using namespace logic;

class TestPlaylist : public BasePlaylist
{
    public:
        TestPlaylist() : BasePlaylist(nullptr)
        {
            b_live = false;
            b_lowlatency = false;
        }

        virtual ~TestPlaylist() {}

        virtual bool isLive() const override
        {
            return b_live;
        }

        virtual bool isLowLatency() const override
        {
            return b_lowlatency;
        }

        bool b_live;
        bool b_lowlatency;
};

int BufferingLogic_test()
{
    DefaultBufferingLogic bufferinglogic;
    TestPlaylist *playlist = nullptr;
    try
    {
        playlist = new TestPlaylist();
        BasePeriod *period = nullptr;
        BaseAdaptationSet *set = nullptr;
        BaseRepresentation *rep = nullptr;
        try
        {
            period = new BasePeriod(playlist);
            set = new BaseAdaptationSet(period);
            rep = new BaseRepresentation(set);
        } catch(...) {
            delete period;
            delete set;
            std::rethrow_exception(std::current_exception());
        }
        set->addRepresentation(rep);
        period->addAdaptationSet(set);
        playlist->addPeriod(period);

        Timescale timescale(100);
        set->addAttribute(new TimescaleAttr(timescale));

        SegmentList *segmentList = new SegmentList(rep);
        rep->addAttribute(segmentList);

        Expect(bufferinglogic.getStartSegmentNumber(rep) == std::numeric_limits<uint64_t>::max());

        stime_t segmentduration = timescale.ToScaled(DefaultBufferingLogic::BUFFERING_LOWEST_LIMIT / 2);
        uint64_t number = 22;
        Segment *seg = new Segment(rep);
        seg->setSequenceNumber(number);
        seg->duration.Set(segmentduration);
        segmentList->addSegment(seg);

        Expect(bufferinglogic.getStartSegmentNumber(rep) == number);
        Expect(bufferinglogic.getMinBuffering(playlist) == DefaultBufferingLogic::DEFAULT_MIN_BUFFERING);
        Expect(bufferinglogic.getMaxBuffering(playlist) == DefaultBufferingLogic::DEFAULT_MAX_BUFFERING);

        bufferinglogic.setUserMinBuffering(DefaultBufferingLogic::DEFAULT_MIN_BUFFERING / 2);
        Expect(bufferinglogic.getMinBuffering(playlist) == std::max(DefaultBufferingLogic::DEFAULT_MIN_BUFFERING / 2,
                                                                    DefaultBufferingLogic::BUFFERING_LOWEST_LIMIT));

        bufferinglogic.setUserMinBuffering(DefaultBufferingLogic::BUFFERING_LOWEST_LIMIT / 2);
        Expect(bufferinglogic.getMinBuffering(playlist) == DefaultBufferingLogic::BUFFERING_LOWEST_LIMIT);

        bufferinglogic.setUserMinBuffering(DefaultBufferingLogic::DEFAULT_MIN_BUFFERING);
        bufferinglogic.setUserMaxBuffering(DefaultBufferingLogic::DEFAULT_MIN_BUFFERING / 2);
        Expect(bufferinglogic.getMaxBuffering(playlist) == DefaultBufferingLogic::DEFAULT_MIN_BUFFERING);

        bufferinglogic.setUserMinBuffering(DefaultBufferingLogic::BUFFERING_LOWEST_LIMIT / 2);
        bufferinglogic.setUserMaxBuffering(DefaultBufferingLogic::BUFFERING_LOWEST_LIMIT / 2);
        Expect(bufferinglogic.getMaxBuffering(playlist) == DefaultBufferingLogic::BUFFERING_LOWEST_LIMIT);

        playlist->b_live = true;
        bufferinglogic.setUserMinBuffering(0);
        bufferinglogic.setUserMaxBuffering(0);
        Expect(bufferinglogic.getMinBuffering(playlist) == DefaultBufferingLogic::DEFAULT_MIN_BUFFERING);
        Expect(bufferinglogic.getMaxBuffering(playlist) <= DefaultBufferingLogic::DEFAULT_MAX_BUFFERING);
        Expect(bufferinglogic.getLiveDelay(playlist) == DefaultBufferingLogic::DEFAULT_LIVE_BUFFERING);

        bufferinglogic.setUserLiveDelay(DefaultBufferingLogic::DEFAULT_MIN_BUFFERING / 2);
        Expect(bufferinglogic.getLiveDelay(playlist) ==std::max(DefaultBufferingLogic::DEFAULT_MIN_BUFFERING,
                                                                DefaultBufferingLogic::BUFFERING_LOWEST_LIMIT));

        playlist->b_lowlatency = true;
        bufferinglogic.setUserLiveDelay(0);
        if(DefaultBufferingLogic::DEFAULT_MIN_BUFFERING > DefaultBufferingLogic::BUFFERING_LOWEST_LIMIT)
            Expect(bufferinglogic.getMinBuffering(playlist) < DefaultBufferingLogic::DEFAULT_MIN_BUFFERING);
        Expect(bufferinglogic.getMaxBuffering(playlist) < DefaultBufferingLogic::DEFAULT_MAX_BUFFERING);
        Expect(bufferinglogic.getMinBuffering(playlist) >= DefaultBufferingLogic::BUFFERING_LOWEST_LIMIT);
        Expect(bufferinglogic.getLiveDelay(playlist) >= DefaultBufferingLogic::BUFFERING_LOWEST_LIMIT);

        playlist->b_lowlatency = false;
        Expect(bufferinglogic.getStartSegmentNumber(rep) == number);

        while(segmentList->getTotalLength() <
              timescale.ToScaled(DefaultBufferingLogic::DEFAULT_MAX_BUFFERING))
        {
            seg = new Segment(rep);
            seg->setSequenceNumber(++number);
            seg->duration.Set(segmentduration);
            segmentList->addSegment(seg);
        }

        Expect(bufferinglogic.getStartSegmentNumber(rep) > 22);

        Expect(bufferinglogic.getStartSegmentNumber(rep) <=
               number - DefaultBufferingLogic::SAFETY_BUFFERING_EDGE_OFFSET);
        Expect(bufferinglogic.getStartSegmentNumber(rep) >=
               22 + DefaultBufferingLogic::SAFETY_EXPURGING_OFFSET);

        delete playlist;
    } catch(...) {
        delete playlist;
        return 1;
    }

    return 0;
}
