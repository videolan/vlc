/*
 * Streams.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC authors
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

#include "Streams.hpp"
#include "logic/AbstractAdaptationLogic.h"
#include "http/HTTPConnection.hpp"
#include "http/HTTPConnectionManager.h"
#include "playlist/BaseRepresentation.h"
#include "playlist/SegmentChunk.hpp"
#include "plumbing/SourceStream.hpp"
#include "plumbing/CommandsQueue.hpp"
#include "tools/Debug.hpp"
#include <vlc_demux.h>

#include <algorithm>

using namespace adaptive;
using namespace adaptive::http;

AbstractStream::AbstractStream(demux_t * demux_)
{
    p_realdemux = demux_;
    format = StreamFormat::UNSUPPORTED;
    currentChunk = NULL;
    eof = false;
    dead = false;
    disabled = false;
    discontinuity = false;
    needrestart = false;
    inrestart = false;
    segmentTracker = NULL;
    demuxersource = NULL;
    demuxer = NULL;
    fakeesout = NULL;
    notfound_sequence = 0;
    last_buffer_status = buffering_lessthanmin;
    vlc_mutex_init(&lock);
}

bool AbstractStream::init(const StreamFormat &format_, SegmentTracker *tracker, AbstractConnectionManager *conn)
{
    /* Don't even try if not supported or already init */
    if((unsigned)format_ == StreamFormat::UNSUPPORTED || demuxersource)
        return false;

    demuxersource = new (std::nothrow) BufferedChunksSourceStream( VLC_OBJECT(p_realdemux), this );
    if(demuxersource)
    {
        CommandsFactory *factory = new (std::nothrow) CommandsFactory();
        if(factory)
        {
            CommandsQueue *commandsqueue = new (std::nothrow) CommandsQueue(factory);
            if(commandsqueue)
            {
                fakeesout = new (std::nothrow) FakeESOut(p_realdemux->out, commandsqueue);
                if(fakeesout)
                {
                    /* All successfull */
                    fakeesout->setExtraInfoProvider( this );
                    format = format_;
                    segmentTracker = tracker;
                    segmentTracker->registerListener(this);
                    segmentTracker->notifyBufferingState(true);
                    connManager = conn;
                    setTimeOffset(segmentTracker->getPlaybackTime());
                    return true;
                }
                delete commandsqueue;
                commandsqueue = NULL;
            }
            else
            {
                delete factory;
            }
        }
        delete demuxersource;
    }

    return false;
}

AbstractStream::~AbstractStream()
{
    delete currentChunk;
    if(segmentTracker)
        segmentTracker->notifyBufferingState(false);
    delete segmentTracker;

    delete demuxer;
    delete demuxersource;
    delete fakeesout;

    vlc_mutex_destroy(&lock);
}

void AbstractStream::prepareRestart(bool b_discontinuity)
{
    if(demuxer)
    {
        /* Enqueue Del Commands for all current ES */
        demuxer->drain();
        setTimeOffset(true);
        /* Enqueue Del Commands for all current ES */
        fakeEsOut()->scheduleAllForDeletion();
        if(b_discontinuity)
            fakeEsOut()->schedulePCRReset();
        fakeEsOut()->commandsQueue()->Commit();
        /* ignoring demuxer's own Del commands */
        fakeEsOut()->commandsQueue()->setDrop(true);
        delete demuxer;
        fakeEsOut()->commandsQueue()->setDrop(false);
        demuxer = NULL;
    }
}

void AbstractStream::setLanguage(const std::string &lang)
{
    language = lang;
}

void AbstractStream::setDescription(const std::string &desc)
{
    description = desc;
}

vlc_tick_t AbstractStream::getPCR() const
{
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    vlc_tick_t pcr = isDisabled() ? VLC_TICK_INVALID : fakeEsOut()->commandsQueue()->getPCR();
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return pcr;
}

