/*
 * BufferingLogic.cpp
 *****************************************************************************
 * Copyright (C) 2014 - 2020 VideoLabs, VideoLAN and VLC authors
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

#include "BufferingLogic.hpp"
#include "../playlist/BaseRepresentation.h"
#include "../playlist/BasePeriod.h"
#include "../playlist/AbstractPlaylist.hpp"
#include "../playlist/SegmentTemplate.h"
#include "../playlist/SegmentTimeline.h"
#include "../playlist/SegmentList.h"
#include "../playlist/SegmentBase.h"

#include <limits>
#include <cassert>

using namespace adaptive;
using namespace adaptive::playlist;
using namespace adaptive::logic;

const vlc_tick_t AbstractBufferingLogic::BUFFERING_LOWEST_LIMIT = VLC_TICK_FROM_SEC(2);
const vlc_tick_t AbstractBufferingLogic::DEFAULT_MIN_BUFFERING = VLC_TICK_FROM_SEC(6);
const vlc_tick_t AbstractBufferingLogic::DEFAULT_MAX_BUFFERING = VLC_TICK_FROM_SEC(30);
const vlc_tick_t AbstractBufferingLogic::DEFAULT_LIVE_BUFFERING = VLC_TICK_FROM_SEC(15);

AbstractBufferingLogic::AbstractBufferingLogic()
{
    userMinBuffering = 0;
    userMaxBuffering = 0;
    userLiveDelay = 0;
}

void AbstractBufferingLogic::setLowDelay(bool b)
{
    userLowLatency = b;
}

void AbstractBufferingLogic::setUserMinBuffering(vlc_tick_t v)
{
    userMinBuffering = v;
}

void AbstractBufferingLogic::setUserMaxBuffering(vlc_tick_t v)
{
    userMaxBuffering = v;
}

void AbstractBufferingLogic::setUserLiveDelay(vlc_tick_t v)
{
    userLiveDelay = v;
}

DefaultBufferingLogic::DefaultBufferingLogic()
    : AbstractBufferingLogic()
{

}

uint64_t DefaultBufferingLogic::getStartSegmentNumber(BaseRepresentation *rep) const
{
    if(rep->getPlaylist()->isLive())
        return getLiveStartSegmentNumber(rep);

    const MediaSegmentTemplate *segmentTemplate = rep->inheritSegmentTemplate();
    if(segmentTemplate)
    {
        const SegmentTimeline *timeline = segmentTemplate->inheritSegmentTimeline();
        if(timeline)
            return timeline->minElementNumber();
        return segmentTemplate->inheritStartNumber();
    }

    const SegmentList *list = rep->inheritSegmentList();
    if(list)
        return list->getStartIndex();

    const SegmentBase *base = rep->inheritSegmentBase();
    if(base)
        return base->getSequenceNumber();

    return 0;
}

vlc_tick_t DefaultBufferingLogic::getMinBuffering(const AbstractPlaylist *p) const
{
    if(isLowLatency(p))
        return BUFFERING_LOWEST_LIMIT;

    vlc_tick_t buffering = userMinBuffering ? userMinBuffering
                                            : DEFAULT_MIN_BUFFERING;
    if(p->getMinBuffering())
        buffering = std::max(buffering, p->getMinBuffering());
    return std::max(buffering, BUFFERING_LOWEST_LIMIT);
}

vlc_tick_t DefaultBufferingLogic::getMaxBuffering(const AbstractPlaylist *p) const
{
    if(isLowLatency(p))
        return getMinBuffering(p);

    vlc_tick_t buffering = userMaxBuffering ? userMaxBuffering
                                            : DEFAULT_MAX_BUFFERING;
    if(p->isLive())
        buffering = std::min(buffering, getLiveDelay(p));
    if(p->getMaxBuffering())
        buffering = std::min(buffering, p->getMaxBuffering());
    return std::max(buffering, getMinBuffering(p));
}

vlc_tick_t DefaultBufferingLogic::getLiveDelay(const AbstractPlaylist *p) const
{
    if(isLowLatency(p))
        return getMinBuffering(p);
    vlc_tick_t delay = userLiveDelay ? userLiveDelay
                                     : DEFAULT_LIVE_BUFFERING;
    if(p->suggestedPresentationDelay.Get())
        delay = p->suggestedPresentationDelay.Get();
    if(p->timeShiftBufferDepth.Get())
        delay = std::min(delay, p->timeShiftBufferDepth.Get());
    return std::max(delay, getMinBuffering(p));
}

uint64_t DefaultBufferingLogic::getLiveStartSegmentNumber(BaseRepresentation *rep) const
{
    AbstractPlaylist *playlist = rep->getPlaylist();

    /* Get buffering offset min <= max <= live delay */
    vlc_tick_t i_buffering = getBufferingOffset(playlist);

    /* Try to never buffer up to really end */
    /* Enforce no overlap for demuxers segments 3.0.0 */
    /* FIXME: check duration instead ? */
    const unsigned SAFETY_BUFFERING_EDGE_OFFSET = 1;
    const unsigned SAFETY_EXPURGING_OFFSET = 2;

    SegmentList *segmentList = rep->inheritSegmentList();
    SegmentBase *segmentBase = rep->inheritSegmentBase();
    MediaSegmentTemplate *mediaSegmentTemplate = rep->inheritSegmentTemplate();
    if(mediaSegmentTemplate)
    {
        uint64_t start = 0;
        const Timescale timescale = mediaSegmentTemplate->inheritTimescale();

        const SegmentTimeline *timeline = mediaSegmentTemplate->inheritSegmentTimeline();
        if(timeline)
        {
            uint64_t safeMinElementNumber = timeline->minElementNumber();
            uint64_t safeMaxElementNumber = timeline->maxElementNumber();
            stime_t safeedgetime, safestarttime, duration;
            for(unsigned i=0; i<SAFETY_BUFFERING_EDGE_OFFSET; i++)
            {
                if(safeMinElementNumber == safeMaxElementNumber)
                    break;
                safeMaxElementNumber--;
            }
            bool b_ret = timeline->getScaledPlaybackTimeDurationBySegmentNumber(safeMaxElementNumber,
                                                                                &safeedgetime, &duration);
            if(unlikely(!b_ret))
                return 0;
            safeedgetime += duration - 1;

            for(unsigned i=0; i<SAFETY_EXPURGING_OFFSET; i++)
            {
                if(safeMinElementNumber + 1 >= safeMaxElementNumber)
                    break;
                safeMinElementNumber++;
            }
            b_ret = timeline->getScaledPlaybackTimeDurationBySegmentNumber(safeMinElementNumber,
                                                                           &safestarttime, &duration);
            if(unlikely(!b_ret))
                return 0;

            if(playlist->timeShiftBufferDepth.Get())
            {
                stime_t edgetime;
                bool b_ret = timeline->getScaledPlaybackTimeDurationBySegmentNumber(timeline->maxElementNumber(),
                                                                                    &edgetime, &duration);
                if(unlikely(!b_ret))
                    return 0;
                edgetime += duration - 1;
                stime_t timeshiftdepth = timescale.ToScaled(playlist->timeShiftBufferDepth.Get());
                if(safestarttime + timeshiftdepth < edgetime)
                {
                    safestarttime = edgetime - timeshiftdepth;
                    safeMinElementNumber = timeline->getElementNumberByScaledPlaybackTime(safestarttime);
                }
            }
            assert(safestarttime<=safeedgetime);

            stime_t starttime;
            if(safeedgetime - safestarttime > timescale.ToScaled(i_buffering))
                starttime = safeedgetime - timescale.ToScaled(i_buffering);
            else
                starttime = safestarttime;

            start = timeline->getElementNumberByScaledPlaybackTime(starttime);
            assert(start >= timeline->minElementNumber());
            assert(start >= safeMinElementNumber);
            assert(start <= timeline->maxElementNumber());
            assert(start <= safeMaxElementNumber);

            return start;
        }
        /* Else compute, current time and timeshiftdepth based */
        else if(mediaSegmentTemplate->duration.Get())
        {
            /* Compute playback offset and effective finished segment from wall time */
            vlc_tick_t now = vlc_tick_from_sec(time(NULL));
            vlc_tick_t playbacktime = now - i_buffering;
            vlc_tick_t minavailtime = playlist->availabilityStartTime.Get() + rep->getPeriodStart();
            const uint64_t startnumber = mediaSegmentTemplate->inheritStartNumber();
            const Timescale timescale = mediaSegmentTemplate->inheritTimescale();
            if(!timescale)
                return startnumber;
            const vlc_tick_t duration = timescale.ToTime(mediaSegmentTemplate->inheritDuration());
            if(!duration)
                return startnumber;

            /* restrict to DVR window */
            if(playlist->timeShiftBufferDepth.Get())
            {
                vlc_tick_t elapsed = now - minavailtime;
                elapsed = elapsed - (elapsed % duration); /* align to last segment */
                vlc_tick_t alignednow = minavailtime + elapsed;
                if(playlist->timeShiftBufferDepth.Get() < elapsed)
                    minavailtime = alignednow - playlist->timeShiftBufferDepth.Get();

                if(playbacktime < minavailtime)
                    playbacktime = minavailtime;
            }
            /* Get completed segment containing the time ref */
            start = mediaSegmentTemplate->getLiveTemplateNumber(playbacktime);
            if (unlikely(start < startnumber))
            {
                assert(startnumber > start); /* blame getLiveTemplateNumber() */
                start = startnumber;
            }

            const uint64_t max_safety_offset = playbacktime - minavailtime / duration;
            const uint64_t safety_offset = std::min((uint64_t)SAFETY_BUFFERING_EDGE_OFFSET,
                                                    max_safety_offset);
            if(startnumber + safety_offset <= start)
                start -= safety_offset;
            else
                start = startnumber;

            return start;
        }
    }
    else if (segmentList && !segmentList->getSegments().empty())
    {
        const Timescale timescale = segmentList->inheritTimescale();
        const std::vector<ISegment *> list = segmentList->getSegments();
        const ISegment *back = list.back();

        /* working around HLS discontinuities by using durations */
        stime_t totallistduration = 0;
        for(auto it = list.begin(); it != list.end(); ++it)
            totallistduration += (*it)->duration.Get();

        /* Apply timeshift restrictions */
        stime_t availableduration;
        if(playlist->timeShiftBufferDepth.Get())
        {
            availableduration = std::min(totallistduration,
                    timescale.ToScaled(playlist->timeShiftBufferDepth.Get()));
        }
        else availableduration = totallistduration;

        uint64_t availableliststartnumber = list.front()->getSequenceNumber();
        if(totallistduration != availableduration)
        {
            stime_t offset = totallistduration - availableduration;
            for(auto it = list.begin(); it != list.end(); ++it)
            {
                availableliststartnumber = (*it)->getSequenceNumber();
                if(offset < (*it)->duration.Get())
                    break;
                offset -= (*it)->duration.Get();
            }
        }

        uint64_t safeedgenumber = back->getSequenceNumber() -
                        std::min((uint64_t)list.size() - 1,
                                 (uint64_t)SAFETY_BUFFERING_EDGE_OFFSET);

        uint64_t safestartnumber = availableliststartnumber;
        if(safeedgenumber > safestartnumber)
            safestartnumber -= std::min(safeedgenumber-safestartnumber - 1,
                                        (uint64_t)SAFETY_EXPURGING_OFFSET);

        stime_t maxbufferizable = 0;
        stime_t safeedgeduration = 0;
        for(auto it = list.begin(); it != list.end(); ++it)
        {
            if((*it)->getSequenceNumber() < safestartnumber)
                continue;
            if((*it)->getSequenceNumber() <= safeedgenumber)
                maxbufferizable += (*it)->duration.Get();
            else
                safeedgeduration += (*it)->duration.Get();
        }

        stime_t tobuffer = std::min(maxbufferizable, timescale.ToScaled(i_buffering));
        stime_t skipduration = totallistduration - safeedgeduration - tobuffer ;
        uint64_t start = safestartnumber;
        for(auto it = list.begin(); it != list.end(); ++it)
        {
            start = (*it)->getSequenceNumber();
            if((*it)->duration.Get() > skipduration)
                break;
            skipduration -= (*it)->duration.Get();
        }

        return start;
    }
    else if(segmentBase)
    {
        const std::vector<ISegment *> list = segmentBase->subSegments();
        if(!list.empty())
            return segmentBase->getSequenceNumber();

        const Timescale timescale = rep->inheritTimescale();
        const ISegment *back = list.back();
        const stime_t bufferingstart = back->startTime.Get() + back->duration.Get() -
                                       timescale.ToScaled(i_buffering);

        uint64_t start;
        if(!SegmentInfoCommon::getSegmentNumberByScaledTime(list, bufferingstart, &start))
            return list.front()->getSequenceNumber();

        if(segmentBase->getSequenceNumber() + SAFETY_BUFFERING_EDGE_OFFSET <= start)
            start -= SAFETY_BUFFERING_EDGE_OFFSET;
        else
            start = segmentBase->getSequenceNumber();

        return start;
    }

    return std::numeric_limits<uint64_t>::max();
}

vlc_tick_t DefaultBufferingLogic::getBufferingOffset(const AbstractPlaylist *p) const
{
    return p->isLive() ? getLiveDelay(p) : getMaxBuffering(p);
}

bool DefaultBufferingLogic::isLowLatency(const AbstractPlaylist *p) const
{
    if(userLowLatency.isSet())
        return userLowLatency.value();
    return p->isLowLatency();
}
