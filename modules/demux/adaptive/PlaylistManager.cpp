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
#include "http/AuthStorage.hpp"
#include "logic/AlwaysBestAdaptationLogic.h"
#include "logic/RateBasedAdaptationLogic.h"
#include "logic/AlwaysLowestAdaptationLogic.hpp"
#include "logic/PredictiveAdaptationLogic.hpp"
#include "logic/NearOptimalAdaptationLogic.hpp"
#include "tools/Debug.hpp"
#include <vlc_stream.h>
#include <vlc_demux.h>
#include <vlc_threads.h>

#include <algorithm>
#include <ctime>

using namespace adaptive::http;
using namespace adaptive::logic;
using namespace adaptive;

PlaylistManager::PlaylistManager( demux_t *p_demux_,
                                  AuthStorage *auth,
                                  AbstractPlaylist *pl,
                                  AbstractStreamFactory *factory,
                                  AbstractAdaptationLogic::LogicType type ) :
             conManager     ( NULL ),
             logicType      ( type ),
             logic          ( NULL ),
             playlist       ( pl ),
             streamFactory  ( factory ),
             p_demux        ( p_demux_ )
{
    currentPeriod = playlist->getFirstPeriod();
    authStorage = auth;
    failedupdates = 0;
    b_thread = false;
    b_buffering = false;
    nextPlaylistupdate = 0;
    demux.i_nzpcr = VLC_TS_INVALID;
    demux.i_firstpcr = VLC_TS_INVALID;
    vlc_mutex_init(&demux.lock);
    vlc_cond_init(&demux.cond);
    vlc_mutex_init(&lock);
    vlc_cond_init(&waitcond);
    vlc_mutex_init(&cached.lock);
    cached.b_live = false;
    cached.i_length = 0;
    cached.f_position = 0.0;
    cached.i_time = VLC_TS_INVALID;
}

PlaylistManager::~PlaylistManager   ()
{
    delete streamFactory;
    unsetPeriod();
    delete playlist;
    delete conManager;
    delete logic;
    delete authStorage;
    vlc_cond_destroy(&waitcond);
    vlc_mutex_destroy(&lock);
    vlc_mutex_destroy(&demux.lock);
    vlc_cond_destroy(&demux.cond);
    vlc_mutex_destroy(&cached.lock);
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
    if(!conManager &&
       !(conManager =
         new (std::nothrow) HTTPConnectionManager(VLC_OBJECT(p_demux->s), authStorage))
      )
        return false;

    if(!setupPeriod())
        return false;

    playlist->playbackStart.Set(time(NULL));
    nextPlaylistupdate = playlist->playbackStart.Get();

    updateControlsContentType();
    updateControlsPosition();

    b_thread = !vlc_clone(&thread, managerThread,
                          static_cast<void *>(this), VLC_THREAD_PRIORITY_INPUT);
    if(!b_thread)
        return false;

    setBufferingRunState(true);

    return true;
}

void PlaylistManager::stop()
{
    if(b_thread)
    {
        vlc_cancel(thread);
        vlc_join(thread, NULL);
        b_thread = false;
    }
}

struct PrioritizedAbstractStream
{
    AbstractStream::buffering_status status;
    mtime_t demuxed_amount;
    AbstractStream *st;
};

static bool streamCompare(const PrioritizedAbstractStream &a,  const PrioritizedAbstractStream &b)
{
    if( a.status >= b.status ) /* Highest prio is higer value in enum */
    {
        if ( a.status == b.status ) /* Highest prio is lowest buffering */
           return a.demuxed_amount < b.demuxed_amount;
        else
            return true;
    }
    return false;
}

