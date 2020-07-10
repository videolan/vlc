/*
 * Representation.cpp
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

#include "Representation.hpp"
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

Representation::Representation  ( BaseAdaptationSet *set ) :
                BaseRepresentation( set )
{
    b_live = true;
    b_loaded = false;
    b_failed = false;
    lastUpdateTime = 0;
    targetDuration = 0;
    streamFormat = StreamFormat::UNKNOWN;
}

Representation::~Representation ()
{
}

StreamFormat Representation::getStreamFormat() const
{
    return streamFormat;
}

bool Representation::isLive() const
{
    return b_live;
}

bool Representation::initialized() const
{
    return b_loaded;
}

void Representation::setPlaylistUrl(const std::string &uri)
{
    playlistUrl = Url(uri);
}

Url Representation::getPlaylistUrl() const
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

void Representation::debug(vlc_object_t *obj, int indent) const
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

void Representation::scheduleNextUpdate(uint64_t, bool b_updated)
{
    if(!isLive())
        return;

    if(!b_updated)
    {
        /* Ensure we don't update-loop if it failed */
        //lastUpdateTime = vlc_tick_now();
        return;
    }

    const vlc_tick_t now = vlc_tick_now();
    const AbstractPlaylist *playlist = getPlaylist();

    msg_Dbg(playlist->getVLCObject(), "Updated playlist ID %s, after %" PRId64 "s",
            getID().str().c_str(),
            lastUpdateTime ? SEC_FROM_VLC_TICK(now - lastUpdateTime) : 0);

    lastUpdateTime = now;

    debug(playlist->getVLCObject(), 0);
}

bool Representation::needsUpdate(uint64_t number) const
{
    if(b_failed)
        return false;
    if(!b_loaded)
        return true;
    if(isLive())
    {
        const vlc_tick_t now = vlc_tick_now();
        const vlc_tick_t elapsed = now - lastUpdateTime;
        const vlc_tick_t duration = targetDuration
                                  ? vlc_tick_from_sec(targetDuration)
                                  : VLC_TICK_FROM_SEC(2);
        if(elapsed < duration)
            return false;

        if(number != std::numeric_limits<uint64_t>::max())
        {
            vlc_tick_t minbuffer = getMinAheadTime(number);
            return ( minbuffer < duration );
        }
    }
    return false;
}

bool Representation::runLocalUpdates(SharedResources *res)
{
    AbstractPlaylist *playlist = getPlaylist();
    M3U8Parser parser(res);
    if(!parser.appendSegmentsFromPlaylistURI(playlist->getVLCObject(), this))
        b_failed = true;
    else
        b_loaded = true;
    return true;
}

uint64_t Representation::translateSegmentNumber(uint64_t num, const SegmentInformation *from) const
{
    if(consistentSegmentNumber())
        return num;
    ISegment *fromSeg = from->getSegment(INFOTYPE_MEDIA, num);
    HLSSegment *fromHlsSeg = dynamic_cast<HLSSegment *>(fromSeg);
    if(!fromHlsSeg)
        return 1;
    const vlc_tick_t utcTime = fromHlsSeg->getUTCTime() +
                               getTimescale().ToTime(fromHlsSeg->duration.Get()) / 2;

    std::vector<ISegment *> list;
    std::vector<ISegment *>::const_iterator it;
    getSegments(INFOTYPE_MEDIA, list);
    for(it=list.begin(); it != list.end(); ++it)
    {
        const HLSSegment *hlsSeg = dynamic_cast<HLSSegment *>(*it);
        if(hlsSeg)
        {
            if (hlsSeg->getUTCTime() <= utcTime || it == list.begin())
                num = hlsSeg->getSequenceNumber();
            else
                return num;
        }
    }

    return 1;
}