vlc_tick_t AbstractStream::getMinAheadTime() const
{
    if(!segmentTracker)
        return 0;
    return segmentTracker->getMinAheadTime();
}

vlc_tick_t AbstractStream::getFirstDTS() const
{
    vlc_tick_t dts;
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    if(isDisabled())
    {
        dts = VLC_TICK_INVALID;
    }
    else
    {
        dts = fakeEsOut()->commandsQueue()->getFirstDTS();
        if(dts == VLC_TICK_INVALID)
            dts = fakeEsOut()->commandsQueue()->getPCR();
    }
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return dts;
}

int AbstractStream::esCount() const
{
    return fakeEsOut()->esCount();
}

bool AbstractStream::seekAble() const
{
    bool restarting = fakeEsOut()->restarting();
    bool draining = fakeEsOut()->commandsQueue()->isDraining();
    bool eof = fakeEsOut()->commandsQueue()->isEOF();

    msg_Dbg(p_realdemux, "demuxer %p, fakeesout restarting %d, "
            "discontinuity %d, commandsqueue draining %d, commandsqueue eof %d",
            static_cast<void *>(demuxer), restarting, discontinuity, draining, eof);

    if(!demuxer || restarting || discontinuity || (!eof && draining))
    {
        msg_Warn(p_realdemux, "not seekable");
        return false;
    }
    else
    {
        return true;
    }
}

bool AbstractStream::isSelected() const
{
    return fakeEsOut()->hasSelectedEs();
}

bool AbstractStream::reactivate(vlc_tick_t basetime)
{
    if(setPosition(basetime, false))
    {
        setDisabled(false);
        return true;
    }
    else
    {
        eof = true; /* can't reactivate */
        return false;
    }
}

bool AbstractStream::startDemux()
{
    if(demuxer)
        return false;

    demuxersource->Reset();
    demuxer = createDemux(format);
    if(!demuxer && format != StreamFormat())
        msg_Err(p_realdemux, "Failed to create demuxer %p %s", (void *)demuxer,
                format.str().c_str());

    return !!demuxer;
}

bool AbstractStream::restartDemux()
{
    bool b_ret = true;
    if(!demuxer)
    {
        b_ret = startDemux();
    }
    else if(demuxer->needsRestartOnSeek())
    {
        inrestart = true;
        /* Push all ES as recycling candidates */
        fakeEsOut()->recycleAll();
        /* Restart with ignoring es_Del pushes to queue when terminating demux */
        fakeEsOut()->commandsQueue()->setDrop(true);
        demuxer->destroy();
        fakeEsOut()->commandsQueue()->setDrop(false);
        b_ret = demuxer->create();
        inrestart = false;
    }
    else
    {
        fakeEsOut()->commandsQueue()->Commit();
    }
    return b_ret;
}

void AbstractStream::setDisabled(bool b)
{
    if(disabled != b)
        segmentTracker->notifyBufferingState(!b);
    disabled = b;
}

bool AbstractStream::isDisabled() const
{
    return dead || disabled;
}

bool AbstractStream::canActivate() const
{
    return !dead;
}

bool AbstractStream::decodersDrained()
{
    return fakeEsOut()->decodersDrained();
}

AbstractStream::buffering_status AbstractStream::getLastBufferStatus() const
{
    return last_buffer_status;
}

vlc_tick_t AbstractStream::getDemuxedAmount() const
{
    return fakeEsOut()->commandsQueue()->getDemuxedAmount();
}

AbstractStream::buffering_status AbstractStream::bufferize(vlc_tick_t nz_deadline,
                                                           vlc_tick_t i_min_buffering, vlc_tick_t i_extra_buffering)
{
    last_buffer_status = doBufferize(nz_deadline, i_min_buffering, i_extra_buffering);
    return last_buffer_status;
}

