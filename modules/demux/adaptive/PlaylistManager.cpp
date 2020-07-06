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
#include "SharedResources.hpp"
#include "playlist/AbstractPlaylist.hpp"
#include "playlist/BasePeriod.h"
#include "playlist/BaseAdaptationSet.h"
#include "playlist/BaseRepresentation.h"
#include "http/HTTPConnectionManager.h"
#include "logic/AlwaysBestAdaptationLogic.h"
#include "logic/RateBasedAdaptationLogic.h"
#include "logic/AlwaysLowestAdaptationLogic.hpp"
#include "logic/PredictiveAdaptationLogic.hpp"
#include "logic/NearOptimalAdaptationLogic.hpp"
#include "logic/BufferingLogic.hpp"
#include "tools/Debug.hpp"
#ifdef ADAPTIVE_DEBUGGING_LOGIC
# include "logic/RoundRobinLogic.hpp"
#endif
#include <vlc_stream.h>
#include <vlc_demux.h>
#include <vlc_threads.h>

#include <algorithm>
#include <ctime>

using namespace adaptive::http;
using namespace adaptive::logic;
using namespace adaptive;

PlaylistManager::PlaylistManager( demux_t *p_demux_,
                                  SharedResources *res,
                                  AbstractPlaylist *pl,
                                  AbstractStreamFactory *factory,
                                  AbstractAdaptationLogic::LogicType type ) :
             logicType      ( type ),
             logic          ( NULL ),
             playlist       ( pl ),
             streamFactory  ( factory ),
             p_demux        ( p_demux_ )
{
    currentPeriod = playlist->getFirstPeriod();
    resources = res;
    bufferingLogic = NULL;
    failedupdates = 0;
    b_thread = false;
    b_buffering = false;
    b_canceled = false;
    nextPlaylistupdate = 0;
    demux.i_nzpcr = VLC_TICK_INVALID;
    demux.i_firstpcr = VLC_TICK_INVALID;
    vlc_mutex_init(&demux.lock);
    vlc_cond_init(&demux.cond);
    vlc_mutex_init(&lock);
    vlc_cond_init(&waitcond);
    vlc_mutex_init(&cached.lock);
    cached.b_live = false;
    cached.f_position = 0.0;
    cached.i_time = VLC_TICK_INVALID;
    cached.playlistStart = 0;
    cached.playlistEnd = 0;
    cached.playlistLength = 0;
    cached.lastupdate = 0;
}

PlaylistManager::~PlaylistManager   ()
{
    delete streamFactory;
    unsetPeriod();
    delete playlist;
    delete logic;
    delete resources;
    delete bufferingLogic;
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

    if(!logic && !(logic = createLogic(logicType, resources->getConnManager())))
        return false;

    if(!bufferingLogic && !(bufferingLogic = createBufferingLogic()))
        return false;

    std::vector<BaseAdaptationSet*> sets = currentPeriod->getAdaptationSets();
    std::vector<BaseAdaptationSet*>::iterator it;
    for(it=sets.begin();it!=sets.end();++it)
    {
        BaseAdaptationSet *set = *it;
        if(set && streamFactory)
        {
            SegmentTracker *tracker = new SegmentTracker(resources, logic,
                                                         bufferingLogic, set);
            if(!tracker)
                continue;

            AbstractStream *st = streamFactory->create(p_demux, set->getStreamFormat(),
                                                       tracker, resources->getConnManager());
            if(!st)
            {
                delete tracker;
                continue;
            }

            streams.push_back(st);

            /* Generate stream description */
            if(!set->getLang().empty())
                st->setLanguage(set->getLang());

            if(!set->description.Get().empty())
                st->setDescription(set->description.Get());
        }
    }
    return true;
}

bool PlaylistManager::init()
{
    if(!setupPeriod())
        return false;

    playlist->playbackStart.Set(time(NULL));
    nextPlaylistupdate = playlist->playbackStart.Get();

    updateControlsPosition();

    return true;
}

bool PlaylistManager::start()
{
    if(b_thread)
        return false;

    b_thread = !vlc_clone(&thread, managerThread,
                          static_cast<void *>(this), VLC_THREAD_PRIORITY_INPUT);
    if(!b_thread)
        return false;

    setBufferingRunState(true);

    return true;
}

