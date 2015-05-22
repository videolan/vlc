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

/* config.h may include inttypes.h, so make sure we define that option
 * early enough. */
#define __STDC_FORMAT_MACROS 1
#define __STDC_CONSTANT_MACROS 1
#define __STDC_LIMIT_MACROS 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <inttypes.h>

#include "DASHManager.h"
#include "mpd/MPDFactory.h"
#include "xml/DOMParser.h"
#include "../adaptative/logic/RateBasedAdaptationLogic.h"
#include <vlc_stream.h>

#include <algorithm>
#include <ctime>

using namespace dash;
using namespace dash::mpd;
using namespace adaptative::logic;

DASHManager::DASHManager(MPD *mpd,
                         AbstractAdaptationLogic::LogicType type, stream_t *stream) :
             PlaylistManager(mpd, type, stream)
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

        stream_t *mpdstream = stream_UrlNew(stream, url.c_str());
        if(!mpdstream)
            return false;

        xml::DOMParser parser(mpdstream);
        if(!parser.parse())
        {
            stream_Delete(mpdstream);
            return false;
        }

        mtime_t minsegmentTime = 0;
        for(int type=0; type<StreamTypeCount; type++)
        {
            if(!streams[type])
                continue;
            mtime_t segmentTime = streams[type]->getPosition();
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
        maxinterval = (maxinterval - mininterval) / CLOCK_FREQ;
    else
        maxinterval = 60;
    maxinterval = std::max(maxinterval, (mtime_t)60);

    mininterval = std::max(playlist->minUpdatePeriod.Get(),
                           playlist->maxSegmentDuration.Get());

    nextPlaylistupdate = now + (maxinterval - mininterval) / 2;

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
