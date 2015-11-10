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
#include "../playlist/BasePeriod.h"
#include "../http/Chunk.h"
#include "../tools/Debug.hpp"

using namespace adaptative::logic;

RateBasedAdaptationLogic::RateBasedAdaptationLogic  (vlc_object_t *p_obj_, int w, int h) :
                          AbstractAdaptationLogic   (),
                          bpsAvg(0), bpsRemainder(0), bpsSamplecount(0),
                          currentBps(0)
{
    width  = w;
    height = h;
    usedBps = 0;
    p_obj = p_obj_;
}

BaseRepresentation *RateBasedAdaptationLogic::getNextRepresentation(BaseAdaptationSet *adaptSet, BaseRepresentation *currep) const
{
    if(adaptSet == NULL)
        return NULL;

    size_t availBps = currentBps + ((currep) ? currep->getBandwidth() : 0);
    if(availBps > usedBps)
        availBps -= usedBps;
    else
        availBps = 0;

    RepresentationSelector selector;
    BaseRepresentation *rep = selector.select(adaptSet, availBps, width, height);
    if ( rep == NULL )
    {
        rep = selector.select(adaptSet);
        if ( rep == NULL )
            return NULL;
    }

    return rep;
}

void RateBasedAdaptationLogic::updateDownloadRate(size_t size, mtime_t time)
{
    if(unlikely(time == 0) || size < (HTTPChunkSource::CHUNK_SIZE>>1) )
        return;

    size_t current = bpsRemainder + CLOCK_FREQ * size * 8 / time;

    if (current >= bpsAvg)
    {
        bpsAvg += (current - bpsAvg) / ++bpsSamplecount;
        bpsRemainder = (current - bpsAvg) % bpsSamplecount;
    }
    else
    {
        bpsAvg -= (bpsAvg - current) / ++bpsSamplecount;
        bpsRemainder = (bpsAvg - current) % bpsSamplecount;
    }

    currentBps = bpsAvg * 3/4;

    BwDebug(msg_Info(p_obj, "Current bandwidth %zu KiB/s using %u%%",
                    (bpsAvg / 8192), (bpsAvg) ? (unsigned)(usedBps * 100.0 / bpsAvg) : 0 ));
}

void RateBasedAdaptationLogic::trackerEvent(const SegmentTrackerEvent &event)
{
    if(event.type == SegmentTrackerEvent::SWITCHING)
    {
        if(event.u.switching.prev)
            usedBps -= event.u.switching.prev->getBandwidth();
        if(event.u.switching.next)
            usedBps += event.u.switching.next->getBandwidth();

        BwDebug(msg_Info(p_obj, "New bandwidth usage %zu KiB/s %u%%",
                        (usedBps / 8192), (bpsAvg) ? (unsigned)(usedBps * 100.0 / bpsAvg) : 0 ));
    }
}

FixedRateAdaptationLogic::FixedRateAdaptationLogic(size_t bps) :
    AbstractAdaptationLogic()
{
    currentBps = bps;
}

BaseRepresentation *FixedRateAdaptationLogic::getNextRepresentation(BaseAdaptationSet *adaptSet, BaseRepresentation *) const
{
    if(adaptSet == NULL)
        return NULL;

    RepresentationSelector selector;
    BaseRepresentation *rep = selector.select(adaptSet, currentBps);
    if ( rep == NULL )
    {
        rep = selector.select(adaptSet);
        if ( rep == NULL )
            return NULL;
    }
    return rep;
}
