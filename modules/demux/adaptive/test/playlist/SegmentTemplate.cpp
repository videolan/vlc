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
#include "../../playlist/SegmentTemplate.h"
#include "../../playlist/SegmentTimeline.h"
#include "../../playlist/BasePlaylist.hpp"
#include "../../playlist/BasePeriod.h"
#include "../../playlist/Inheritables.hpp"
#include "../../../dash/mpd/AdaptationSet.h"
#include "../../../dash/mpd/Representation.h"

#include "../test.hpp"

#include <limits>

using namespace adaptive;
using namespace adaptive::playlist;
using namespace dash::mpd;

int SegmentTemplate_test()
{
    BasePlaylist *pl = nullptr;
    try
    {
        pl = new BasePlaylist(nullptr);
        BasePeriod *period = nullptr;
        AdaptationSet *set = nullptr;
        Representation *rep = nullptr;
        try
        {
            period = new BasePeriod(pl);
            set = new AdaptationSet(period);
            rep = new Representation(set);
        } catch(...) {
            delete period;
            delete set;
            std::rethrow_exception(std::current_exception());
        }
        set->addRepresentation(rep);
        period->addAdaptationSet(set);
        pl->addPeriod(period);

        Timescale timescale(100);
        rep->addAttribute(new TimescaleAttr(timescale));
        SegmentTemplate *templ = new SegmentTemplate(new SegmentTemplateSegment(), rep);
        rep->addAttribute(templ);
        templ->addAttribute(new StartnumberAttr(11));
        std::string res = rep->contextualize(0, "$Number$.m4v", templ);
        Expect(res == "0.m4v");

        /* No timeline, no start/end, no segment duration known */
        Expect(templ->getStartSegmentNumber() == 11);
        //Expect(templ->getMediaSegment(11) == nullptr);
        uint64_t number;
        Expect(templ->getSegmentNumberByTime(timescale.ToTime(0), &number) == false);
        vlc_tick_t time, duration;
        //Expect(templ->getPlaybackTimeDurationBySegmentNumber(11, &time, &duration) == false);
        Expect(templ->getMinAheadTime(11) == 0);

        /* No timeline, no start/end, duration known */
        rep->addAttribute(new DurationAttr(100));
        Expect(templ->getSegmentNumberByTime(timescale.ToTime(500), &number) == true);
        Expect(number == 11 + 5);
        Expect(templ->getPlaybackTimeDurationBySegmentNumber(11 + 2, &time, &duration) == true);
        Expect(time == timescale.ToTime(2 * 100));
        Expect(duration == timescale.ToTime(100));
        Expect(templ->getMediaSegment(11 + 2) != nullptr);

        /* start/end, duration known */
        vlc_tick_t now = timescale.ToTime(1000000);
        pl->availabilityStartTime.Set(now);
        pl->availabilityEndTime.Set(now + timescale.ToTime(100 * 20));
        Expect(templ->getLiveTemplateNumber(now, true) == templ->getStartSegmentNumber());
        //Expect(templ->getLiveTemplateNumber(now / 2, true) == std::numeric_limits<uint64_t>::max());
        Expect(templ->getLiveTemplateNumber(now + timescale.ToTime(100) * 2 + 1, true) ==
               templ->getStartSegmentNumber() + 1);

        /* reset */
        pl->availabilityStartTime.Set(0);
        pl->availabilityEndTime.Set(0);

        /* timeline */
        const stime_t START = 1337;
        SegmentTimeline *timeline = new SegmentTimeline(nullptr);
        templ->addAttribute(timeline);
        timeline->addElement(44,     100, 4, START);
        timeline->addElement(44 + 5,  33, 4, START + 5 * 100);
        Expect(templ->getMediaSegment(44 - 1) == nullptr);
        Expect(templ->getMediaSegment(44 + 5 + 4) != nullptr);
        Expect(templ->getMediaSegment(44 + 5 + 5) == nullptr);
        Expect(templ->getStartSegmentNumber() == 44);
        Expect(templ->getSegmentNumberByTime(timescale.ToTime(START + 5 * 100 + 2), &number) == true);
        Expect(number == 44 + 5);
        Expect(templ->getPlaybackTimeDurationBySegmentNumber(44 + 6, &time, &duration) == true);
        Expect(time == timescale.ToTime(START + 5 * 100 + 33));
        Expect(duration == timescale.ToTime(33));
        Expect(templ->getMinAheadTime(44+6) == timescale.ToTime(3 * 33));

        delete pl;

    } catch (...) {
        delete pl;
        return 1;
    }

    return 0;
}