bool PlaylistManager::started() const
{
    return b_thread;
}

void PlaylistManager::stop()
{
    if(b_thread)
    {
        vlc_mutex_lock(&lock);
        b_canceled = true;
        vlc_cond_signal(&waitcond);
        vlc_mutex_unlock(&lock);

        vlc_join(thread, NULL);
        b_thread = false;
    }
}

struct PrioritizedAbstractStream
{
    AbstractStream::buffering_status status;
    vlc_tick_t demuxed_amount;
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

AbstractStream::buffering_status PlaylistManager::bufferize(vlc_tick_t i_nzdeadline,
                                                            vlc_tick_t i_min_buffering, vlc_tick_t i_extra_buffering)
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

        if(!st->isValid())
            continue;

        if(st->esCount())
        {
            if (st->isDisabled() &&
                (!st->isSelected() || !reactivateStream(st)))
                  continue;
        }
        else
        {
            /* initial */
        }

        AbstractStream::buffering_status i_ret = st->bufferize(i_nzdeadline,
                                                               i_min_buffering,
                                                               i_extra_buffering,
                                                               getActiveStreamsCount() <= 1);
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
    if(demux.i_nzpcr == VLC_TICK_INVALID &&
       i_return != AbstractStream::buffering_lessthanmin /* prevents starting before buffering is reached */ )
    {
        demux.i_nzpcr = getFirstDTS();
    }
    vlc_mutex_unlock(&demux.lock);

    return i_return;
}

AbstractStream::status PlaylistManager::dequeue(vlc_tick_t i_floor, vlc_tick_t *pi_nzbarrier)
{
    AbstractStream::status i_return = AbstractStream::status_eof;

    const vlc_tick_t i_nzdeadline = *pi_nzbarrier;

    std::vector<AbstractStream *>::iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        AbstractStream *st = *it;

        vlc_tick_t i_pcr;
        AbstractStream::status i_ret = st->dequeue(i_nzdeadline, &i_pcr);
        if( i_ret > i_return )
            i_return = i_ret;

        if( i_pcr > i_floor )
            *pi_nzbarrier = std::min( *pi_nzbarrier, i_pcr - VLC_TICK_0 );
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

            if (!st->isValid() || st->isDisabled())
                continue;

            b_drained &= st->decodersDrained();
        }

        if(b_drained)
            break;

        vlc_tick_sleep(VLC_TICK_FROM_MS(20)); /* ugly, but we have no way to get feedback */
    }
    es_out_Control(p_demux->out, ES_OUT_RESET_PCR);
}

vlc_tick_t PlaylistManager::getResumeTime() const
{
    vlc_mutex_locker locker(&demux.lock);
    return demux.i_nzpcr;
}

vlc_tick_t PlaylistManager::getFirstDTS() const
{
    vlc_tick_t mindts = VLC_TICK_INVALID;
    std::vector<AbstractStream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        const vlc_tick_t dts = (*it)->getFirstDTS();
        if(mindts == VLC_TICK_INVALID)
            mindts = dts;
        else if(dts != VLC_TICK_INVALID)
            mindts = std::min(mindts, dts);
    }
    return mindts;
}

unsigned PlaylistManager::getActiveStreamsCount() const
{
    unsigned count = 0;
    std::vector<AbstractStream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        if((*it)->isValid() && !(*it)->isDisabled())
            count++;
    }
    return count;
}

bool PlaylistManager::setPosition(vlc_tick_t time)
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
            if(st->isValid() && !st->isDisabled())
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
    return playlist->needsUpdates() &&
           playlist->isLive() && (failedupdates < 3);
}

void PlaylistManager::scheduleNextUpdate()
{

}

bool PlaylistManager::updatePlaylist()
{
    std::vector<AbstractStream *>::const_iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
        (*it)->runUpdates();

    updateControlsPosition();
    return true;
}

vlc_tick_t PlaylistManager::getFirstPlaybackTime() const
{
    return 0;
}

vlc_tick_t PlaylistManager::getCurrentDemuxTime() const
{
    vlc_mutex_locker locker(&demux.lock);
    return demux.i_nzpcr;
}

bool PlaylistManager::reactivateStream(AbstractStream *stream)
{
    return stream->reactivate(getResumeTime());
}