AbstractStream::buffering_status PlaylistManager::bufferize(mtime_t i_nzdeadline,
                                                            unsigned i_min_buffering, unsigned i_extra_buffering)
{
    AbstractStream::buffering_status i_return = AbstractStream::buffering_end;

    /* First reorder by status >> buffering level */
    std::vector<PrioritizedAbstractStream> prioritized_streams(streams.size());
    std::vector<PrioritizedAbstractStream>::iterator it = prioritized_streams.begin();
    std::vector<AbstractStream *>::iterator sit = streams.begin();
    for( ; sit!=streams.end(); ++sit)
    {
        PrioritizedAbstractStream &p = *it;
        p.st = *sit;
        p.status = p.st->getLastBufferStatus();
        p.demuxed_amount = p.st->getDemuxedAmount();
        ++it;
    }
    std::sort(prioritized_streams.begin(), prioritized_streams.end(), streamCompare);

    for(it=prioritized_streams.begin(); it!=prioritized_streams.end(); ++it)
    {
        AbstractStream *st = (*it).st;

        if (st->isDisabled() &&
            (!st->isSelected() || !st->canActivate() || !reactivateStream(st)))
                continue;

        AbstractStream::buffering_status i_ret = st->bufferize(i_nzdeadline, i_min_buffering, i_extra_buffering);
        if(i_return != AbstractStream::buffering_ongoing) /* Buffering streams need to keep going */
        {
            if(i_ret > i_return)
                i_return = i_ret;
        }

        /* Bail out, will start again (high prio could be same starving stream) */
        if( i_return == AbstractStream::buffering_lessthanmin )
            break;
    }

    vlc_mutex_lock(&demux.lock);
    if(demux.i_nzpcr == VLC_TS_INVALID &&
       i_return != AbstractStream::buffering_lessthanmin /* prevents starting before buffering is reached */ )
    {
        demux.i_nzpcr = getFirstDTS();
    }
    vlc_mutex_unlock(&demux.lock);

    return i_return;
}

AbstractStream::status PlaylistManager::dequeue(mtime_t i_floor, mtime_t *pi_nzbarrier)
{
    AbstractStream::status i_return = AbstractStream::status_eof;

    const mtime_t i_nzdeadline = *pi_nzbarrier;

    std::vector<AbstractStream *>::iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        AbstractStream *st = *it;

        mtime_t i_pcr;
        AbstractStream::status i_ret = st->dequeue(i_nzdeadline, &i_pcr);
        if( i_ret > i_return )
            i_return = i_ret;

        if( i_pcr > i_floor )
            *pi_nzbarrier = std::min( *pi_nzbarrier, i_pcr - VLC_TS_0 );
    }

    return i_return;
}

void PlaylistManager::drain()
{
    for(;;)
    {
        bool b_drained = true;
        std::vector<AbstractStream *>::iterator it;
        for(it=streams.begin(); it!=streams.end(); ++it)
        {
            AbstractStream *st = *it;

            if (st->isDisabled())
                continue;

            b_drained &= st->decodersDrained();
        }

        if(b_drained)
            break;

        msleep(20*1000); /* ugly, but we have no way to get feedback */
    }
    es_out_Control(p_demux->out, ES_OUT_RESET_PCR);
}

mtime_t PlaylistManager::getPCR() const
{
    mtime_t minpcr = VLC_TS_INVALID;
    std::vector<AbstractStream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        const mtime_t pcr = (*it)->getPCR();
        if(minpcr == VLC_TS_INVALID)
            minpcr = pcr;
        else if(pcr > VLC_TS_INVALID)
            minpcr = std::min(minpcr, pcr);
    }
    return minpcr;
}

mtime_t PlaylistManager::getFirstDTS() const
{
    mtime_t mindts = VLC_TS_INVALID;
    std::vector<AbstractStream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        const mtime_t dts = (*it)->getFirstDTS();
        if(mindts == VLC_TS_INVALID)
            mindts = dts;
        else if(dts > VLC_TS_INVALID)
            mindts = std::min(mindts, dts);
    }
    return mindts;
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
    bool hasValidStream = false;
    for(int real = 0; real < 2; real++)
    {
        /* Always probe if we can seek first */
        std::vector<AbstractStream *>::iterator it;
        for(it=streams.begin(); it!=streams.end(); ++it)
        {
            AbstractStream *st = *it;
            if(!st->isDisabled())
            {
                hasValidStream = true;
                ret &= st->setPosition(time, !real);
            }
        }
        if(!ret)
            break;
    }
    if(!hasValidStream)
    {
        msg_Warn(p_demux, "there is no valid streams");
        ret = false;
    }
    return ret;
}

