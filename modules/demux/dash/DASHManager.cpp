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
#include "mpd/ProgramInformation.h"
#include "xml/DOMParser.h"
#include "../adaptative/logic/RateBasedAdaptationLogic.h"
#include "../adaptative/tools/Helper.h"
#include <vlc_stream.h>
#include <vlc_demux.h>
#include <vlc_meta.h>
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

DASHManager::DASHManager(demux_t *demux_, MPD *mpd,
                         AbstractStreamOutputFactory *factory,
                         AbstractAdaptationLogic::LogicType type) :
             PlaylistManager(demux_, mpd, factory, type)
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
        std::string url(p_demux->psz_access);
        url.append("://");
        url.append(p_demux->psz_location);

        uint8_t *p_data = NULL;
        size_t i_data = Retrieve::HTTP(VLC_OBJECT(p_demux->s), url, (void**) &p_data);
        if(!p_data)
            return false;

        stream_t *mpdstream = stream_MemoryNew(p_demux->s, p_data, i_data, false);
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

        MPD *newmpd = MPDFactory::create(parser.getRootNode(), mpdstream,
                                         Helper::getDirectoryPath(url).append("/"),
                                         parser.getProfile());
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

    msg_Dbg(p_demux, "Updated MPD, next update in %" PRId64 "s (%" PRId64 "..%" PRId64 ")",
            nextPlaylistupdate - now, mininterval, maxinterval );

    return true;
}

int DASHManager::doControl(int i_query, va_list args)
{
    switch (i_query)
    {
        case DEMUX_GET_META:
        {
            MPD *mpd = dynamic_cast<MPD *>(playlist);
            if(!mpd)
                return VLC_EGENERIC;

            if(!mpd->programInfo.Get())
                break;

            vlc_meta_t *p_meta = (vlc_meta_t *) va_arg (args, vlc_meta_t*);
            vlc_meta_t *meta = vlc_meta_New();
            if (meta == NULL)
                return VLC_EGENERIC;

            if(!mpd->programInfo.Get()->getTitle().empty())
                vlc_meta_SetTitle(meta, mpd->programInfo.Get()->getTitle().c_str());

            if(!mpd->programInfo.Get()->getSource().empty())
                vlc_meta_SetPublisher(meta, mpd->programInfo.Get()->getSource().c_str());

            if(!mpd->programInfo.Get()->getCopyright().empty())
                vlc_meta_SetCopyright(meta, mpd->programInfo.Get()->getCopyright().c_str());

            if(!mpd->programInfo.Get()->getMoreInformationUrl().empty())
                vlc_meta_SetURL(meta, mpd->programInfo.Get()->getMoreInformationUrl().c_str());

            vlc_meta_Merge(p_meta, meta);
            vlc_meta_Delete(meta);
            break;
        }
    }
    return PlaylistManager::doControl(i_query, args);
}

bool DASHManager::isDASH(stream_t *stream)
{
    const std::string namespaces[] = {
        "xmlns=\"urn:mpeg:mpegB:schema:DASH:MPD:DIS2011\"",
        "xmlns=\"urn:mpeg:schema:dash:mpd:2011\"",
        "xmlns=\"urn:mpeg:DASH:schema:MPD:2011\"",
        "xmlns='urn:mpeg:mpegB:schema:DASH:MPD:DIS2011'",
        "xmlns='urn:mpeg:schema:dash:mpd:2011'",
        "xmlns='urn:mpeg:DASH:schema:MPD:2011'",
    };

    const uint8_t *peek;
    int peek_size = stream_Peek(stream, &peek, 1024);
    if (peek_size < (int)namespaces[0].length())
        return false;

    std::string header((const char*)peek, peek_size);
    for( size_t i=0; i<ARRAY_SIZE(namespaces); i++ )
    {
        if ( adaptative::Helper::ifind(header, namespaces[i]) )
            return true;
    }
    return false;
}

AbstractAdaptationLogic *DASHManager::createLogic(AbstractAdaptationLogic::LogicType type)
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