#define DEMUX_INCREMENT VLC_TICK_FROM_MS(50)
int PlaylistManager::demux_callback(demux_t *p_demux)
{
    PlaylistManager *manager = reinterpret_cast<PlaylistManager *>(p_demux->p_sys);
    if(!manager->started() && !manager->start())
        return VLC_DEMUXER_EOF;
    return manager->doDemux(DEMUX_INCREMENT);
}

int PlaylistManager::doDemux(vlc_tick_t increment)
{
    vlc_mutex_lock(&demux.lock);
    if(demux.i_nzpcr == VLC_TICK_INVALID)
    {
        bool b_dead = true;
        bool b_all_disabled = true;
        std::vector<AbstractStream *>::const_iterator it;
        for(it=streams.begin(); it!=streams.end(); ++it)
        {
            b_dead &= !(*it)->isValid();
            b_all_disabled &= (*it)->isDisabled();
        }
        if(!b_dead)
            vlc_cond_timedwait(&demux.cond, &demux.lock, vlc_tick_now() + VLC_TICK_FROM_MS(50));
        vlc_mutex_unlock(&demux.lock);
        return (b_dead || b_all_disabled) ? AbstractStream::status_eof : AbstractStream::status_buffering;
    }

    if(demux.i_firstpcr == VLC_TICK_INVALID)
        demux.i_firstpcr = demux.i_nzpcr;

    vlc_tick_t i_nzbarrier = demux.i_nzpcr + increment;
    vlc_mutex_unlock(&demux.lock);

    AbstractStream::status status = dequeue(demux.i_nzpcr, &i_nzbarrier);

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

                demux.i_nzpcr = VLC_TICK_INVALID;
                demux.i_firstpcr = VLC_TICK_INVALID;
                es_out_Control(p_demux->out, ES_OUT_RESET_PCR);

                setBufferingRunState(true);
            }
        }
        break;
    case AbstractStream::status_buffering:
        vlc_mutex_lock(&demux.lock);
        vlc_cond_timedwait(&demux.cond, &demux.lock, vlc_tick_now() + VLC_TICK_FROM_MS(50));
        vlc_mutex_unlock(&demux.lock);
        break;
    case AbstractStream::status_discontinuity:
        vlc_mutex_lock(&demux.lock);
        demux.i_nzpcr = VLC_TICK_INVALID;
        demux.i_firstpcr = VLC_TICK_INVALID;
        es_out_Control(p_demux->out, ES_OUT_RESET_PCR);
        vlc_mutex_unlock(&demux.lock);
        break;
    case AbstractStream::status_demuxed:
        vlc_mutex_lock(&demux.lock);
        if( demux.i_nzpcr != VLC_TICK_INVALID && i_nzbarrier != demux.i_nzpcr )
        {
            demux.i_nzpcr = i_nzbarrier;
            vlc_tick_t pcr = VLC_TICK_0 + std::max(INT64_C(0), demux.i_nzpcr - VLC_TICK_FROM_MS(100));
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
            *(va_arg (args, vlc_tick_t *)) = cached.i_time;
            break;
        }

        case DEMUX_GET_LENGTH:
        {
            vlc_mutex_locker locker(&cached.lock);
            if(cached.b_live && cached.playlistLength == 0)
                return VLC_EGENERIC;
            *(va_arg (args, vlc_tick_t *)) = cached.playlistLength;
            break;
        }

        case DEMUX_GET_POSITION:
        {
            vlc_mutex_locker locker(&cached.lock);
            if(cached.b_live && cached.playlistLength == 0)
                return VLC_EGENERIC;
            *(va_arg (args, double *)) = cached.f_position;
            break;
        }

        case DEMUX_SET_POSITION:
        {
            setBufferingRunState(false); /* stop downloader first */
            vlc_mutex_locker locker(&cached.lock);

            if(cached.playlistLength == 0)
            {
                setBufferingRunState(true);
                return VLC_EGENERIC;
            }

            double pos = va_arg(args, double);
            vlc_tick_t seekTime = cached.playlistStart + cached.playlistLength * pos;

            SeekDebug(msg_Dbg(p_demux, "Seek %f to %ld plstart %ld duration %ld",
                   pos, seekTime, cached.playlistEnd, cached.playlistLength));

            if(!setPosition(seekTime))
            {
                setBufferingRunState(true);
                return VLC_EGENERIC;
            }

            demux.i_nzpcr = VLC_TICK_INVALID;
            cached.lastupdate = 0;
            setBufferingRunState(true);
            break;
        }

        case DEMUX_SET_TIME:
        {
            setBufferingRunState(false); /* stop downloader first */

            vlc_tick_t time = va_arg(args, vlc_tick_t);// + getFirstPlaybackTime();
            if(!setPosition(time))
            {
                setBufferingRunState(true);
                return VLC_EGENERIC;
            }

            vlc_mutex_locker locker(&cached.lock);
            demux.i_nzpcr = VLC_TICK_INVALID;
            cached.lastupdate = 0;
            setBufferingRunState(true);
            break;
        }

        case DEMUX_GET_PTS_DELAY:
            *va_arg (args, vlc_tick_t *) = VLC_TICK_FROM_SEC(1);
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
    const vlc_tick_t i_min_buffering = bufferingLogic->getMinBuffering(playlist);
    const vlc_tick_t i_extra_buffering = bufferingLogic->getMaxBuffering(playlist) - i_min_buffering;
    while(1)
    {
        while(!b_buffering && !b_canceled)
            vlc_cond_wait(&waitcond, &lock);
        if (b_canceled)
            break;

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
        vlc_tick_t i_nzpcr = demux.i_nzpcr;
        vlc_mutex_unlock(&demux.lock);

        int canc = vlc_savecancel();
        AbstractStream::buffering_status i_return = bufferize(i_nzpcr, i_min_buffering, i_extra_buffering);
        vlc_restorecancel( canc );

        if(i_return != AbstractStream::buffering_lessthanmin)
        {
            vlc_tick_t i_deadline = vlc_tick_now();
            if(i_return == AbstractStream::buffering_ongoing)
                i_deadline += VLC_TICK_FROM_MS(10);
            else if(i_return == AbstractStream::buffering_full)
                i_deadline += VLC_TICK_FROM_MS(100);
            else if(i_return == AbstractStream::buffering_end)
                i_deadline += VLC_TICK_FROM_SEC(1);
            else /*if(i_return == AbstractStream::buffering_suspended)*/
                i_deadline += VLC_TICK_FROM_MS(250);

            vlc_mutex_lock(&demux.lock);
            vlc_cond_signal(&demux.cond);
            vlc_mutex_unlock(&demux.lock);

            while(b_buffering &&
                    vlc_cond_timedwait(&waitcond, &lock, i_deadline) == 0 &&
                    i_deadline > vlc_tick_now() &&
                    !b_canceled);
            if (b_canceled)
                break;
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

    time_t now = time(NULL);
    if(now - cached.lastupdate < 1)
        return;
    cached.lastupdate = now;

    vlc_tick_t rapPlaylistStart = 0;
    vlc_tick_t rapDemuxStart = 0;
    std::vector<AbstractStream *>::iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        AbstractStream *st = *it;
        if(st->isValid() && !st->isDisabled() && st->isSelected())
        {
            if(st->getMediaPlaybackTimes(&cached.playlistStart, &cached.playlistEnd,
                                         &cached.playlistLength,
                                         &rapPlaylistStart, &rapDemuxStart))
                break;
        }
    }

    /*
     * Relative position:
     * -> Elapsed demux time (current demux time - first demux time)
     * Since PlaylistTime != DemuxTime (HLS crap, TS):
     * -> Use Playlist<->Demux time offset provided by EsOut
     *    to convert to elapsed playlist time.
     *    But this diff is not available until we have demuxed data...
     *    Fallback on relative seek from playlist start in that case
     * But also playback might not have started at beginning of playlist
     * -> Apply relative start from seek point (set on EsOut as ExpectedTime)
     *
     * All seeks need to be done in playlist time !
     */

    vlc_tick_t currentDemuxTime = getCurrentDemuxTime();
    cached.b_live = playlist->isLive();

    SeekDebug(msg_Dbg(p_demux, "playlist Start/End %ld/%ld len %ld"
                               "rap pl/demux (%ld/%ld)",
                      cached.playlistStart, cached.playlistEnd, cached.playlistEnd,
                      rapPlaylistStart, rapDemuxStart));

    if(cached.b_live)
    {
        /* Special case for live until we can provide relative start to fully match
           the above description */
        cached.i_time = currentDemuxTime;

        if(cached.playlistStart != cached.playlistEnd)
        {
            if(cached.playlistStart < 0) /* Live template. Range start = now() - buffering depth */
            {
                cached.playlistEnd = vlc_tick_from_sec(now);
                cached.playlistStart = cached.playlistEnd - cached.playlistLength;
            }
        }
        const vlc_tick_t currentTime = getCurrentDemuxTime();
        if(currentTime > cached.playlistStart &&
           currentTime <= cached.playlistEnd && cached.playlistLength)
        {
            cached.f_position = ((double)(currentTime - cached.playlistStart)) / cached.playlistLength;
        }
        else
        {
            cached.f_position = 0.0;
        }
    }
    else
    {
        if(playlist->duration.Get() > cached.playlistLength)
            cached.playlistLength = playlist->duration.Get();

        if(cached.playlistLength && currentDemuxTime)
        {
            /* convert to playlist time */
            vlc_tick_t rapRelOffset = currentDemuxTime - rapDemuxStart; /* offset from start/seek */
            vlc_tick_t absPlaylistTime = rapPlaylistStart + rapRelOffset; /* converted as abs playlist time */
            vlc_tick_t relMediaTime = absPlaylistTime - cached.playlistStart; /* elapsed, in playlist time */
            cached.i_time = absPlaylistTime;
            cached.f_position = (double) relMediaTime / cached.playlistLength;
        }
        else
        {
            cached.f_position = 0.0;
        }
    }

    SeekDebug(msg_Dbg(p_demux, "cached.i_time (%ld) cur %ld rap start (pl %ld/dmx %ld)",
               cached.i_time, currentDemuxTime, rapPlaylistStart, rapDemuxStart));
}