AbstractStream::buffering_status AbstractStream::doBufferize(vlc_tick_t nz_deadline,
                                                             vlc_tick_t i_min_buffering, vlc_tick_t i_extra_buffering)
{
    vlc_mutex_lock(&lock);

    /* Ensure it is configured */
    if(!segmentTracker || !connManager || dead)
    {
        vlc_mutex_unlock(&lock);
        return AbstractStream::buffering_end;
    }

    /* Disable streams that are not selected (alternate streams) */
    if(esCount() && !isSelected() && !fakeEsOut()->restarting())
    {
        setDisabled(true);
        segmentTracker->reset();
        fakeEsOut()->commandsQueue()->Abort(false);
        msg_Dbg(p_realdemux, "deactivating %s stream %s",
                format.str().c_str(), description.c_str());
        vlc_mutex_unlock(&lock);
        return AbstractStream::buffering_end;
    }

    if(fakeEsOut()->commandsQueue()->isDraining())
    {
        vlc_mutex_unlock(&lock);
        return AbstractStream::buffering_suspended;
    }

    if(!demuxer)
    {
        format = segmentTracker->getCurrentFormat();
        if(!startDemux())
        {
            /* If demux fails because of probing failure / wrong format*/
            if(discontinuity)
            {
                msg_Dbg( p_realdemux, "Draining on format change" );
                prepareRestart();
                discontinuity = false;
                fakeEsOut()->commandsQueue()->setDraining();
                vlc_mutex_unlock(&lock);
                return AbstractStream::buffering_ongoing;
            }
            dead = true; /* Prevent further retries */
            fakeEsOut()->commandsQueue()->setEOF(true);
            vlc_mutex_unlock(&lock);
            return AbstractStream::buffering_end;
        }
    }

    const vlc_tick_t i_total_buffering = i_min_buffering + i_extra_buffering;

    vlc_tick_t i_demuxed = fakeEsOut()->commandsQueue()->getDemuxedAmount();
    segmentTracker->notifyBufferingLevel(i_min_buffering, i_demuxed, i_total_buffering);
    if(i_demuxed < i_total_buffering) /* not already demuxed */
    {
        if(!segmentTracker->segmentsListReady()) /* Live Streams */
        {
            vlc_mutex_unlock(&lock);
            return AbstractStream::buffering_suspended;
        }

        vlc_tick_t nz_extdeadline = fakeEsOut()->commandsQueue()->getBufferingLevel() +
                                    (i_total_buffering - i_demuxed) / 4;
        nz_deadline = std::max(nz_deadline, nz_extdeadline);

        /* need to read, demuxer still buffering, ... */
        vlc_mutex_unlock(&lock);
        int i_ret = demuxer->demux(nz_deadline);
        vlc_mutex_lock(&lock);
        if(i_ret != VLC_DEMUXER_SUCCESS)
        {
            if(discontinuity || needrestart)
            {
                msg_Dbg(p_realdemux, "Restarting demuxer");
                prepareRestart(discontinuity);
                if(discontinuity)
                {
                    msg_Dbg(p_realdemux, "Draining on discontinuity");
                    fakeEsOut()->commandsQueue()->setDraining();
                    discontinuity = false;
                }
                needrestart = false;
                vlc_mutex_unlock(&lock);
                return AbstractStream::buffering_ongoing;
            }
            fakeEsOut()->commandsQueue()->setEOF(true);
            vlc_mutex_unlock(&lock);
            return AbstractStream::buffering_end;
        }
        i_demuxed = fakeEsOut()->commandsQueue()->getDemuxedAmount();
        segmentTracker->notifyBufferingLevel(i_min_buffering, i_demuxed, i_total_buffering);
    }
    vlc_mutex_unlock(&lock);

    if(i_demuxed < i_total_buffering) /* need to read more */
    {
        if(i_demuxed < i_min_buffering)
            return AbstractStream::buffering_lessthanmin; /* high prio */
        return AbstractStream::buffering_ongoing;
    }
    return AbstractStream::buffering_full;
}

