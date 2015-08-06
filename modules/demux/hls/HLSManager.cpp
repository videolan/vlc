/*****************************************************************************
 * HLSManager.cpp
 *****************************************************************************
 * Copyright © 2015 VideoLAN and VLC authors
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

#include "HLSManager.hpp"
#include "../adaptative/logic/RateBasedAdaptationLogic.h"
#include "../adaptative/tools/Retrieve.hpp"
#include "playlist/Parser.hpp"
#include <vlc_stream.h>
#include <vlc_demux.h>
#include <time.h>

using namespace adaptative;
using namespace adaptative::logic;
using namespace hls;
using namespace hls::playlist;

HLSManager::HLSManager(demux_t *demux_, M3U8 *playlist,
                       AbstractStreamOutputFactory *factory,
                       AbstractAdaptationLogic::LogicType type) :
             PlaylistManager(demux_, playlist, factory, type)
{
}

HLSManager::~HLSManager()
{
}

bool HLSManager::isHTTPLiveStreaming(stream_t *s)
{
    const uint8_t *peek;

    int size = stream_Peek(s, &peek, 7);
    if (size < 7 || memcmp(peek, "#EXTM3U", 7))
        return false;

    size = stream_Peek(s, &peek, 512);
    if (size < 7)
        return false;

    peek += 7;
    size -= 7;

    /* Parse stream and search for
     * EXT-X-TARGETDURATION or EXT-X-STREAM-INF tag, see
     * http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8 */
    while (size--)
    {
        static const char *const ext[] = {
            "TARGETDURATION",
            "MEDIA-SEQUENCE",
            "KEY",
            "ALLOW-CACHE",
            "ENDLIST",
            "STREAM-INF",
            "DISCONTINUITY",
            "VERSION"
        };

        if (*peek++ != '#')
            continue;

        if (size < 6)
            continue;

        if (memcmp(peek, "EXT-X-", 6))
            continue;

        peek += 6;
        size -= 6;

        for (size_t i = 0; i < ARRAY_SIZE(ext); i++)
        {
            size_t len = strlen(ext[i]);
            if (size < 0 || (size_t)size < len)
                continue;
            if (!memcmp(peek, ext[i], len))
                return true;
        }
    }

    return false;
}

AbstractAdaptationLogic *HLSManager::createLogic(AbstractAdaptationLogic::LogicType type)
{
    switch(type)
    {
        case AbstractAdaptationLogic::FixedRate:
        {
            size_t bps = var_InheritInteger(p_demux, "adaptative-bw") * 8192;
            return new (std::nothrow) FixedRateAdaptationLogic(bps);
        }
        case AbstractAdaptationLogic::Default:
        case AbstractAdaptationLogic::RateBased:
        {
            int width = var_InheritInteger(p_demux, "adaptative-width");
            int height = var_InheritInteger(p_demux, "adaptative-height");
            return new (std::nothrow) RateBasedAdaptationLogic(width, height);
        }
        default:
            return PlaylistManager::createLogic(type);
    }
}

bool HLSManager::updatePlaylist()
{
    if(!playlist->isLive() || !playlist->minUpdatePeriod.Get())
        return true;

    mtime_t now = time(NULL);
    if(nextPlaylistupdate && now < nextPlaylistupdate)
        return true;

    M3U8 *updatedplaylist = NULL;

    /* do update */
    if(nextPlaylistupdate)
    {
        std::string url(p_demux->psz_access);
        url.append("://");
        url.append(p_demux->psz_location);

        uint8_t *p_data = NULL;
        size_t i_data = Retrieve::HTTP(VLC_OBJECT(p_demux->s), url, (void**) &p_data);
        if(!p_data)
            return false;

        stream_t *updatestream = stream_MemoryNew(p_demux->s, p_data, i_data, false);
        if(!updatestream)
        {
            free(p_data);
            nextPlaylistupdate = now + playlist->minUpdatePeriod.Get();
            return false;
        }

        Parser parser(updatestream);
        updatedplaylist = parser.parse(url);
        if(!updatedplaylist)
        {
            stream_Delete(updatestream);
            nextPlaylistupdate = now + playlist->minUpdatePeriod.Get();
            return false;
        }


        stream_Delete(updatestream);
    }

    /* Compute new MPD update time */
    mtime_t mininterval = 0;
    mtime_t maxinterval = 0;
    if(updatedplaylist)
    {
        updatedplaylist->getPlaylistDurationsRange(&mininterval, &maxinterval);
        playlist->mergeWith(updatedplaylist);
        playlist->debug();
        delete updatedplaylist;

        /* pruning */
        std::vector<Stream *>::iterator it;
        for(it=streams.begin(); it!=streams.end(); ++it)
        {
            (*it)->prune();
        }
    }
    else
    {
        playlist->getPlaylistDurationsRange(&mininterval, &maxinterval);
    }

    if(playlist->minUpdatePeriod.Get() * CLOCK_FREQ > mininterval)
        mininterval = playlist->minUpdatePeriod.Get() * CLOCK_FREQ;

    if(maxinterval < mininterval)
        maxinterval = mininterval;

    nextPlaylistupdate = now + (mininterval + maxinterval) / (2 * CLOCK_FREQ);

    msg_Dbg(p_demux, "Updated playlist, next update in %" PRId64 "s "
            "%" PRId64 " %" PRId64, nextPlaylistupdate - now, mininterval,
            maxinterval);

    return true;
}
