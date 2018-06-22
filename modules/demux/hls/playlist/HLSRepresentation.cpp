/*
 * HLSRepresentation.cpp
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC Authors
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

#include <vlc_fixups.h>
#include <cinttypes>

#include "HLSRepresentation.hpp"
#include "M3U8.hpp"
#include "Parser.hpp"
#include "HLSSegment.hpp"
#include "../../adaptive/playlist/BasePeriod.h"
#include "../../adaptive/playlist/BaseAdaptationSet.h"
#include "../../adaptive/playlist/SegmentList.h"

#include <ctime>
#include <limits>
#include <cassert>

using namespace hls;
using namespace hls::playlist;

HLSRepresentation::HLSRepresentation  ( BaseAdaptationSet *set ) :
                BaseRepresentation( set )
{
    b_live = true;
    b_loaded = false;
    updateFailureCount = 0;
    lastUpdateTime = 0;
    targetDuration = 0;
    streamFormat = StreamFormat::Type::Unknown;
    channels = 0;
}

HLSRepresentation::~HLSRepresentation ()
{
}

StreamFormat HLSRepresentation::getStreamFormat() const
{
    return streamFormat;
}

bool HLSRepresentation::isLive() const
{
    return b_live;
}

bool HLSRepresentation::initialized() const
{
    return b_loaded;
}

void HLSRepresentation::setPlaylistUrl(const std::string &uri)
{
    playlistUrl = Url(uri);
}

Url HLSRepresentation::getPlaylistUrl() const
{
    if(playlistUrl.hasScheme())
    {
        return playlistUrl;
    }
    else
    {
        Url ret = getParentUrlSegment();
        if(!playlistUrl.empty())
            ret.append(playlistUrl);
        return ret;
    }
}

void HLSRepresentation::debug(vlc_object_t *obj, int indent) const
{
    BaseRepresentation::debug(obj, indent);
    if(!b_loaded)
    {
        std::string text(indent + 1, ' ');
        text.append(" (not loaded) ");
        text.append(getStreamFormat().str());
        msg_Dbg(obj, "%s", text.c_str());
    }
}

void HLSRepresentation::scheduleNextUpdate(uint64_t, bool b_updated)
{
    if(!isLive())
        return;

    if(!b_updated)
    {
        /* Ensure we don't update-loop if it failed */
        //lastUpdateTime = vlc_tick_now();
        return;
    }

    const vlc_tick_t now = mdate();
    const BasePlaylist *playlist = getPlaylist();

    msg_Dbg(playlist->getVLCObject(), "Updated playlist ID %s, after %" PRId64 "s",
            getID().str().c_str(),
            lastUpdateTime ? (now - lastUpdateTime)/CLOCK_FREQ : 0);

    lastUpdateTime = now;

    debug(playlist->getVLCObject(), 0);
}

bool HLSRepresentation::needsUpdate(uint64_t number) const
{
    if(updateFailureCount > MAX_UPDATE_FAILED_UPDATE_COUNT)
        return false;
    if(!b_loaded)
        return true;
    if(isLive())
    {
        const vlc_tick_t now = mdate();
        const vlc_tick_t elapsed = now - lastUpdateTime;
        vlc_tick_t duration = targetDuration
                         ? CLOCK_FREQ * targetDuration
                         : CLOCK_FREQ * 2;
        if(updateFailureCount)
            duration /= 2;
        if(elapsed < duration)
            return false;

        if(number == std::numeric_limits<uint64_t>::max())
            return true;

        vlc_tick_t minbuffer = getMinAheadTime(number);
        return ( minbuffer < duration );
    }
    return false;
}

bool HLSRepresentation::runLocalUpdates(SharedResources *res)
{
    BasePlaylist *playlist = getPlaylist();
    M3U8Parser parser(res);
    if(!parser.appendSegmentsFromPlaylistURI(playlist->getVLCObject(), this))
    {
        msg_Warn(playlist->getVLCObject(), "Failed to update %u/%u playlist ID %s",
                 updateFailureCount, MAX_UPDATE_FAILED_UPDATE_COUNT,
                 getID().str().c_str());
        updateFailureCount++;
        lastUpdateTime = mdate();
        return false;
    }
    else
    {
        updateFailureCount = 0;
        b_loaded = true;
        return true;
    }
}

bool HLSRepresentation::canNoLongerUpdate() const
{
    return updateFailureCount > MAX_UPDATE_FAILED_UPDATE_COUNT;
}

void HLSRepresentation::setChannelsCount(unsigned c)
{
    channels = c;
}

CodecDescription * HLSRepresentation::makeCodecDescription(const std::string &s) const
{
    CodecDescription *desc = BaseRepresentation::makeCodecDescription(s);
    if(desc)
    {
        desc->setChannelsCount(channels);
    }
    return desc;
}

uint64_t HLSRepresentation::translateSegmentNumber(uint64_t num, const BaseRepresentation *from) const
{
    if(targetDuration == static_cast<const HLSRepresentation *>(from)->targetDuration)
        return num;

    ISegment *fromSeg = from->getMediaSegment(num);
    const SegmentList *segmentList = inheritSegmentList();
    if(!fromSeg || !segmentList)
        return std::numeric_limits<uint64_t>::max();

    const uint64_t discontinuitySequence = fromSeg->getDiscontinuitySequenceNumber();

    if(!segmentList->hasRelativeMediaTimes())
    {
        const stime_t wantedTimeIn = fromSeg->startTime.Get();
        const stime_t wantedTimeOut = wantedTimeIn + fromSeg->duration.Get();

        const std::vector<Segment *> &list = segmentList->getSegments();
        std::vector<Segment *>::const_iterator it;
        for(it=list.begin(); it != list.end(); ++it)
        {
            const ISegment *seg = *it;
            /* Must be in the same sequence */
            if(seg->getDiscontinuitySequenceNumber() < discontinuitySequence)
                continue;
            const stime_t segTimeIn = seg->startTime.Get();
            const stime_t segTimeOut = segTimeIn + seg->duration.Get();
            if(wantedTimeIn >= segTimeIn && wantedTimeIn < segTimeOut)
                return seg->getSequenceNumber();
            /* approx / gap */
            if(wantedTimeOut >= segTimeIn && wantedTimeOut < segTimeOut)
                return seg->getSequenceNumber();
        }
    }
    else if(segmentList->getTotalLength())
    {
        const SegmentList *fromList = inheritSegmentList();
        if(fromList)
        {
            stime_t length = fromList->getTotalLength();
            stime_t first = fromList->getSegments().front()->startTime.Get();
            stime_t now = fromSeg->startTime.Get();
            double relpos = ((double)(now - first)) / length;

            const std::vector<Segment *> &list = segmentList->getSegments();
            stime_t lookup = list.front()->startTime.Get() +
                             segmentList->getTotalLength() * relpos;
            std::vector<Segment *>::const_iterator it;
            for(it=list.begin(); it != list.end(); ++it)
            {
                const ISegment *seg = *it;
                /* Must be in the same sequence */
                if(seg->getDiscontinuitySequenceNumber() < discontinuitySequence)
                    continue;
                const stime_t segTimeIn = seg->startTime.Get();
                const stime_t segTimeOut = segTimeIn + seg->duration.Get();
                if(lookup >= segTimeIn && lookup < segTimeOut)
                    return seg->getSequenceNumber();
            }
        }
    }

    return std::numeric_limits<uint64_t>::max();
}
