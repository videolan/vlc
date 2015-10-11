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

#include <cstdlib>

#include "Representation.hpp"
#include "M3U8.hpp"
#include "Parser.hpp"
#include "../adaptative/playlist/BasePeriod.h"
#include "../adaptative/playlist/BaseAdaptationSet.h"
#include "../adaptative/playlist/SegmentList.h"
#include "../HLSStreamFormat.hpp"

#include <ctime>

using namespace hls;
using namespace hls::playlist;

Representation::Representation  ( BaseAdaptationSet *set ) :
                BaseRepresentation( set )
{
    b_live = true;
    b_loaded = false;
    switchpolicy = SegmentInformation::SWITCH_SEGMENT_ALIGNED; /* FIXME: based on streamformat */
    nextPlaylistupdate = 0;
    streamFormat = HLSStreamFormat::UNKNOWN;
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
        text.append(" (not loaded)");
        msg_Dbg(obj, "%s", text.c_str());
    }
}

bool Representation::needsUpdate() const
{
    return true;
}

void Representation::runLocalUpdates(mtime_t /*currentplaybacktime*/, uint64_t number)
{
    const time_t now = time(NULL);
    const AbstractPlaylist *playlist = getPlaylist();
    if(!b_loaded || (isLive() && nextPlaylistupdate < now))
    {
        M3U8Parser parser;
        parser.appendSegmentsFromPlaylistURI(playlist->getVLCObject(), this);
        b_loaded = true;

        pruneBySegmentNumber(number);

        /* Compute new update time */
        mtime_t mininterval = 0;
        mtime_t maxinterval = 0;

        getDurationsRange( &mininterval, &maxinterval );

        if(playlist->minUpdatePeriod.Get() > mininterval)
            mininterval = playlist->minUpdatePeriod.Get();

        if(mininterval < 5 * CLOCK_FREQ)
            mininterval = 5 * CLOCK_FREQ;

        if(maxinterval < mininterval)
            maxinterval = mininterval;

        nextPlaylistupdate = now + (mininterval + (maxinterval - mininterval) / 2) / CLOCK_FREQ;

        msg_Dbg(playlist->getVLCObject(), "Updated playlist ID %s, next update in %" PRId64 "s",
                getID().str().c_str(), nextPlaylistupdate - now);

        debug(playlist->getVLCObject(), 0);
    }
}

void Representation::getDurationsRange(mtime_t *min, mtime_t *max) const
{
    if(!b_loaded)
        return;
    BaseRepresentation::getDurationsRange(min, max);
}