AbstractAdaptationLogic *PlaylistManager::createLogic(AbstractAdaptationLogic::LogicType type, AbstractConnectionManager *conn)
{
    vlc_object_t *obj = VLC_OBJECT(p_demux);
    AbstractAdaptationLogic *logic = NULL;
    switch(type)
    {
        case AbstractAdaptationLogic::FixedRate:
        {
            size_t bps = var_InheritInteger(p_demux, "adaptive-bw") * 8192;
            logic = new (std::nothrow) FixedRateAdaptationLogic(obj, bps);
            break;
        }
        case AbstractAdaptationLogic::AlwaysLowest:
            logic = new (std::nothrow) AlwaysLowestAdaptationLogic(obj);
            break;
        case AbstractAdaptationLogic::AlwaysBest:
            logic = new (std::nothrow) AlwaysBestAdaptationLogic(obj);
            break;
        case AbstractAdaptationLogic::RateBased:
        {
            RateBasedAdaptationLogic *ratelogic =
                    new (std::nothrow) RateBasedAdaptationLogic(obj);
            if(ratelogic)
                conn->setDownloadRateObserver(ratelogic);
            logic = ratelogic;
            break;
        }
        case AbstractAdaptationLogic::Default:
#ifdef ADAPTIVE_DEBUGGING_LOGIC
            logic = new (std::nothrow) RoundRobinLogic(obj);
            msg_Warn(p_demux, "using RoundRobinLogic every %u", RoundRobinLogic::QUANTUM);
            break;
#endif
        case AbstractAdaptationLogic::NearOptimal:
        {
            NearOptimalAdaptationLogic *noplogic =
                    new (std::nothrow) NearOptimalAdaptationLogic(obj);
            if(noplogic)
                conn->setDownloadRateObserver(noplogic);
            logic = noplogic;
            break;
        }
        case AbstractAdaptationLogic::Predictive:
        {
            AbstractAdaptationLogic *predictivelogic =
                    new (std::nothrow) PredictiveAdaptationLogic(obj);
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

AbstractBufferingLogic *PlaylistManager::createBufferingLogic() const
{
    DefaultBufferingLogic *bl = new DefaultBufferingLogic();
    if(bl)
    {
        unsigned v = var_InheritInteger(p_demux, "adaptive-livedelay");
        if(v)
            bl->setUserLiveDelay(VLC_TICK_FROM_MS(v));
        v = var_InheritInteger(p_demux, "adaptive-maxbuffer");
        if(v)
            bl->setUserMaxBuffering(VLC_TICK_FROM_MS(v));
    }
    return bl;
}
