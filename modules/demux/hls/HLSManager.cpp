/*****************************************************************************
 * HLSManager.cpp
 *****************************************************************************
 * Copyright © 2015 VideoLAN authors
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
#include <time.h>

using namespace adaptative;
using namespace adaptative::logic;
using namespace hls;
using namespace hls::playlist;

HLSManager::HLSManager(M3U8 *playlist,
                       AbstractAdaptationLogic::LogicType type, stream_t *stream) :
             PlaylistManager(playlist, type, stream)
{
}

HLSManager::~HLSManager()
{
}

AbstractAdaptationLogic *HLSManager::createLogic(AbstractAdaptationLogic::LogicType type)
{
    switch(type)
    {
        case AbstractAdaptationLogic::FixedRate:
        {
            size_t bps = var_InheritInteger(stream, "hls-prefbw") * 8192;
            return new (std::nothrow) FixedRateAdaptationLogic(bps);
        }
        case AbstractAdaptationLogic::Default:
        case AbstractAdaptationLogic::RateBased:
        {
            int width = var_InheritInteger(stream, "hls-prefwidth");
            int height = var_InheritInteger(stream, "hls-prefheight");
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
        std::string url(stream->psz_access);
        url.append("://");
        url.append(stream->psz_path);

        uint8_t *p_data = NULL;
        size_t i_data = Retrieve::HTTP(VLC_OBJECT(stream), url, (void**) &p_data);
        if(!p_data)
            return false;

        stream_t *updatestream = stream_MemoryNew(stream, p_data, i_data, false);
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
        for(int type=0; type<StreamTypeCount; type++)
        {
            if(!streams[type])
                continue;
            streams[type]->prune();
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

    msg_Dbg(stream, "Updated playlist, next update in %" PRId64 "s "
            "%" PRId64 " %" PRId64, nextPlaylistupdate - now, mininterval,
            maxinterval);

    return true;
}
