/*
 * PlaylistManager.cpp
 *****************************************************************************
 * Copyright © 2010 - 2011 Klagenfurt University
 *             2015 VideoLAN and VLC Authors
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
                                  AbstractStreamOutputFactory *factory,
                                  AbstractAdaptationLogic::LogicType type,
                                  stream_t *stream) :
             conManager     ( NULL ),
             logicType      ( type ),
             playlist       ( pl ),
             streamOutputFactory( factory ),
             stream         ( stream ),
             nextPlaylistupdate  ( 0 )
{
    currentPeriod = playlist->getFirstPeriod();
}

PlaylistManager::~PlaylistManager   ()
{
    delete conManager;
    delete streamOutputFactory;
    unsetPeriod();
}

void PlaylistManager::unsetPeriod()
{
    std::vector<Stream *>::iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
        delete *it;
    streams.clear();
}

bool PlaylistManager::setupPeriod()
{
    if(!currentPeriod)
        return false;

    std::vector<BaseAdaptationSet*> sets = currentPeriod->getAdaptationSets();
    std::vector<BaseAdaptationSet*>::iterator it;
    for(it=sets.begin();it!=sets.end();++it)
    {
        BaseAdaptationSet *set = *it;
        if(set)
        {
            Stream *st = new (std::nothrow) Stream(p_demux, set->getStreamFormat());
            if(!st)
                continue;
            AbstractAdaptationLogic *logic = createLogic(logicType);
            if(!logic)
            {
                delete st;
                continue;
            }

            SegmentTracker *tracker = new (std::nothrow) SegmentTracker(logic, set);
            try
            {
                if(!tracker || !streamOutputFactory)
                {
                    delete tracker;
                    delete logic;
                    throw VLC_ENOMEM;
                }
                st->create(logic, tracker, streamOutputFactory);
                streams.push_back(st);
            } catch (int) {
                delete st;
            }
        }
    }
    return true;
}

bool PlaylistManager::start(demux_t *demux_)
{
    p_demux = demux_;

    if(!setupPeriod())
        return false;

    conManager = new (std::nothrow) HTTPConnectionManager(VLC_OBJECT(stream));
    if(!conManager)
        return false;

    playlist->playbackStart.Set(time(NULL));
    nextPlaylistupdate = playlist->playbackStart.Get();

    return true;
}

Stream::status PlaylistManager::demux(mtime_t nzdeadline, bool send)
{
    Stream::status i_return = Stream::status_eof;

    std::vector<Stream *>::iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        Stream *st = *it;

        if (st->isDisabled())
        {
            if(st->isSelected() && !st->isEOF())
                st->reactivate(getPCR());
            else
                continue;
        }

        Stream::status i_ret = st->demux(conManager, nzdeadline, send);

        if(i_ret == Stream::status_buffering)
        {
            i_return = Stream::status_buffering;
        }
        else if(i_ret == Stream::status_demuxed &&
                i_return != Stream::status_buffering)
        {
            i_return = Stream::status_demuxed;
        }
    }

    return i_return;
}

mtime_t PlaylistManager::getPCR() const
{
    mtime_t pcr = VLC_TS_INVALID;
    std::vector<Stream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        if ((*it)->isDisabled())
            continue;
        if(pcr == VLC_TS_INVALID || pcr > (*it)->getPCR())
            pcr = (*it)->getPCR();
    }
    return pcr;
}

mtime_t PlaylistManager::getFirstDTS() const
{
    mtime_t dts = VLC_TS_INVALID;
    std::vector<Stream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        if ((*it)->isDisabled())
            continue;
        if(dts == VLC_TS_INVALID || dts > (*it)->getFirstDTS())
            dts = (*it)->getFirstDTS();
    }
    return dts;
}

int PlaylistManager::getGroup() const
{
    std::vector<Stream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        return (*it)->getGroup();
    }
    return -1;
}

int PlaylistManager::esCount() const
{
    int es = 0;
    std::vector<Stream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        es += (*it)->esCount();
    }
    return es;
}

mtime_t PlaylistManager::getDuration() const
{
    if (playlist->isLive())
        return 0;
    else
        return playlist->duration.Get();
}

bool PlaylistManager::setPosition(mtime_t time)
{
    bool ret = true;
    for(int real = 0; real < 2; real++)
    {
        /* Always probe if we can seek first */
        std::vector<Stream *>::iterator it;
        for(it=streams.begin(); it!=streams.end(); ++it)
        {
            ret &= (*it)->setPosition(time, !real);
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

    std::vector<Stream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        if(!(*it)->seekAble())
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
