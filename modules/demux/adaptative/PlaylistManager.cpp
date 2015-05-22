/*
 * PlaylistManager.cpp
 *****************************************************************************
 * Copyright © 2010 - 2011 Klagenfurt University
 *             2015 VideoLAN Authors
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
#define __STDC_FORMAT_MACROS
#define __STDC_CONSTANT_MACROS
#define __STDC_LIMIT_MACROS

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "PlaylistManager.h"
#include "SegmentTracker.hpp"
#include "playlist/AbstractPlaylist.hpp"
#include "playlist/BasePeriod.h"
#include "playlist/BaseAdaptationSet.h"
#include "http/HTTPConnectionManager.h"
#include "logic/AlwaysBestAdaptationLogic.h"
#include "logic/RateBasedAdaptationLogic.h"
#include "logic/AlwaysLowestAdaptationLogic.hpp"
#include <vlc_stream.h>

#include <ctime>

using namespace adaptative::http;
using namespace adaptative::logic;
using namespace adaptative;

PlaylistManager::PlaylistManager( AbstractPlaylist *pl,
                                  AbstractAdaptationLogic::LogicType type,
                                  stream_t *stream) :
             conManager     ( NULL ),
             logicType      ( type ),
             playlist       ( pl ),
             stream         ( stream ),
             nextPlaylistupdate  ( 0 )
{
    for(int i=0; i<StreamTypeCount; i++)
        streams[i] = NULL;
}

PlaylistManager::~PlaylistManager   ()
{
    delete conManager;
    for(int i=0; i<StreamTypeCount; i++)
        delete streams[i];
}

bool PlaylistManager::start(demux_t *demux)
{
    const BasePeriod *period = playlist->getFirstPeriod();
    if(!period)
        return false;

    for(int i=0; i<StreamTypeCount; i++)
    {
        StreamType type = static_cast<StreamType>(i);
        const BaseAdaptationSet *set = period->getAdaptationSet(type);
        if(set)
        {
            streams[type] = new (std::nothrow) Stream(set->getMimeType());
            if(!streams[type])
                continue;
            AbstractAdaptationLogic *logic = createLogic(logicType);
            if(!logic)
            {
                delete streams[type];
                streams[type] = NULL;
                continue;
            }

            SegmentTracker *tracker = new (std::nothrow) SegmentTracker(logic, playlist);
            try
            {
                if(!tracker)
                    throw VLC_ENOMEM;
                streams[type]->create(demux, logic, tracker);
            } catch (int) {
                delete streams[type];
                delete logic;
                delete tracker;
                streams[type] = NULL;
            }
        }
    }

    conManager = new (std::nothrow) HTTPConnectionManager(VLC_OBJECT(stream));
    if(!conManager)
        return false;

    playlist->playbackStart.Set(time(NULL));
    nextPlaylistupdate = playlist->playbackStart.Get();

    return true;
}

Stream::status PlaylistManager::demux(mtime_t nzdeadline)
{
    Stream::status i_return = Stream::status_demuxed;

    for(int type=0; type<StreamTypeCount; type++)
    {
        if(!streams[type])
            continue;

        Stream::status i_ret =
                streams[type]->demux(conManager, nzdeadline);

        if(i_ret < Stream::status_eof)
            return i_ret;
        else if (i_ret == Stream::status_buffering)
            i_return = Stream::status_buffering;
    }

    return i_return;
}

mtime_t PlaylistManager::getPCR() const
{
    mtime_t pcr = VLC_TS_INVALID;
    for(int type=0; type<StreamTypeCount; type++)
    {
        if(!streams[type])
            continue;
        if(pcr == VLC_TS_INVALID || pcr > streams[type]->getPCR())
            pcr = streams[type]->getPCR();
    }
    return pcr;
}

int PlaylistManager::getGroup() const
{
    for(int type=0; type<StreamTypeCount; type++)
    {
        if(!streams[type])
            continue;
        return streams[type]->getGroup();
    }
    return -1;
}

int PlaylistManager::esCount() const
{
    int es = 0;
    for(int type=0; type<StreamTypeCount; type++)
    {
        if(!streams[type])
            continue;
        es += streams[type]->esCount();
    }
    return es;
}

mtime_t PlaylistManager::getDuration() const
{
    if (playlist->isLive())
        return 0;
    else
        return CLOCK_FREQ * playlist->duration.Get();
}

bool PlaylistManager::setPosition(mtime_t time)
{
    bool ret = true;
    for(int real = 0; real < 2; real++)
    {
        /* Always probe if we can seek first */
        for(int type=0; type<StreamTypeCount; type++)
        {
            if(!streams[type])
                continue;
            ret &= streams[type]->setPosition(time, !real);
        }
        if(!ret)
            break;
    }
    return ret;
}

bool PlaylistManager::seekAble() const
{
    if(playlist->isLive())
        return false;

    for(int type=0; type<StreamTypeCount; type++)
    {
        if(!streams[type])
            continue;
        if(!streams[type]->seekAble())
            return false;
    }
    return true;
}

bool PlaylistManager::updatePlaylist()
{
    return true;
}

AbstractAdaptationLogic *PlaylistManager::createLogic(AbstractAdaptationLogic::LogicType type)
{
    switch(type)
    {
        case AbstractAdaptationLogic::AlwaysBest:
            return new (std::nothrow) AlwaysBestAdaptationLogic();
        case AbstractAdaptationLogic::FixedRate:
        case AbstractAdaptationLogic::AlwaysLowest:
            return new (std::nothrow) AlwaysLowestAdaptationLogic();
        case AbstractAdaptationLogic::Default:
        case AbstractAdaptationLogic::RateBased:
            return new (std::nothrow) RateBasedAdaptationLogic(0, 0);
        default:
            return NULL;
    }
}
