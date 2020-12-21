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

#include "../test.hpp"

using namespace adaptive;
using namespace adaptive::playlist;

int Inheritables_test()
{
    BasePeriod *period = nullptr;
    try
    {
        period = new BasePeriod(nullptr);
        BaseAdaptationSet *set = nullptr;
        BaseRepresentation *rep = nullptr;
        try
        {
            set = new BaseAdaptationSet(period);
            rep = new BaseRepresentation(set);
        } catch(...) {
            delete set;
            std::rethrow_exception(std::current_exception());
        }
        set->addRepresentation(rep);
        period->addAdaptationSet(set);

        Expect(rep->inheritAttribute(AbstractAttr::Type::SegmentBase) == nullptr);
        period->addAttribute(new TimescaleAttr(Timescale(123)));

        const AbstractAttr *attr = rep->inheritAttribute(AbstractAttr::Type::Timescale);
        Expect(attr != nullptr);
        Expect(attr->isValid());
        Timescale timescale = rep->inheritTimescale();
        Expect(timescale == 123);

        set->addAttribute(new TimescaleAttr(Timescale(1230)));
        timescale = rep->inheritTimescale();
        Expect(timescale == 1230);

        set->replaceAttribute(new TimescaleAttr(Timescale()));
        timescale = rep->inheritTimescale();
        Expect(timescale == 123);

        delete period;
    } catch(...) {
        delete period;
        return 1;
    }

    try
    {
        /* Check inheritance from siblings */
        period = new BasePeriod(nullptr);
        BaseAdaptationSet *set = nullptr;
        BaseRepresentation *rep = nullptr;
        try
        {
            set = new BaseAdaptationSet(period);
            rep = new BaseRepresentation(set);
        } catch(...) {
            delete set;
            std::rethrow_exception(std::current_exception());
        }
        set->addRepresentation(rep);
        period->addAdaptationSet(set);

        SegmentTemplate *templ = new SegmentTemplate(new SegmentTemplateSegment(), rep);
        rep->addAttribute(templ);
        SegmentTimeline *timeline = new SegmentTimeline(templ);
        templ->addAttribute(timeline);

        templ = new SegmentTemplate(new SegmentTemplateSegment(), period);
        period->addAttribute(templ);
        templ->addAttribute(new TimescaleAttr(Timescale(123)));
        timeline = new SegmentTimeline(templ);
        timeline->addAttribute(new TimescaleAttr(Timescale(456)));
        templ->addAttribute(timeline);

        /* check inheritance from matched sibling */
        const AbstractAttr *attr = rep->inheritAttribute(AbstractAttr::Type::Timescale);
        Expect(attr == nullptr);
        Timescale timescale = timeline->inheritTimescale();
        Expect(timescale == 456);
        timeline->replaceAttribute(new TimescaleAttr(Timescale())); /* invalidate on timeline */
        /* should now inherit from sibling parent template */
        timescale = timeline->inheritTimescale();
        Expect(timescale == 123);

        delete period;
    } catch(...) {
        delete period;
        return 1;
    }

    return 0;
}
