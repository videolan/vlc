/*****************************************************************************
 * DASHManager.cpp
 *****************************************************************************
 * Copyright © 2010 - 2011 Klagenfurt University
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

#include <inttypes.h>

#include "DASHManager.h"
#include "DASHStreamFormat.hpp"
#include "mpd/MPDFactory.h"
#include "xml/DOMParser.h"
#include "../adaptative/logic/RateBasedAdaptationLogic.h"
#include <vlc_stream.h>
#include "../adaptative/tools/Retrieve.hpp"

#include <algorithm>
#include <ctime>

using namespace dash;
using namespace dash::mpd;
using namespace adaptative::logic;


AbstractStreamOutput *DASHStreamOutputFactory::create(demux_t *demux, const StreamFormat &format) const
{
    unsigned fmt = format;
    switch(fmt)
    {
        case DASHStreamFormat::MP4:
            return new BaseStreamOutput(demux, format, "mp4");

        case DASHStreamFormat::MPEG2TS:
            return new BaseStreamOutput(demux, format, "ts");
    }
    return NULL;
}

DASHManager::DASHManager(MPD *mpd,
                         AbstractStreamOutputFactory *factory,
                         AbstractAdaptationLogic::LogicType type, stream_t *stream) :
             PlaylistManager(mpd, factory, type, stream)
{
}

DASHManager::~DASHManager   ()
{
}

bool DASHManager::updatePlaylist()
{
    if(!playlist->isLive() || !playlist->minUpdatePeriod.Get())
        return true;

    mtime_t now = time(NULL);
    if(nextPlaylistupdate && now < nextPlaylistupdate)
        return true;

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

        stream_t *mpdstream = stream_MemoryNew(stream, p_data, i_data, false);
        if(!mpdstream)
        {
            free(p_data);
            nextPlaylistupdate = now + playlist->minUpdatePeriod.Get();
            return false;
        }

        xml::DOMParser parser(mpdstream);
        if(!parser.parse())
        {
            stream_Delete(mpdstream);
            nextPlaylistupdate = now + playlist->minUpdatePeriod.Get();
            return false;
        }

        mtime_t minsegmentTime = 0;
        std::vector<Stream *>::iterator it;
        for(it=streams.begin(); it!=streams.end(); it++)
        {
            mtime_t segmentTime = (*it)->getPosition();
            if(!minsegmentTime || segmentTime < minsegmentTime)
                minsegmentTime = segmentTime;
        }

        MPD *newmpd = MPDFactory::create(parser.getRootNode(), mpdstream, parser.getProfile());
        if(newmpd)
        {
            playlist->mergeWith(newmpd, minsegmentTime);
            delete newmpd;
        }
        stream_Delete(mpdstream);
    }

    /* Compute new MPD update time */
    mtime_t mininterval = 0;
    mtime_t maxinterval = 0;
    playlist->getTimeLinesBoundaries(&mininterval, &maxinterval);
    if(maxinterval > mininterval)
        maxinterval = (maxinterval - mininterval);
    else
        maxinterval = 60 * CLOCK_FREQ;
    maxinterval = std::max(maxinterval, (mtime_t)60 * CLOCK_FREQ);

    mininterval = std::max(playlist->minUpdatePeriod.Get() * CLOCK_FREQ,
                           playlist->maxSegmentDuration.Get());

    nextPlaylistupdate = now + (maxinterval - mininterval) / (2 * CLOCK_FREQ);

    msg_Dbg(stream, "Updated MPD, next update in %" PRId64 "s (%" PRId64 "..%" PRId64 ")",
            nextPlaylistupdate - now, mininterval, maxinterval );

    return true;
}

AbstractAdaptationLogic *DASHManager::createLogic(AbstractAdaptationLogic::LogicType type)
{
    switch(type)
    {
        case AbstractAdaptationLogic::FixedRate:
        {
            size_t bps = var_InheritInteger(stream, "dash-prefbw") * 8192;
            return new (std::nothrow) FixedRateAdaptationLogic(bps);
        }
        case AbstractAdaptationLogic::Default:
        case AbstractAdaptationLogic::RateBased:
        {
            int width = var_InheritInteger(stream, "dash-prefwidth");
            int height = var_InheritInteger(stream, "dash-prefheight");
            return new (std::nothrow) RateBasedAdaptationLogic(width, height);
        }
        default:
            return PlaylistManager::createLogic(type);
    }
}