bool PlaylistManager::needsUpdate() const
{
    return playlist->isLive() && (failedupdates < 3);
}

void PlaylistManager::scheduleNextUpdate()
{

}

bool PlaylistManager::updatePlaylist()
{
    std::vector<AbstractStream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
        (*it)->runUpdates();

    updateControlsContentType();
    updateControlsPosition();
    return true;
}

mtime_t PlaylistManager::getFirstPlaybackTime() const
{
    return 0;
}

mtime_t PlaylistManager::getCurrentPlaybackTime() const
{
    return demux.i_nzpcr;
}

void PlaylistManager::pruneLiveStream()
{
    mtime_t minValidPos = 0;
    std::vector<AbstractStream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); it++)
    {
        const AbstractStream *st = *it;
        if(st->isDisabled() || !st->isSelected())
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
    vlc_mutex_lock(&demux.lock);
    if(demux.i_nzpcr == VLC_TS_INVALID)
    {
        bool b_dead = true;
        std::vector<AbstractStream *>::const_iterator it;
        for(it=streams.begin(); it!=streams.end(); ++it)
            b_dead &= !(*it)->canActivate();
        if(!b_dead)
            vlc_cond_timedwait(&demux.cond, &demux.lock, mdate() + CLOCK_FREQ / 20);
        vlc_mutex_unlock(&demux.lock);
        return (b_dead) ? AbstractStream::status_eof : AbstractStream::status_buffering;
    }

    if(demux.i_firstpcr == VLC_TS_INVALID)
        demux.i_firstpcr = demux.i_nzpcr;

    mtime_t i_nzbarrier = demux.i_nzpcr + increment;
    vlc_mutex_unlock(&demux.lock);

    AbstractStream::status status = dequeue(demux.i_nzpcr, &i_nzbarrier);

    updateControlsContentType();
    updateControlsPosition();

    switch(status)
    {
    case AbstractStream::status_eof:
        {
            /* might be end of current period */
            if(currentPeriod)
            {
                setBufferingRunState(false);
                BasePeriod *nextPeriod = playlist->getNextPeriod(currentPeriod);
                if(!nextPeriod)
                    return VLC_DEMUXER_EOF;
                unsetPeriod();
                currentPeriod = nextPeriod;
                if (!setupPeriod())
                    return VLC_DEMUXER_EOF;

                demux.i_nzpcr = VLC_TS_INVALID;
                demux.i_firstpcr = VLC_TS_INVALID;
                es_out_Control(p_demux->out, ES_OUT_RESET_PCR);

                setBufferingRunState(true);
            }
        }
        break;
    case AbstractStream::status_buffering:
        vlc_mutex_lock(&demux.lock);
        vlc_cond_timedwait(&demux.cond, &demux.lock, mdate() + CLOCK_FREQ / 20);
        vlc_mutex_unlock(&demux.lock);
        break;
    case AbstractStream::status_discontinuity:
        vlc_mutex_lock(&demux.lock);
        demux.i_nzpcr = VLC_TS_INVALID;
        demux.i_firstpcr = VLC_TS_INVALID;
        es_out_Control(p_demux->out, ES_OUT_RESET_PCR);
        vlc_mutex_unlock(&demux.lock);
        break;
    case AbstractStream::status_demuxed:
        vlc_mutex_lock(&demux.lock);
        if( demux.i_nzpcr != VLC_TS_INVALID && i_nzbarrier != demux.i_nzpcr )
        {
            demux.i_nzpcr = i_nzbarrier;
            mtime_t pcr = VLC_TS_0 + std::max(INT64_C(0), demux.i_nzpcr - CLOCK_FREQ / 10);
            es_out_Control(p_demux->out, ES_OUT_SET_GROUP_PCR, 0, pcr);
        }
        vlc_mutex_unlock(&demux.lock);
        break;
    }

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
        {
            vlc_mutex_locker locker(&cached.lock);
            *(va_arg (args, bool *)) = ! cached.b_live;
            break;
        }

        case DEMUX_CAN_CONTROL_PACE:
            *(va_arg (args, bool *)) = true;
            break;

        case DEMUX_CAN_PAUSE:
        {
            /* Always return true then fail late.
             * See demux.c/demux_vaControl,
             * misleading and should be DEMUX_CAN_CONTROL_PAUSE */
            *(va_arg (args, bool *)) = true;
            break;
        }

        case DEMUX_SET_PAUSE_STATE:
        {
            vlc_mutex_locker locker(&cached.lock);
            return cached.b_live ? VLC_EGENERIC : VLC_SUCCESS;
        }

        case DEMUX_GET_TIME:
        {
            vlc_mutex_locker locker(&cached.lock);
            *(va_arg (args, int64_t *)) = cached.i_time;
            break;
        }

        case DEMUX_GET_LENGTH:
        {
            vlc_mutex_locker locker(&cached.lock);
            if(cached.b_live)
                return VLC_EGENERIC;
            *(va_arg (args, int64_t *)) = cached.i_length;
            break;
        }

        case DEMUX_GET_POSITION:
        {
            vlc_mutex_locker locker(&cached.lock);
            if(cached.b_live)
                return VLC_EGENERIC;
            *(va_arg (args, double *)) = cached.f_position;
            break;
        }

        case DEMUX_SET_POSITION:
        {
            setBufferingRunState(false); /* stop downloader first */

            const mtime_t i_duration = getDuration();
            if(i_duration == 0) /* == playlist->isLive() */
            {
                setBufferingRunState(true);
                return VLC_EGENERIC;
            }

            int64_t time = i_duration * va_arg(args, double);
            time += getFirstPlaybackTime();

            if(!setPosition(time))
            {
                setBufferingRunState(true);
                return VLC_EGENERIC;
            }

            demux.i_nzpcr = VLC_TS_INVALID;
            setBufferingRunState(true);
            break;
        }

        case DEMUX_SET_TIME:
        {
            setBufferingRunState(false); /* stop downloader first */
            if(playlist->isLive())
            {
                setBufferingRunState(true);
                return VLC_EGENERIC;
            }

            int64_t time = va_arg(args, int64_t);// + getFirstPlaybackTime();
            if(!setPosition(time))
            {
                setBufferingRunState(true);
                return VLC_EGENERIC;
            }

            demux.i_nzpcr = VLC_TS_INVALID;
            setBufferingRunState(true);
            break;
        }

        case DEMUX_GET_PTS_DELAY:
            *va_arg (args, int64_t *) = 1000 * INT64_C(1000);
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

void PlaylistManager::setBufferingRunState(bool b)
{
    vlc_mutex_lock(&lock);
    b_buffering = b;
    vlc_cond_signal(&waitcond);
    vlc_mutex_unlock(&lock);
}

void PlaylistManager::Run()
{
    vlc_mutex_lock(&lock);
    const unsigned i_min_buffering = playlist->getMinBuffering();
    const unsigned i_extra_buffering = playlist->getMaxBuffering() - i_min_buffering;
    while(1)
    {
        mutex_cleanup_push(&lock);
        while(!b_buffering)
            vlc_cond_wait(&waitcond, &lock);
        vlc_testcancel();
        vlc_cleanup_pop();

        if(needsUpdate())
        {
            int canc = vlc_savecancel();
            if(updatePlaylist())
                scheduleNextUpdate();
            else
                failedupdates++;
            vlc_restorecancel(canc);
        }

        vlc_mutex_lock(&demux.lock);
        mtime_t i_nzpcr = demux.i_nzpcr;
        vlc_mutex_unlock(&demux.lock);

        int canc = vlc_savecancel();
        AbstractStream::buffering_status i_return = bufferize(i_nzpcr, i_min_buffering, i_extra_buffering);
        vlc_restorecancel( canc );

        if(i_return != AbstractStream::buffering_lessthanmin)
        {
            mtime_t i_deadline = mdate();
            if(i_return == AbstractStream::buffering_ongoing)
                i_deadline += (CLOCK_FREQ / 100);
            else if(i_return == AbstractStream::buffering_full)
                i_deadline += (CLOCK_FREQ / 10);
            else if(i_return == AbstractStream::buffering_end)
                i_deadline += (CLOCK_FREQ);
            else /*if(i_return == AbstractStream::buffering_suspended)*/
                i_deadline += (CLOCK_FREQ / 4);

            vlc_mutex_lock(&demux.lock);
            vlc_cond_signal(&demux.cond);
            vlc_mutex_unlock(&demux.lock);

            mutex_cleanup_push(&lock);
            while(b_buffering &&
                    vlc_cond_timedwait(&waitcond, &lock, i_deadline) == 0 &&
                    i_deadline > mdate());
            vlc_cleanup_pop();
        }
    }
    vlc_mutex_unlock(&lock);
}

void * PlaylistManager::managerThread(void *opaque)
{
    static_cast<PlaylistManager *>(opaque)->Run();
    return NULL;
}

void PlaylistManager::updateControlsPosition()
{
    vlc_mutex_locker locker(&cached.lock);
    const mtime_t i_duration = cached.i_length;
    if(i_duration == 0)
    {
        cached.f_position = 0.0;
    }
    else
    {
        const mtime_t i_length = getCurrentPlaybackTime() - getFirstPlaybackTime();
        cached.f_position = (double) i_length / i_duration;
    }

    mtime_t i_time = getCurrentPlaybackTime();
    if(!playlist->isLive())
        i_time -= getFirstPlaybackTime();
    cached.i_time = i_time;
}

void PlaylistManager::updateControlsContentType()
{
    vlc_mutex_locker locker(&cached.lock);
    if(playlist->isLive())
    {
        cached.b_live = true;
        cached.i_length = 0;
    }
    else
    {
        cached.b_live = false;
        cached.i_length = getDuration();
    }
}

AbstractAdaptationLogic *PlaylistManager::createLogic(AbstractAdaptationLogic::LogicType type, AbstractConnectionManager *conn)
{
    AbstractAdaptationLogic *logic = NULL;
    switch(type)
    {
        case AbstractAdaptationLogic::FixedRate:
        {
            size_t bps = var_InheritInteger(p_demux, "adaptive-bw") * 8192;
            logic = new (std::nothrow) FixedRateAdaptationLogic(bps);
            break;
        }
        case AbstractAdaptationLogic::AlwaysLowest:
            logic = new (std::nothrow) AlwaysLowestAdaptationLogic();
            break;
        case AbstractAdaptationLogic::AlwaysBest:
            logic = new (std::nothrow) AlwaysBestAdaptationLogic();
            break;
        case AbstractAdaptationLogic::RateBased:
        {
            RateBasedAdaptationLogic *ratelogic =
                    new (std::nothrow) RateBasedAdaptationLogic(VLC_OBJECT(p_demux));
            if(ratelogic)
                conn->setDownloadRateObserver(ratelogic);
            logic = ratelogic;
            break;
        }
        case AbstractAdaptationLogic::Default:
        case AbstractAdaptationLogic::NearOptimal:
        {
            NearOptimalAdaptationLogic *noplogic =
                    new (std::nothrow) NearOptimalAdaptationLogic(VLC_OBJECT(p_demux));
            if(noplogic)
                conn->setDownloadRateObserver(noplogic);
            logic = noplogic;
            break;
        }
        case AbstractAdaptationLogic::Predictive:
        {
            AbstractAdaptationLogic *predictivelogic =
                    new (std::nothrow) PredictiveAdaptationLogic(VLC_OBJECT(p_demux));
            if(predictivelogic)
                conn->setDownloadRateObserver(predictivelogic);
            logic = predictivelogic;
        }

        default:
            break;
    }

    if(logic)
    {
        logic->setMaxDeviceResolution( var_InheritInteger(p_demux, "adaptive-maxwidth"),
                                       var_InheritInteger(p_demux, "adaptive-maxheight") );
    }

    return logic;
}
