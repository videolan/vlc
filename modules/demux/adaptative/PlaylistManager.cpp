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
#include "playlist/BaseRepresentation.h"
#include "http/HTTPConnectionManager.h"
#include "logic/AlwaysBestAdaptationLogic.h"
#include "logic/RateBasedAdaptationLogic.h"
#include "logic/AlwaysLowestAdaptationLogic.hpp"
#include <vlc_stream.h>
#include <vlc_demux.h>

#include <ctime>

using namespace adaptative::http;
using namespace adaptative::logic;
using namespace adaptative;

PlaylistManager::PlaylistManager( demux_t *p_demux_,
                                  AbstractPlaylist *pl,
                                  AbstractStreamOutputFactory *factory,
                                  AbstractAdaptationLogic::LogicType type ) :
             conManager     ( NULL ),
             logicType      ( type ),
             playlist       ( pl ),
             streamOutputFactory( factory ),
             p_demux        ( p_demux_ ),
             nextPlaylistupdate  ( 0 ),
             i_nzpcr        ( 0 )
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

                std::list<std::string> languages;
                if(!set->getLang().empty())
                {
                    languages = set->getLang();
                }
                else if(!set->getRepresentations().empty())
                {
                    languages = set->getRepresentations().front()->getLang();
                }

                if(!languages.empty())
                    st->setLanguage(languages.front());

                if(!set->description.Get().empty())
                    st->setDescription(set->description.Get());

                st->create(logic, tracker, streamOutputFactory);

                streams.push_back(st);
            } catch (int) {
                delete st;
            }
        }
    }
    return true;
}

bool PlaylistManager::start()
{
    if(!setupPeriod())
        return false;

    conManager = new (std::nothrow) HTTPConnectionManager(VLC_OBJECT(p_demux->s));
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

    /* might be end of current period */
    if(i_return == Stream::status_eof && currentPeriod)
    {
        unsetPeriod();
        currentPeriod = playlist->getNextPeriod(currentPeriod);
        i_return = (setupPeriod()) ? Stream::status_eop : Stream::status_eof;
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

#define DEMUX_INCREMENT (CLOCK_FREQ / 20)
int PlaylistManager::demux_callback(demux_t *p_demux)
{
    PlaylistManager *manager = reinterpret_cast<PlaylistManager *>(p_demux->p_sys);
    return manager->doDemux(DEMUX_INCREMENT);
}

int PlaylistManager::doDemux(int64_t increment)
{
    if(i_nzpcr == VLC_TS_INVALID)
    {
        if( Stream::status_eof == demux(i_nzpcr + increment, false) )
        {
            return VLC_DEMUXER_EOF;
        }
        i_nzpcr = getFirstDTS();
        if(i_nzpcr == VLC_TS_INVALID)
            i_nzpcr = getPCR();
    }

    Stream::status status = demux(i_nzpcr + increment, true);

    switch(status)
    {
    case Stream::status_eof:
        return VLC_DEMUXER_EOF;
    case Stream::status_buffering:
        break;
    case Stream::status_eop:
        i_nzpcr = VLC_TS_INVALID;
        es_out_Control(p_demux->out, ES_OUT_RESET_PCR);
        break;
    case Stream::status_demuxed:
        if( i_nzpcr != VLC_TS_INVALID )
        {
            i_nzpcr += increment;
            es_out_Control(p_demux->out, ES_OUT_SET_GROUP_PCR, 0, VLC_TS_0 + i_nzpcr);
        }
        break;
    }

    if( !updatePlaylist() )
        msg_Warn(p_demux, "Can't update MPD");

    return VLC_DEMUXER_SUCCESS;
}

int PlaylistManager::control_callback(demux_t *p_demux, int i_query, va_list args)
{
    PlaylistManager *manager = reinterpret_cast<PlaylistManager *>(p_demux->p_sys);
    return manager->doControl(i_query, args);
}

int PlaylistManager::doControl(int i_query, va_list args)
{
    switch (i_query)
    {
        case DEMUX_CAN_SEEK:
            *(va_arg (args, bool *)) = seekAble();
            break;

        case DEMUX_CAN_CONTROL_PACE:
            *(va_arg (args, bool *)) = true;
            break;

        case DEMUX_CAN_PAUSE:
            *(va_arg (args, bool *)) = playlist->isLive();
            break;

        case DEMUX_GET_TIME:
            *(va_arg (args, int64_t *)) = i_nzpcr;
            break;

        case DEMUX_GET_LENGTH:
            *(va_arg (args, int64_t *)) = getDuration();
            break;

        case DEMUX_GET_POSITION:
            if(!getDuration())
                return VLC_EGENERIC;

            *(va_arg (args, double *)) = (double) i_nzpcr
                                         / getDuration();
            break;

        case DEMUX_SET_POSITION:
        {
            int64_t time = getDuration() * va_arg(args, double);
            if(playlist->isLive() ||
               !getDuration() ||
               !setPosition(time))
                return VLC_EGENERIC;
            i_nzpcr = VLC_TS_INVALID;
            break;
        }

        case DEMUX_SET_TIME:
        {
            int64_t time = va_arg(args, int64_t);
            if(playlist->isLive() ||
               !setPosition(time))
                return VLC_EGENERIC;
            i_nzpcr = VLC_TS_INVALID;
            break;
        }

        case DEMUX_GET_PTS_DELAY:
            *va_arg (args, int64_t *) = INT64_C(1000) *
                var_InheritInteger(p_demux, "network-caching");
             break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
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
