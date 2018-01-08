/*
 * RateBasedAdaptationLogic.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
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

#include "RateBasedAdaptationLogic.h"
#include "Representationselectors.hpp"

#include "../playlist/BaseRepresentation.h"
#include "../http/Chunk.h"
#include "../tools/Debug.hpp"

using namespace adaptive::logic;
using namespace adaptive;

RateBasedAdaptationLogic::RateBasedAdaptationLogic  (vlc_object_t *obj) :
                          AbstractAdaptationLogic   (obj),
                          bpsAvg(0),
                          currentBps(0)
{
    usedBps = 0;
    dllength = 0;
    dlsize = 0;
    vlc_mutex_init(&lock);
}

RateBasedAdaptationLogic::~RateBasedAdaptationLogic()
{
}

BaseRepresentation *RateBasedAdaptationLogic::getNextRepresentation(BaseAdaptationSet *adaptSet, BaseRepresentation *currep)
{
    if(adaptSet == nullptr)
        return nullptr;

    vlc_mutex_lock(&lock);
    size_t availBps = currentBps + ((currep) ? currep->getBandwidth() : 0);
    vlc_mutex_unlock(&lock);
    if(availBps > usedBps)
        availBps -= usedBps;
    else
        availBps = 0;

    RepresentationSelector selector(maxwidth, maxheight);
    BaseRepresentation *rep = selector.select(adaptSet, availBps);
    if ( rep == nullptr )
    {
        rep = selector.select(adaptSet);
        if ( rep == nullptr )
            return nullptr;
    }

    return rep;
}

void RateBasedAdaptationLogic::updateDownloadRate(const ID &, size_t size,
                                                  vlc_tick_t time, vlc_tick_t)
{
    if(unlikely(time == 0))
        return;
    /* Accumulate up to observation window */
    dllength += time;
    dlsize += size;

    if(dllength < VLC_TICK_FROM_MS(250))
        return;

    const size_t bps = CLOCK_FREQ * dlsize * 8 / dllength;

    vlc_mutex_locker locker(&lock);
    bpsAvg = average.push(bps);

//    BwDebug(msg_Dbg(p_obj, "alpha1 %lf alpha0 %lf dmax %ld ds %ld", alpha,
//                    (double)deltamax / diffsum, deltamax, diffsum));
    BwDebug(msg_Dbg(p_obj, "bw estimation bps %zu -> avg %zu",
                            bps / 8000, bpsAvg / 8000));

    currentBps = bpsAvg * 3/4;
    dlsize = dllength = 0;

    BwDebug(msg_Info(p_obj, "Current bandwidth %zu KiB/s using %u%%",
                    (bpsAvg / 8000), (bpsAvg) ? (unsigned)(usedBps * 100.0 / bpsAvg) : 0));
}

void RateBasedAdaptationLogic::trackerEvent(const TrackerEvent &ev)
{
    if(ev.getType() == TrackerEvent::Type::RepresentationSwitch)
    {
        const RepresentationSwitchEvent &event =
                static_cast<const RepresentationSwitchEvent &>(ev);
        vlc_mutex_locker locker(&lock);
        if(event.prev)
            usedBps -= event.prev->getBandwidth();
        if(event.next)
            usedBps += event.next->getBandwidth();

        BwDebug(msg_Info(p_obj, "New bandwidth usage %zu KiB/s %u%%",
                        (usedBps / 8000), (bpsAvg) ? (unsigned)(usedBps * 100.0 / bpsAvg) : 0 ));
    }
}

FixedRateAdaptationLogic::FixedRateAdaptationLogic(vlc_object_t *obj, size_t bps) :
    AbstractAdaptationLogic(obj)
{
    currentBps = bps;
}

BaseRepresentation *FixedRateAdaptationLogic::getNextRepresentation(BaseAdaptationSet *adaptSet, BaseRepresentation *)
{
    if(adaptSet == nullptr)
        return nullptr;

    RepresentationSelector selector(maxwidth, maxheight);
    BaseRepresentation *rep = selector.select(adaptSet, currentBps);
    if ( rep == nullptr )
    {
        rep = selector.select(adaptSet);
        if ( rep == nullptr )
            return nullptr;
    }
    return rep;
}