AbstractStream::status AbstractStream::dequeue(vlc_tick_t nz_deadline, vlc_tick_t *pi_pcr)
{
    vlc_mutex_locker locker(&lock);

    *pi_pcr = nz_deadline;

    if(fakeEsOut()->commandsQueue()->isDraining())
    {
        AdvDebug(msg_Dbg(p_realdemux, "Stream %s pcr %" PRId64 " dts %" PRId64 " deadline %" PRId64 " [DRAINING]",
                         description.c_str(), commandsqueue->getPCR(), commandsqueue->getFirstDTS(),
                         nz_deadline));

        *pi_pcr = fakeEsOut()->commandsQueue()->Process(p_realdemux->out, VLC_TICK_0 + nz_deadline);
        if(!fakeEsOut()->commandsQueue()->isEmpty())
            return AbstractStream::status_demuxed;

        if(!fakeEsOut()->commandsQueue()->isEOF())
        {
            fakeEsOut()->commandsQueue()->Abort(true); /* reset buffering level and flags */
            return AbstractStream::status_discontinuity;
        }
    }

    if(isDisabled() || fakeEsOut()->commandsQueue()->isEOF())
    {
        *pi_pcr = nz_deadline;
        return AbstractStream::status_eof;
    }

    AdvDebug(msg_Dbg(p_realdemux, "Stream %s pcr %" PRId64 " dts %" PRId64 " deadline %" PRId64 " buflevel %" PRId64,
                     description.c_str(), fakeEsOut()->commandsQueue()->getPCR(), fakeEsOut()->commandsQueue()->getFirstDTS(),
                     nz_deadline, fakeEsOut()->commandsQueue()->getBufferingLevel()));

    if(nz_deadline + VLC_TICK_0 <= fakeEsOut()->commandsQueue()->getBufferingLevel()) /* demuxed */
    {
        *pi_pcr = fakeEsOut()->commandsQueue()->Process( p_realdemux->out, VLC_TICK_0 + nz_deadline );
        return AbstractStream::status_demuxed;
    }

    return AbstractStream::status_buffering;
}

std::string AbstractStream::getContentType()
{
    if (currentChunk == NULL && !eof)
        currentChunk = segmentTracker->getNextChunk(!fakeEsOut()->restarting(), connManager);
    if(currentChunk)
        return currentChunk->getContentType();
    else
        return std::string();
}

block_t * AbstractStream::readNextBlock()
{
    if (currentChunk == NULL && !eof)
        currentChunk = segmentTracker->getNextChunk(!fakeEsOut()->restarting(), connManager);

    if(discontinuity || needrestart)
    {
        msg_Info(p_realdemux, "Encountered discontinuity");
        /* Force stream/demuxer to end for this call */
        return NULL;
    }

    if(currentChunk == NULL)
    {
        eof = true;
        return NULL;
    }

    const bool b_segment_head_chunk = (currentChunk->getBytesRead() == 0);

    block_t *block = currentChunk->readBlock();
    if(block == NULL)
    {
        if(currentChunk->getRequestStatus() == RequestStatus::NotFound &&
           ++notfound_sequence < 3)
        {
            discontinuity = true;
        }
        delete currentChunk;
        currentChunk = NULL;
        return NULL;
    }
    else notfound_sequence = 0;

    if (currentChunk->isEmpty())
    {
        delete currentChunk;
        currentChunk = NULL;
    }

    block = checkBlock(block, b_segment_head_chunk);

    return block;
}

