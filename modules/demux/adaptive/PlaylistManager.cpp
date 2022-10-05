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
#include "playlist/BasePlaylist.hpp"
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
#include <vlc_stream.h>
#include <vlc_demux.h>
#include <vlc_threads.h>

#include <algorithm>
#include <ctime>
#include <cassert>

using namespace adaptive::http;
using namespace adaptive::logic;
using namespace adaptive;

PlaylistManager::PlaylistManager( demux_t *p_demux_,
                                  SharedResources *res,
                                  BasePlaylist *pl,
                                  AbstractStreamFactory *factory,
                                  AbstractAdaptationLogic::LogicType type ) :
             logicType      ( type ),
             logic          ( nullptr ),
             playlist       ( pl ),
             streamFactory  ( factory ),
             p_demux        ( p_demux_ )
{
    currentPeriod = playlist->getFirstPeriod();
    resources = res;
    bufferingLogic = nullptr;
    failedupdates = 0;
    b_thread = false;
    b_buffering = false;
    b_canceled = false;
    b_preparsing = false;
    nextPlaylistupdate = 0;
    demux.pcr_syncpoint = TimestampSynchronizationPoint::RandomAccess;
    vlc_mutex_init(&demux.lock);
    vlc_cond_init(&demux.cond);
    vlc_mutex_init(&lock);
    vlc_cond_init(&waitcond);
    vlc_mutex_init(&cached.lock);
    cached.b_live = false;
    cached.f_position = 0.0;
    cached.i_time = VLC_TS_INVALID;
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

    if(!logic && !(logic = createLogic(logicType, resources->getConnManager())))
        return false;

    if(!bufferingLogic && !(bufferingLogic = createBufferingLogic()))
        return false;

    const std::vector<BaseAdaptationSet*> &sets = currentPeriod->getAdaptationSets();
    for(BaseAdaptationSet *set : sets)
    {
        if(set && streamFactory)
        {
            SegmentTracker *tracker = new SegmentTracker(resources, logic,
                                                         bufferingLogic, set,
                                                         &synchronizationReferences);
            if(!tracker)
                continue;

            AbstractStream *st = streamFactory->create(p_demux, set->getStreamFormat(),
                                                       tracker);
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

bool PlaylistManager::init(bool b_preparsing)
{
    this->b_preparsing = b_preparsing;

    if(!setupPeriod())
        return false;

    playlist->playbackStart.Set(time(nullptr));
    nextPlaylistupdate = playlist->playbackStart.Get();

    if(b_preparsing)
        preparsePlaylist();
    updateControlsPosition();

    return true;
}

bool PlaylistManager::start()
{
    if(b_thread || b_preparsing)
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

        vlc_join(thread, nullptr);
        b_thread = false;
    }
}

struct PrioritizedAbstractStream
{
    AbstractStream::BufferingStatus status;
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

AbstractStream::BufferingStatus PlaylistManager::bufferize(Times deadline,
                                                           mtime_t i_min_buffering,
                                                           mtime_t i_max_buffering,
                                                           mtime_t i_target_buffering)
{
    AbstractStream::BufferingStatus i_return = AbstractStream::BufferingStatus::End;

    /* First reorder by status >> buffering level */
    std::vector<PrioritizedAbstractStream> prioritized_streams(streams.size());
    std::vector<PrioritizedAbstractStream>::iterator it = prioritized_streams.begin();
    std::vector<AbstractStream *>::iterator sit = streams.begin();
    for( ; sit!=streams.end(); ++sit)
    {
        PrioritizedAbstractStream &p = *it;
        p.st = *sit;
        p.status = p.st->getBufferAndStatus(deadline, i_min_buffering, i_max_buffering, &p.demuxed_amount);
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

        AbstractStream::BufferingStatus i_ret = st->bufferize(deadline,
                                                               i_min_buffering,
                                                               i_max_buffering,
                                                               i_target_buffering,
                                                               getActiveStreamsCount() <= 1);
        if(i_return != AbstractStream::BufferingStatus::Ongoing) /* Buffering streams need to keep going */
        {
            if(i_ret > i_return)
                i_return = i_ret;
        }

        /* Bail out, will start again (high prio could be same starving stream) */
        if( i_return == AbstractStream::BufferingStatus::Lessthanmin )
            break;
    }

    vlc_mutex_lock(&demux.lock);
    if(demux.times.continuous == VLC_TS_INVALID &&
        /* don't wait minbuffer on simple discontinuity or restart */
       (demux.pcr_syncpoint == TimestampSynchronizationPoint::Discontinuity ||
        /* prevents starting before buffering is reached */
        i_return != AbstractStream::BufferingStatus::Lessthanmin ))
    {
        demux.times = getFirstTimes();
    }
    vlc_mutex_unlock(&demux.lock);

    return i_return;
}

AbstractStream::Status PlaylistManager::dequeue(Times floor, Times *barrier)
{
    AbstractStream::Status i_return = AbstractStream::Status::Eof;

    const Times deadline = *barrier;

    std::vector<AbstractStream *>::iterator it;
    for(it=streams.begin(); it!=streams.end(); ++it)
    {
        AbstractStream *st = *it;
        Times pcr;
        AbstractStream::Status i_ret = st->dequeue(deadline, &pcr);
        if( i_ret > i_return )
            i_return = i_ret;

        if( pcr.continuous > floor.continuous )
        {
            if( barrier->continuous > pcr.continuous )
                *barrier = pcr;
        }
    }

    return i_return;
}

StreamPosition PlaylistManager::getResumePosition() const
{
    vlc_mutex_locker locker(&demux.lock);
    StreamPosition pos;
    pos.times = demux.times;
    return pos;
}

Times PlaylistManager::getFirstTimes() const
{
    Times mindts;
    for(const AbstractStream *stream : streams)
    {
        const Times dts = stream->getFirstTimes();
        if(mindts.continuous == VLC_TS_INVALID)
            mindts = dts;
        else if(dts.continuous != VLC_TS_INVALID &&
                dts.continuous < mindts.continuous)
            mindts = dts;
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

bool PlaylistManager::setPosition(mtime_t mediatime, double pos, bool accurate)
{
    bool ret = true;
    bool hasValidStream = false;
    StreamPosition streampos;
    streampos.times = demux.firsttimes;
    if(streampos.times.continuous != VLC_TS_INVALID)
        streampos.times.offsetBy(mediatime - streampos.times.segment.media);
    else
        streampos.times.segment.media = mediatime;
    streampos.pos = pos;
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
                ret &= st->setPosition(streampos, !real);
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

    if(accurate && ret && streampos.times.continuous >= VLC_TS_0)
    {
        es_out_Control(p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                       streampos.times.continuous);
        SeekDebug(msg_Dbg(p_demux,"ES_OUT_SET_NEXT_DISPLAY_TIME to %" PRId64,
                          streampos.times.continuous));
    }

    return ret;
}

void PlaylistManager::setLivePause(bool b)
{
    if(!started())
        return;

    for(AbstractStream* st : streams)
        if(st->isValid() && !st->isDisabled())
            st->setLivePause(b);
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

void PlaylistManager::preparsePlaylist()
{

}

Times PlaylistManager::getTimes(bool b_first) const
{
    vlc_mutex_locker locker(&demux.lock);
    return b_first ? demux.firsttimes : demux.times;
}

mtime_t PlaylistManager::getMinAheadTime() const
{
    mtime_t minbuffer = 0;
    std::for_each(streams.cbegin(), streams.cend(),
        [&minbuffer](const AbstractStream *st) {
            if(st->isValid() && !st->isDisabled() && st->isSelected())
            {
                const mtime_t m = st->getMinAheadTime();
                if(m > 0 && (m < minbuffer || minbuffer == 0))
                    minbuffer = m;
            }
        });
    return minbuffer;
}

bool PlaylistManager::reactivateStream(AbstractStream *stream)
{
    return stream->reactivate(getResumePosition());
}

#define DEMUX_INCREMENT (CLOCK_FREQ / 20)
int PlaylistManager::demux_callback(demux_t *p_demux)
{
    PlaylistManager *manager = reinterpret_cast<PlaylistManager *>(p_demux->p_sys);
    if(!manager->started() && !manager->start())
        return VLC_DEMUXER_EOF;
    return manager->doDemux(DEMUX_INCREMENT);
}

int PlaylistManager::doDemux(int64_t increment)
{
    vlc_mutex_lock(&demux.lock);
    if(demux.times.continuous == VLC_TS_INVALID)
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
            vlc_cond_timedwait(&demux.cond, &demux.lock, mdate() + CLOCK_FREQ / 20);
        vlc_mutex_unlock(&demux.lock);
        return (b_dead || b_all_disabled) ? VLC_DEMUXER_EOF : VLC_DEMUXER_SUCCESS;
    }

    Times barrier = demux.times;
    barrier.offsetBy(increment);

    vlc_mutex_unlock(&demux.lock);

    AbstractStream::Status status = dequeue(demux.times, &barrier);

    vlc_mutex_lock(&demux.lock);
    if(demux.firsttimes.continuous == VLC_TS_INVALID && barrier.continuous != VLC_TS_INVALID)
    {
        demux.firsttimes = barrier;
        assert(barrier.segment.media != VLC_TS_INVALID);
    }
    vlc_mutex_unlock(&demux.lock);

    updateControlsPosition();

    switch(status)
    {
    case AbstractStream::Status::Eof:
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

                demux.times = Times();
                demux.firsttimes = Times();
                es_out_Control(p_demux->out, ES_OUT_RESET_PCR);

                setBufferingRunState(true);
            }
        }
        break;
    case AbstractStream::Status::Buffering:
        vlc_mutex_lock(&demux.lock);
        vlc_cond_timedwait(&demux.cond, &demux.lock, mdate() + CLOCK_FREQ / 20);
        vlc_mutex_unlock(&demux.lock);
        break;
    case AbstractStream::Status::Discontinuity:
        vlc_mutex_lock(&demux.lock);
        demux.times = Times();
        demux.firsttimes = Times();
        demux.pcr_syncpoint = TimestampSynchronizationPoint::Discontinuity;
        es_out_Control(p_demux->out, ES_OUT_RESET_PCR);
        vlc_mutex_unlock(&demux.lock);
        break;
    case AbstractStream::Status::Demuxed:
        vlc_mutex_lock(&demux.lock);
        if( demux.times.continuous != VLC_TS_INVALID && barrier.continuous != demux.times.continuous )
        {
            demux.times = barrier;
            mtime_t pcr = VLC_TS_0 + std::max(INT64_C(0), demux.times.continuous - CLOCK_FREQ/10);
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
            setBufferingRunState(false); /* /!\ always stop buffering process first */
            bool b_pause = (bool)va_arg(args, int);
            if(playlist->isLive())
            {
                mtime_t now = mdate();
                demux.times = Times();
                cached.lastupdate = 0;
                if(b_pause)
                {
                    setLivePause(true);
                    pause_start = now;
                    msg_Dbg(p_demux,"Buffering and playback paused. No timeshift support.");
                }
                else
                {
                    setLivePause(false);
                    msg_Dbg(p_demux,"Resuming buffering/playback after %" PRId64 "ms",
                            (now-pause_start) / 1000);
                    es_out_Control(p_demux->out, ES_OUT_RESET_PCR);
                    setBufferingRunState(true);
                }
            }
            else setBufferingRunState(true);
            return VLC_SUCCESS;
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
            if(cached.b_live && cached.playlistLength == 0)
                return VLC_EGENERIC;
            *(va_arg (args, mtime_t *)) = cached.playlistLength;
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
            setBufferingRunState(false); /* /!\ always stop buffering process first */
            vlc_mutex_locker locker(&cached.lock);

            if(cached.playlistLength == 0)
            {
                setBufferingRunState(true);
                return VLC_EGENERIC;
            }

            double pos = va_arg(args, double);
            bool accurate = va_arg(args, int);
            mtime_t seekTime = cached.playlistStart + cached.playlistLength * pos;

            SeekDebug(msg_Dbg(p_demux, "Seek %f to %ld plstart %ld duration %ld",
                   pos, seekTime, cached.playlistEnd, cached.playlistLength));

            if(!setPosition(seekTime, pos, accurate))
            {
                setBufferingRunState(true);
                return VLC_EGENERIC;
            }

            demux.pcr_syncpoint = TimestampSynchronizationPoint::RandomAccess;
            demux.times = Times();
            demux.firsttimes = Times();
            cached.lastupdate = 0;
            cached.i_time = VLC_TS_INVALID;
            setBufferingRunState(true);
            break;
        }

        case DEMUX_SET_TIME:
        {
            setBufferingRunState(false); /* stop downloader first */

            mtime_t time = va_arg(args, mtime_t);
            bool accurate = va_arg(args, int);
            if(!setPosition(time, -1, accurate))
            {
                setBufferingRunState(true);
                return VLC_EGENERIC;
            }

            vlc_mutex_locker locker(&cached.lock);
            demux.pcr_syncpoint = TimestampSynchronizationPoint::RandomAccess;
            demux.times = Times();
            demux.firsttimes = Times();
            cached.lastupdate = 0;
            cached.i_time = VLC_TS_INVALID;
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
    const mtime_t i_min_buffering = bufferingLogic->getMinBuffering(playlist);
    const mtime_t i_max_buffering = bufferingLogic->getMaxBuffering(playlist);
    const mtime_t i_target_buffering = bufferingLogic->getStableBuffering(playlist);
    while(1)
    {
        while(!b_buffering && !b_canceled)
            vlc_cond_wait(&waitcond, &lock);
        if (b_canceled)
            break;

        if(needsUpdate())
        {
            if(updatePlaylist())
                scheduleNextUpdate();
            else
                failedupdates++;
        }

        vlc_mutex_lock(&demux.lock);
        Times pcr = demux.times;
        vlc_mutex_unlock(&demux.lock);

        AbstractStream::BufferingStatus i_return = bufferize(pcr, i_min_buffering,
                                                             i_max_buffering, i_target_buffering);

        if(i_return != AbstractStream::BufferingStatus::Lessthanmin)
        {
            mtime_t i_deadline = mdate();
            if(i_return == AbstractStream::BufferingStatus::Ongoing)
                i_deadline += (CLOCK_FREQ / 100);
            else if(i_return == AbstractStream::BufferingStatus::Full)
                i_deadline += (CLOCK_FREQ / 10);
            else if(i_return == AbstractStream::BufferingStatus::End)
                i_deadline += (CLOCK_FREQ);
            else /*if(i_return == AbstractStream::BufferingStatus::suspended)*/
                i_deadline += (CLOCK_FREQ / 4);

            // TODO: The current function doesn't seem to modify shared
            //       state under demux lock.
            vlc_cond_signal(&demux.cond);

            while(b_buffering &&
                    vlc_cond_timedwait(&waitcond, &lock, i_deadline) == 0 &&
                    i_deadline > mdate() &&
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
    return nullptr;
}

void PlaylistManager::updateControlsPosition()
{
    vlc_mutex_locker locker(&cached.lock);

    time_t now = time(nullptr);
    if(now - cached.lastupdate < 1)
        return;
    cached.lastupdate = now;

    for(AbstractStream* st : streams)
    {
        if(st->isValid() && !st->isDisabled() && st->isSelected())
        {
            if(st->getMediaPlaybackTimes(&cached.playlistStart, &cached.playlistEnd,
                                         &cached.playlistLength))
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

    SeekDebug(Times startTimes = getTimes(true));
    Times currentTimes = getTimes();
    cached.b_live = playlist->isLive();

    SeekDebug(msg_Dbg(p_demux, "playlist Start/End %ld/%ld len %ld"
                               "rap pl/demux (%ld/%ld)",
                      cached.playlistStart, cached.playlistEnd, cached.playlistEnd,
                      startTimes.segment.media, startTimes.segment.demux));

    if(cached.b_live)
    {
        /* Special case for live until we can provide relative start to fully match
           the above description */
        cached.i_time = currentTimes.segment.media;

        if(cached.playlistStart != cached.playlistEnd)
        {
            if(cached.playlistStart < 0) /* Live template. Range start = now() - buffering depth */
            {
                cached.playlistEnd = CLOCK_FREQ * now;
                cached.playlistStart = cached.playlistEnd - cached.playlistLength;
            }
        }

        if(cached.i_time > VLC_TS_0 + cached.playlistStart &&
           cached.i_time <= VLC_TS_0 + cached.playlistEnd && cached.playlistLength)
        {
            cached.f_position = ((double)(cached.i_time - VLC_TS_0 - cached.playlistStart)) / cached.playlistLength;
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

        if(cached.playlistLength && currentTimes.segment.media != VLC_TS_INVALID)
        {
            cached.i_time = currentTimes.segment.media;
            cached.f_position = (double) (cached.i_time - VLC_TS_0 - cached.playlistStart) / cached.playlistLength;
        }
        else
        {
            cached.f_position = 0.0;
        }
    }

    SeekDebug(msg_Dbg(p_demux, "cached.i_time (%ld) cur %ld rap start (pl %ld/dmx %ld) pos %f",
                      cached.i_time, currentTimes.continuous, startTimes.segment.media,
                            startTimes.segment.demux, cached.f_position));
}

AbstractAdaptationLogic *PlaylistManager::createLogic(AbstractAdaptationLogic::LogicType type, AbstractConnectionManager *conn)
{
    vlc_object_t *obj = VLC_OBJECT(p_demux);
    AbstractAdaptationLogic *logic = nullptr;
    switch(type)
    {
        case AbstractAdaptationLogic::LogicType::FixedRate:
        {
            size_t bps = var_InheritInteger(p_demux, "adaptive-bw") * 8192;
            logic = new (std::nothrow) FixedRateAdaptationLogic(obj, bps);
            break;
        }
        case AbstractAdaptationLogic::LogicType::AlwaysLowest:
            logic = new (std::nothrow) AlwaysLowestAdaptationLogic(obj);
            break;
        case AbstractAdaptationLogic::LogicType::AlwaysBest:
            logic = new (std::nothrow) AlwaysBestAdaptationLogic(obj);
            break;
        case AbstractAdaptationLogic::LogicType::RateBased:
        {
            RateBasedAdaptationLogic *ratelogic =
                    new (std::nothrow) RateBasedAdaptationLogic(obj);
            if(ratelogic)
                conn->setDownloadRateObserver(ratelogic);
            logic = ratelogic;
            break;
        }
        case AbstractAdaptationLogic::LogicType::Default:
        case AbstractAdaptationLogic::LogicType::NearOptimal:
        {
            NearOptimalAdaptationLogic *noplogic =
                    new (std::nothrow) NearOptimalAdaptationLogic(obj);
            if(noplogic)
                conn->setDownloadRateObserver(noplogic);
            logic = noplogic;
            break;
        }
        case AbstractAdaptationLogic::LogicType::Predictive:
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
        int w = var_InheritInteger(p_demux, "adaptive-maxwidth");
        int h = var_InheritInteger(p_demux, "adaptive-maxheight");
        if(h == 0)
        {
            h = var_InheritInteger(p_demux, "preferred-resolution");
            /* Adapt for slightly different minimum/maximum semantics */
            if(h == -1)
                h = 0;
            else if(h == 0)
                h = 1;
        }

        logic->setMaxDeviceResolution(w, h);
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
            bl->setUserLiveDelay(CLOCK_FREQ / 1000 * v);
        v = var_InheritInteger(p_demux, "adaptive-maxbuffer");
        if(v)
            bl->setUserMaxBuffering(CLOCK_FREQ / 1000 * v);
    }
    return bl;
}
