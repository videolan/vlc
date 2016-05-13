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
#include "tools/Debug.hpp"
#include <vlc_stream.h>
#include <vlc_demux.h>

#include <ctime>

using namespace adaptive::http;
using namespace adaptive::logic;
using namespace adaptive;

PlaylistManager::PlaylistManager( demux_t *p_demux_,
                                  AbstractPlaylist *pl,
                                  AbstractStreamFactory *factory,
                                  AbstractAdaptationLogic::LogicType type ) :
             conManager     ( NULL ),
             logicType      ( type ),
             logic          ( NULL ),
             playlist       ( pl ),
             streamFactory  ( factory ),
             p_demux        ( p_demux_ ),
             nextPlaylistupdate  ( 0 ),
             i_nzpcr        ( VLC_TS_INVALID )
{
    currentPeriod = playlist->getFirstPeriod();
    failedupdates = 0;
    i_firstpcr = i_nzpcr;
}

PlaylistManager::~PlaylistManager   ()
{
    delete streamFactory;
    unsetPeriod();
    delete playlist;
    delete conManager;
    delete logic;
}

void PlaylistManager::unsetPeriod()
{
    std::vector<AbstractStream *>::iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
        delete *it;
    streams.clear();
}

bool PlaylistManager::setupPeriod()
{
    if(!currentPeriod)
        return false;

    if(!logic && !(logic = createLogic(logicType, conManager)))
        return false;

    std::vector<BaseAdaptationSet*> sets = currentPeriod->getAdaptationSets();
    std::vector<BaseAdaptationSet*>::iterator it;
    for(it=sets.begin();it!=sets.end();++it)
    {
        BaseAdaptationSet *set = *it;
        if(set && streamFactory)
        {
            SegmentTracker *tracker = new (std::nothrow) SegmentTracker(logic, set);
            if(!tracker)
                continue;

            AbstractStream *st = streamFactory->create(p_demux, set->getStreamFormat(),
                                                       tracker, conManager);
            if(!st)
            {
                delete tracker;
                continue;
            }

            streams.push_back(st);

            /* Generate stream description */
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
        }
    }
    return true;
}

bool PlaylistManager::start()
{
    if(!conManager && !(conManager = new (std::nothrow) HTTPConnectionManager(VLC_OBJECT(p_demux->s))))
        return false;

    if(!setupPeriod())
        return false;

    playlist->playbackStart.Set(time(NULL));
    nextPlaylistupdate = playlist->playbackStart.Get();

    return true;
}

AbstractStream::status PlaylistManager::demux(mtime_t nzdeadline, bool send)
{
    AbstractStream::status i_return = AbstractStream::status_eof;

    std::vector<AbstractStream *>::iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        AbstractStream *st = *it;

        if (st->isDisabled())
        {
            if(st->isSelected() && !st->isEOF())
                reactivateStream(st);
            else
                continue;
        }

        AbstractStream::status i_ret = st->demux(nzdeadline, send);
        if(i_ret == AbstractStream::status_buffering_ahead ||
           i_return == AbstractStream::status_buffering_ahead)
        {
            i_return = AbstractStream::status_buffering_ahead;
        }
        else if(i_ret == AbstractStream::status_buffering)
        {
            i_return = AbstractStream::status_buffering;
        }
        else if(i_ret == AbstractStream::status_demuxed &&
                i_return != AbstractStream::status_buffering)
        {
            i_return = AbstractStream::status_demuxed;
        }
        else if(i_ret == AbstractStream::status_dis)
        {
            i_return = AbstractStream::status_dis;
        }
    }

    /* might be end of current period */
    if(i_return == AbstractStream::status_eof && currentPeriod)
    {
        unsetPeriod();
        currentPeriod = playlist->getNextPeriod(currentPeriod);
        i_return = (setupPeriod()) ? AbstractStream::status_eop : AbstractStream::status_eof;
    }

    return i_return;
}

mtime_t PlaylistManager::getPCR() const
{
    mtime_t pcr = VLC_TS_INVALID;
    std::vector<AbstractStream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        if ((*it)->isDisabled() || (*it)->isEOF())
            continue;
        if(pcr == VLC_TS_INVALID || pcr > (*it)->getPCR())
            pcr = (*it)->getPCR();
    }
    return pcr;
}

mtime_t PlaylistManager::getFirstDTS() const
{
    mtime_t dts = VLC_TS_INVALID;
    std::vector<AbstractStream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        if ((*it)->isDisabled() || (*it)->isEOF())
            continue;
        if(dts == VLC_TS_INVALID || dts > (*it)->getFirstDTS())
            dts = (*it)->getFirstDTS();
    }
    return dts;
}

int PlaylistManager::esCount() const
{
    int es = 0;
    std::vector<AbstractStream *>::const_iterator it;
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
        std::vector<AbstractStream *>::iterator it;
        for(it=streams.begin(); it!=streams.end(); ++it)
        {
            ret &= (*it)->setPosition(time, !real);
        }
        if(!ret)
            break;
    }
    return ret;
}

bool PlaylistManager::needsUpdate() const
{
    return playlist->isLive() && (failedupdates < 3);
}

bool PlaylistManager::seekAble() const
{
    if(playlist->isLive())
        return false;

    std::vector<AbstractStream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        if(!(*it)->seekAble())
            return false;
    }
    return true;
}

void PlaylistManager::scheduleNextUpdate()
{

}

bool PlaylistManager::updatePlaylist()
{
    std::vector<AbstractStream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
        (*it)->runUpdates();

    return true;
}