bool AbstractStream::setPosition(vlc_tick_t time, bool tryonly)
{
    if(!seekAble())
        return false;

    bool ret = segmentTracker->setPositionByTime(time, demuxer->needsRestartOnSeek(), tryonly);
    if(!tryonly && ret)
    {
        // clear eof flag before restartDemux() to prevent readNextBlock() fail
        eof = false;
        notfound_sequence = 0;
        if(demuxer->needsRestartOnSeek())
        {
            if(currentChunk)
                delete currentChunk;
            currentChunk = NULL;
            needrestart = false;

            setTimeOffset(-1);
            setTimeOffset(segmentTracker->getPlaybackTime());

            if( !restartDemux() )
            {
                msg_Info(p_realdemux, "Restart demux failed");
                eof = true;
                dead = true;
                ret = false;
            }
            else
            {
                fakeEsOut()->commandsQueue()->setEOF(false);
            }
        }
        else fakeEsOut()->commandsQueue()->Abort( true );

        es_out_Control(p_realdemux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                       VLC_TICK_0 + time);
    }
    return ret;
}

vlc_tick_t AbstractStream::getPlaybackTime() const
{
    return segmentTracker->getPlaybackTime();
}

void AbstractStream::runUpdates()
{
    if(!isDisabled())
        segmentTracker->updateSelected();
}

void AbstractStream::fillExtraFMTInfo( es_format_t *p_fmt ) const
{
    if(!p_fmt->psz_language && !language.empty())
        p_fmt->psz_language = strdup(language.c_str());
    if(!p_fmt->psz_description && !description.empty())
        p_fmt->psz_description = strdup(description.c_str());
}

void AbstractStream::setTimeOffset(vlc_tick_t i_offset)
{
    /* Check if we need to set an offset as the demuxer
     * will start from zero from seek point */
    if(i_offset < 0) /* reset */
    {
        fakeEsOut()->setExpectedTimestampOffset(0);
    }
    else if(demuxer)
    {
        fakeEsOut()->setExpectedTimestampOffset(i_offset);
    }
}

AbstractDemuxer * AbstractStream::createDemux(const StreamFormat &format)
{
    AbstractDemuxer *ret = newDemux( p_realdemux, format,
                                     fakeEsOut()->getEsOut(), demuxersource );
    if(ret && !ret->create())
    {
        delete ret;
        ret = NULL;
    }
    else fakeEsOut()->commandsQueue()->Commit();

    return ret;
}

AbstractDemuxer *AbstractStream::newDemux(demux_t *p_realdemux, const StreamFormat &format,
                                          es_out_t *out, AbstractSourceStream *source) const
{
    AbstractDemuxer *ret = NULL;
    switch((unsigned)format)
    {
        case StreamFormat::MP4:
            ret = new Demuxer(p_realdemux, "mp4", out, source);
            break;

        case StreamFormat::MPEG2TS:
            ret = new Demuxer(p_realdemux, "ts", out, source);
            break;

        default:
        case StreamFormat::UNSUPPORTED:
            break;
    }
    return ret;
}

void AbstractStream::trackerEvent(const SegmentTrackerEvent &event)
{
    switch(event.type)
    {
        case SegmentTrackerEvent::DISCONTINUITY:
            discontinuity = true;
            break;

        case SegmentTrackerEvent::FORMATCHANGE:
            /* Check if our current demux is still valid */
            if(*event.u.format.f != format)
            {
                /* Format has changed between segments, we need to drain and change demux */
                msg_Info(p_realdemux, "Changing stream format %s -> %s",
                         format.str().c_str(), event.u.format.f->str().c_str());
                format = *event.u.format.f;

                /* This is an implict discontinuity */
                discontinuity = true;
            }
            break;

        case SegmentTrackerEvent::SWITCHING:
            if(demuxer && demuxer->needsRestartOnSwitch() && !inrestart)
            {
                needrestart = true;
            }
            break;

        case SegmentTrackerEvent::SEGMENT_CHANGE:
            if(demuxer && demuxer->needsRestartOnEachSegment() && !inrestart)
            {
                needrestart = true;
            }
            break;

        default:
            break;
    }
}

FakeESOut::LockedFakeEsOut AbstractStream::fakeEsOut()
{
    return fakeesout->WithLock();
}

FakeESOut::LockedFakeEsOut AbstractStream::fakeEsOut() const
{
    return fakeesout->WithLock();
}
