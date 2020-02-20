/*
 * NearOptimalAdaptationLogic.cpp
 *****************************************************************************
 * Copyright (C) 2017 - VideoLAN Authors
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

#include "NearOptimalAdaptationLogic.hpp"
#include "Representationselectors.hpp"

#include "../playlist/BaseAdaptationSet.h"
#include "../playlist/BaseRepresentation.h"
#include "../playlist/BasePeriod.h"
#include "../http/Chunk.h"
#include "../tools/Debug.hpp"

#include <cmath>

using namespace adaptive::logic;
using namespace adaptive;

/*
 * Multi stream version of BOLA: Near-Optimal Bitrate Adaptation for Online Videos
 * http://arxiv.org/abs/1601.06748
 */

#define minimumBufferS VLC_TICK_FROM_SEC(6)  /* Qmin */
#define bufferTargetS  VLC_TICK_FROM_SEC(30) /* Qmax */

NearOptimalContext::NearOptimalContext()
    : buffering_min( minimumBufferS )
    , buffering_level( 0 )
    , buffering_target( bufferTargetS )
    , last_download_rate( 0 )
{ }

NearOptimalAdaptationLogic::NearOptimalAdaptationLogic(vlc_object_t *obj)
    : AbstractAdaptationLogic(obj)
    , currentBps( 0 )
    , usedBps( 0 )
{
    vlc_mutex_init(&lock);
}

NearOptimalAdaptationLogic::~NearOptimalAdaptationLogic()
{
}

BaseRepresentation *
NearOptimalAdaptationLogic::getNextQualityIndex( BaseAdaptationSet *adaptSet, RepresentationSelector &selector,
                                                 float gammaP, float VD, float Q )
{
    BaseRepresentation *ret = NULL;
    BaseRepresentation *prev = NULL;
    float argmax;
    for(BaseRepresentation *rep = selector.lowest(adaptSet);
                            rep && rep != prev; rep = selector.higher(adaptSet, rep))
    {
        float arg = ( VD * (getUtility(rep) + gammaP) - Q ) / rep->getBandwidth();
        if(ret == NULL || argmax <= arg)
        {
            ret = rep;
            argmax = arg;
        }
        prev = rep;
    }
    return ret;
}

BaseRepresentation *NearOptimalAdaptationLogic::getNextRepresentation(BaseAdaptationSet *adaptSet, BaseRepresentation *prevRep)
{
    RepresentationSelector selector(maxwidth, maxheight);

    const float umin = getUtility(selector.lowest(adaptSet));
    const float umax = getUtility(selector.highest(adaptSet));

    vlc_mutex_lock(&lock);

    std::map<ID, NearOptimalContext>::iterator it = streams.find(adaptSet->getID());
    if(it == streams.end())
    {
        vlc_mutex_unlock(&lock);
        return selector.lowest(adaptSet);
    }
    NearOptimalContext ctxcopy = (*it).second;

    const unsigned bps = getAvailableBw(currentBps, prevRep);

    vlc_mutex_unlock(&lock);

    const float gammaP = 1.0 + (umax - umin) / ((float)ctxcopy.buffering_target / ctxcopy.buffering_min - 1.0);
    const float Vd = (secf_from_vlc_tick(ctxcopy.buffering_min) - 1.0) / (umin + gammaP);

    BaseRepresentation *m;
    if(prevRep == NULL) /* Starting */
    {
        m = selector.select(adaptSet, bps);
    }
    else
    {
        /* noted m* */
        m = getNextQualityIndex(adaptSet, selector, gammaP - umin /* umin == Sm, utility = std::log(S/Sm) */,
                                Vd, secf_from_vlc_tick(ctxcopy.buffering_level));
        if(m->getBandwidth() < prevRep->getBandwidth()) /* m*[n] < m*[n-1] */
        {
            BaseRepresentation *mp = selector.select(adaptSet, bps); /* m' */
            if(mp->getBandwidth() <= m->getBandwidth())
            {
                mp = m;
            }
            else if(mp->getBandwidth() > prevRep->getBandwidth())
            {
                mp = prevRep;
            }
            else
            {
                mp = selector.lower(adaptSet, mp);
            }
            m = mp;
        }
    }

    BwDebug( msg_Info(p_obj, "buffering level %.2f% rep %ld kBps %zu kBps",
             (float) 100 * ctxcopy.buffering_level / ctxcopy.buffering_target, m->getBandwidth()/8000, bps / 8000); );

    return m;
}

float NearOptimalAdaptationLogic::getUtility(const BaseRepresentation *rep)
{
    float ret;
    std::map<uint64_t, float>::iterator it = utilities.find(rep->getBandwidth());
    if(it == utilities.end())
    {
        ret = std::log((float)rep->getBandwidth());
        utilities.insert(std::pair<uint64_t, float>(rep->getBandwidth(), ret));
    }
    else ret = (*it).second;
    return ret;
}

unsigned NearOptimalAdaptationLogic::getAvailableBw(unsigned i_bw, const BaseRepresentation *curRep) const
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

unsigned NearOptimalAdaptationLogic::getMaxCurrentBw() const
{
    unsigned i_max_bitrate = 0;
    for(std::map<ID, NearOptimalContext>::const_iterator it = streams.begin();
                                                         it != streams.end(); ++it)
        i_max_bitrate = std::max(i_max_bitrate, ((*it).second).last_download_rate);
    return i_max_bitrate;
}

void NearOptimalAdaptationLogic::updateDownloadRate(const ID &id, size_t dlsize, vlc_tick_t time)
{
    vlc_mutex_lock(&lock);
    std::map<ID, NearOptimalContext>::iterator it = streams.find(id);
    if(it != streams.end())
    {
        NearOptimalContext &ctx = (*it).second;
        ctx.last_download_rate = ctx.average.push(CLOCK_FREQ * dlsize * 8 / time);
    }
    currentBps = getMaxCurrentBw();
    vlc_mutex_unlock(&lock);
}

void NearOptimalAdaptationLogic::trackerEvent(const SegmentTrackerEvent &event)
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
                 BwDebug(msg_Info(p_obj, "New total bandwidth usage %zu kBps", (usedBps / 8000)));
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
                    NearOptimalContext ctx;
                    streams.insert(std::pair<ID, NearOptimalContext>(id, ctx));
                }
            }
            else
            {
                std::map<ID, NearOptimalContext>::iterator it = streams.find(id);
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
            NearOptimalContext &ctx = streams[id];
            ctx.buffering_level = event.u.buffering_level.current;
            ctx.buffering_target = event.u.buffering_level.target;
            vlc_mutex_unlock(&lock);
        }
        break;

    default:
            break;
    }
}