mtime_t PlaylistManager::getFirstPlaybackTime() const
{
    return 0;
}

mtime_t PlaylistManager::getCurrentPlaybackTime() const
{
    return i_nzpcr;
}

void PlaylistManager::pruneLiveStream()
{
    mtime_t minValidPos = 0;
    std::vector<AbstractStream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); it++)
    {
        const AbstractStream *st = *it;
        if(st->isDisabled() || !st->isSelected() || st->isEOF())
            continue;
        const mtime_t t = st->getPlaybackTime();
        if(minValidPos == 0 || t < minValidPos)
            minValidPos = t;
    }

    if(minValidPos)
        playlist->pruneByPlaybackTime(minValidPos);
}

bool PlaylistManager::reactivateStream(AbstractStream *stream)
{
    return stream->reactivate(getPCR());
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
        if( AbstractStream::status_eof == demux(i_nzpcr + increment, false) )
        {
            return VLC_DEMUXER_EOF;
        }
        i_nzpcr = getFirstDTS();
        if(i_nzpcr == VLC_TS_INVALID)
            i_nzpcr = getPCR();
        if(i_firstpcr == VLC_TS_INVALID)
            i_firstpcr = i_nzpcr;
    }

    AbstractStream::status status = demux(i_nzpcr + increment, true);
    AdvDebug(msg_Dbg( p_demux, "doDemux() status %d dts %ld pcr %ld", status, getFirstDTS(), getPCR() ));
    switch(status)
    {
    case AbstractStream::status_eof:
        return VLC_DEMUXER_EOF;
    case AbstractStream::status_buffering:
    case AbstractStream::status_buffering_ahead:
        break;
    case AbstractStream::status_dis:
    case AbstractStream::status_eop:
        i_nzpcr = VLC_TS_INVALID;
        i_firstpcr = VLC_TS_INVALID;
        es_out_Control(p_demux->out, ES_OUT_RESET_PCR);
        break;
    case AbstractStream::status_demuxed:
        if( i_nzpcr != VLC_TS_INVALID )
        {
            i_nzpcr += increment;
            es_out_Control(p_demux->out, ES_OUT_SET_GROUP_PCR, 0, VLC_TS_0 + i_nzpcr);
        }
        break;
    }

    if(needsUpdate())
    {
        if(updatePlaylist())
            scheduleNextUpdate();
        else
            failedupdates++;
    }

    /* Live starved and update still not there ? */
    if(status == AbstractStream::status_buffering_ahead && needsUpdate())
        msleep(CLOCK_FREQ / 20); /* Ugly */

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
            *(va_arg (args, bool *)) = !playlist->isLive();
            break;

        case DEMUX_SET_PAUSE_STATE:
            return (playlist->isLive()) ? VLC_EGENERIC : VLC_SUCCESS;

        case DEMUX_GET_TIME:
        {
            mtime_t i_time = getCurrentPlaybackTime();
            if(!playlist->isLive())
                i_time -= getFirstPlaybackTime();
            *(va_arg (args, int64_t *)) = i_time;
            break;
        }

        case DEMUX_GET_LENGTH:
            if(playlist->isLive())
                return VLC_EGENERIC;
            *(va_arg (args, int64_t *)) = getDuration();
            break;

        case DEMUX_GET_POSITION:
        {
            const mtime_t i_duration = getDuration();
            if(i_duration == 0) /* == playlist->isLive() */
                return VLC_EGENERIC;

            const mtime_t i_length = getCurrentPlaybackTime() - getFirstPlaybackTime();
            *(va_arg (args, double *)) = (double) i_length / i_duration;
            break;
        }

        case DEMUX_SET_POSITION:
        {
            const mtime_t i_duration = getDuration();
            if(i_duration == 0) /* == playlist->isLive() */
                return VLC_EGENERIC;

            int64_t time = i_duration * va_arg(args, double);
            time += getFirstPlaybackTime();

            if(!setPosition(time))
                return VLC_EGENERIC;

            i_nzpcr = VLC_TS_INVALID;
            break;
        }

        case DEMUX_SET_TIME:
        {
            if(playlist->isLive())
                return VLC_EGENERIC;

            int64_t time = va_arg(args, int64_t);// + getFirstPlaybackTime();
            if(!setPosition(time))
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

AbstractAdaptationLogic *PlaylistManager::createLogic(AbstractAdaptationLogic::LogicType type, HTTPConnectionManager *conn)
{
    switch(type)
    {
        case AbstractAdaptationLogic::FixedRate:
        {
            size_t bps = var_InheritInteger(p_demux, "adaptive-bw") * 8192;
            return new (std::nothrow) FixedRateAdaptationLogic(bps);
        }
        case AbstractAdaptationLogic::AlwaysLowest:
            return new (std::nothrow) AlwaysLowestAdaptationLogic();
        case AbstractAdaptationLogic::AlwaysBest:
            return new (std::nothrow) AlwaysBestAdaptationLogic();
        case AbstractAdaptationLogic::Default:
        case AbstractAdaptationLogic::RateBased:
        {
            int width = var_InheritInteger(p_demux, "adaptive-width");
            int height = var_InheritInteger(p_demux, "adaptive-height");
            RateBasedAdaptationLogic *logic =
                    new (std::nothrow) RateBasedAdaptationLogic(VLC_OBJECT(p_demux), width, height);
            conn->setDownloadRateObserver(logic);
            return logic;
        }
        default:
            return NULL;
    }
}
