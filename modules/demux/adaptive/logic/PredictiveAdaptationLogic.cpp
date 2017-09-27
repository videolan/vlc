/*
 * PredictiveAdaptationLogic.cpp
 *****************************************************************************
 * Copyright (C) 2016 - VideoLAN Authors
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

#include "PredictiveAdaptationLogic.hpp"

#include "Representationselectors.hpp"

#include "../playlist/BaseAdaptationSet.h"
#include "../playlist/BaseRepresentation.h"
#include "../playlist/BasePeriod.h"
#include "../http/Chunk.h"
#include "../tools/Debug.hpp"

using namespace adaptive::logic;
using namespace adaptive;

/*
 * Modified PBA algorithm (streaming for Cellular Networks) for multi streams
 * Targets optimal quality
 * https://www.cs.princeton.edu/~jrex/papers/hotmobile15.pdf
 */

PredictiveStats::PredictiveStats()
{
    segments_count = 0;
    buffering_level = 0;
    buffering_target = 1;
    last_download_rate = 0;
    last_duration = 1;
}

bool PredictiveStats::starting() const
{
    return (segments_count < 3) || !last_download_rate;
}

PredictiveAdaptationLogic::PredictiveAdaptationLogic(vlc_object_t *p_obj_)
    : AbstractAdaptationLogic()
{
    p_obj = p_obj_;
    usedBps = 0;
    vlc_mutex_init(&lock);
}

PredictiveAdaptationLogic::~PredictiveAdaptationLogic()
{
    vlc_mutex_destroy(&lock);
}

BaseRepresentation *PredictiveAdaptationLogic::getNextRepresentation(BaseAdaptationSet *adaptSet, BaseRepresentation *prevRep)
{
    RepresentationSelector selector(maxwidth, maxheight);
    BaseRepresentation *rep;

    vlc_mutex_lock(&lock);

    std::map<ID, PredictiveStats>::iterator it = streams.find(adaptSet->getID());
    if(it == streams.end())
    {
        rep = selector.highest(adaptSet);
    }
    else
    {
        PredictiveStats &stats = (*it).second;

        double f_buffering_level = (double)stats.buffering_level / stats.buffering_target;
        double f_min_buffering_level = f_buffering_level;
        unsigned i_max_bitrate = 0;
        if(streams.size() > 1)
        {
            std::map<ID, PredictiveStats>::const_iterator it2 = streams.begin();
            for(; it2 != streams.end(); ++it2)
            {
                if(it2 == it)
                    continue;

                const PredictiveStats &other = (*it2).second;
                f_min_buffering_level = std::min((double)other.buffering_level / other.buffering_target,
                                                 f_min_buffering_level);
                i_max_bitrate = std::max(i_max_bitrate, other.last_download_rate);
            }
        }

        if(stats.starting())
        {
            rep = selector.highest(adaptSet);
        }
        else
        {
            const unsigned i_available_bw = getAvailableBw(i_max_bitrate, prevRep);
            if(!prevRep)
            {
                rep = selector.select(adaptSet, i_available_bw);
            }
            else if(f_buffering_level > 0.8)
            {
                rep = selector.select(adaptSet, std::max((uint64_t) i_available_bw,
                                                         (uint64_t) prevRep->getBandwidth()));
            }
            else if(f_buffering_level > 0.5)
            {
                rep = prevRep;
            }
            else
            {
                if(f_buffering_level > 2 * stats.last_duration)
                {
                    rep = selector.lower(adaptSet, prevRep);
                }
                else
                {
                    rep = selector.select(adaptSet, i_available_bw * f_buffering_level);
                }
            }
        }

        BwDebug( for(it=streams.begin(); it != streams.end(); ++it)
        {
            const PredictiveStats &s = (*it).second;
            msg_Info(p_obj, "Stream %s buffering level %.2f%",
                 (*it).first.str().c_str(), (double) s.buffering_level / s.buffering_target);
        } );

        BwDebug( if( rep != prevRep )
                    msg_Info(p_obj, "Stream %s new bandwidth usage %zu KiB/s",
                         adaptSet->getID().str().c_str(), rep->getBandwidth() / 8000); );

        stats.segments_count++;
    }

    vlc_mutex_unlock(&lock);

    return rep;
}

void PredictiveAdaptationLogic::updateDownloadRate(const ID &id, size_t dlsize, mtime_t time)
{
    vlc_mutex_lock(&lock);
    std::map<ID, PredictiveStats>::iterator it = streams.find(id);
    if(it != streams.end())
    {
        PredictiveStats &stats = (*it).second;
        stats.last_download_rate = stats.average.push(CLOCK_FREQ * dlsize * 8 / time);
    }
    vlc_mutex_unlock(&lock);
}

unsigned PredictiveAdaptationLogic::getAvailableBw(unsigned i_bw, const BaseRepresentation *curRep) const
{
    unsigned i_remain = i_bw;
    if(i_remain > usedBps)
        i_remain -= usedBps;
    else
        i_remain = 0;
    if(curRep)
        i_remain += curRep->getBandwidth();
    return i_remain > i_bw ? i_remain : i_bw;
}

void PredictiveAdaptationLogic::trackerEvent(const SegmentTrackerEvent &event)
{
    switch(event.type)
    {
    case SegmentTrackerEvent::SWITCHING:
        {
            vlc_mutex_lock(&lock);
            if(event.u.switching.prev)
                usedBps -= event.u.switching.prev->getBandwidth();
            if(event.u.switching.next)
                usedBps += event.u.switching.next->getBandwidth();

            BwDebug(msg_Info(p_obj, "New total bandwidth usage %zu KiB/s", (usedBps / 8000)));
            vlc_mutex_unlock(&lock);
        }
        break;

    case SegmentTrackerEvent::BUFFERING_STATE:
        {
            const ID &id = *event.u.buffering.id;
            vlc_mutex_lock(&lock);
            if(event.u.buffering.enabled)
            {
                if(streams.find(id) == streams.end())
                {
                    PredictiveStats stats;
                    streams.insert(std::pair<ID, PredictiveStats>(id, stats));
                }
            }
            else
            {
                std::map<ID, PredictiveStats>::iterator it = streams.find(id);
                if(it != streams.end())
                    streams.erase(it);
            }
            vlc_mutex_unlock(&lock);
            BwDebug(msg_Info(p_obj, "Stream %s is now known %sactive", id.str().c_str(),
                             (event.u.buffering.enabled) ? "" : "in"));
        }
        break;

    case SegmentTrackerEvent::BUFFERING_LEVEL_CHANGE:
        {
            const ID &id = *event.u.buffering.id;
            vlc_mutex_lock(&lock);
            PredictiveStats &stats = streams[id];
            stats.buffering_level = event.u.buffering_level.current;
            stats.buffering_target = event.u.buffering_level.target;
            vlc_mutex_unlock(&lock);
        }
        break;

    case SegmentTrackerEvent::SEGMENT_CHANGE:
        {
            const ID &id = *event.u.segment.id;
            vlc_mutex_lock(&lock);
            PredictiveStats &stats = streams[id];
            stats.last_duration = event.u.segment.duration;
            vlc_mutex_unlock(&lock);
        }
        break;

    default:
            break;
    }
}
