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
#include "playlist/BaseAdaptationSet.h"
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
    format = StreamFormat::UNKNOWN;
    currentChunk = nullptr;
    eof = false;
    valid = true;
    disabled = false;
    discontinuity = false;
    needrestart = false;
    inrestart = false;
    demuxfirstchunk = false;
    segmentTracker = nullptr;
    demuxersource = nullptr;
    demuxer = nullptr;
    fakeesout = nullptr;
    notfound_sequence = 0;
    last_buffer_status = BufferingStatus::Lessthanmin;
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
                    const Role & streamRole = tracker->getStreamRole();
                    if(streamRole.isDefault() && streamRole.autoSelectable())
                        fakeesout->setPriority(ES_PRIORITY_MIN + 10);
                    else if(!streamRole.autoSelectable())
                        fakeesout->setPriority(ES_PRIORITY_NOT_DEFAULTABLE);
                    format = format_;
                    segmentTracker = tracker;
                    segmentTracker->registerListener(this);
                    segmentTracker->notifyBufferingState(true);
                    connManager = conn;
                    fakeesout->setExpectedTimestamp(segmentTracker->getPlaybackTime());
                    declaredCodecs();
                    return true;
                }
                delete commandsqueue;
                commandsqueue = nullptr;
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
}

void AbstractStream::prepareRestart(bool b_discontinuity)
{
    if(demuxer)
    {
        /* Enqueue Del Commands for all current ES */
        demuxer->drain();
        fakeEsOut()->resetTimestamps();
        /* Enqueue Del Commands for all current ES */
        fakeEsOut()->scheduleAllForDeletion();
        if(b_discontinuity)
            fakeEsOut()->schedulePCRReset();
        fakeEsOut()->commandsQueue()->Commit();
        /* ignoring demuxer's own Del commands */
        fakeEsOut()->commandsQueue()->setDrop(true);
        delete demuxer;
        fakeEsOut()->commandsQueue()->setDrop(false);
        demuxer = nullptr;
    }
}

bool AbstractStream::resetForNewPosition(vlc_tick_t seekMediaTime)
{
    // clear eof flag before restartDemux() to prevent readNextBlock() fail
    eof = false;
    demuxfirstchunk = true;
    notfound_sequence = 0;
    if(!demuxer || demuxer->needsRestartOnSeek()) /* needs (re)start */
    {
        delete currentChunk;
        currentChunk = nullptr;
        needrestart = false;

        fakeEsOut()->resetTimestamps();

        fakeEsOut()->setExpectedTimestamp(seekMediaTime);
        if( !restartDemux() )
        {
            msg_Info(p_realdemux, "Restart demux failed");
            eof = true;
            valid = false;
            return false;
        }
        else
        {
            fakeEsOut()->commandsQueue()->setEOF(false);
        }
    }
    else fakeEsOut()->commandsQueue()->Abort( true );
    return true;
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
    vlc_mutex_locker locker(&lock);
    if(!valid || disabled)
        return VLC_TICK_INVALID;
    return fakeEsOut()->commandsQueue()->getPCR();
}

vlc_tick_t AbstractStream::getMinAheadTime() const
{
    if(!segmentTracker)
        return 0;
    return segmentTracker->getMinAheadTime();
}

vlc_tick_t AbstractStream::getFirstDTS() const
{
    vlc_mutex_locker locker(&lock);

    if(!valid || disabled)
        return VLC_TICK_INVALID;

    vlc_tick_t dts = fakeEsOut()->commandsQueue()->getFirstDTS();
    if(dts == VLC_TICK_INVALID)
        dts = fakeEsOut()->commandsQueue()->getPCR();
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

    if(!valid || restarting || discontinuity || (!eof && draining))
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
    vlc_mutex_locker locker(&lock);
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
    demuxfirstchunk = true;

    return !!demuxer;
}

bool AbstractStream::restartDemux()
{
    bool b_ret = true;
    if(!demuxer)
    {
        fakeesout->recycleAll();
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

bool AbstractStream::isValid() const
{
    vlc_mutex_locker locker(&lock);
    return valid;
}

bool AbstractStream::isDisabled() const
{
    vlc_mutex_locker locker(&lock);
    return disabled;
}

void AbstractStream::setLivePause(bool b)
{
    vlc_mutex_locker locker(&lock);
    if(!b)
    {
        segmentTracker->setPosition(segmentTracker->getStartPosition(),
                                    !demuxer || demuxer->needsRestartOnSeek());
    }
}

bool AbstractStream::decodersDrained()
{
    return fakeEsOut()->decodersDrained();
}

AbstractStream::BufferingStatus AbstractStream::getLastBufferStatus() const
{
    return last_buffer_status;
}

vlc_tick_t AbstractStream::getDemuxedAmount(vlc_tick_t from) const
{
    return fakeEsOut()->commandsQueue()->getDemuxedAmount(from);
}

AbstractStream::BufferingStatus AbstractStream::bufferize(vlc_tick_t nz_deadline,
                                                           vlc_tick_t i_min_buffering,
                                                           vlc_tick_t i_extra_buffering,
                                                           bool b_keep_alive)
{
    last_buffer_status = doBufferize(nz_deadline, i_min_buffering, i_extra_buffering, b_keep_alive);
    return last_buffer_status;
}

AbstractStream::BufferingStatus AbstractStream::doBufferize(vlc_tick_t nz_deadline,
                                                             vlc_tick_t i_min_buffering,
                                                             vlc_tick_t i_extra_buffering,
                                                             bool b_keep_alive)
{
    vlc_mutex_lock(&lock);

    /* Ensure it is configured */
    if(!segmentTracker || !connManager || !valid)
    {
        vlc_mutex_unlock(&lock);
        return BufferingStatus::End;
    }

    /* Disable streams that are not selected (alternate streams) */
    if(esCount() && !isSelected() && !fakeEsOut()->restarting() && !b_keep_alive)
    {
        setDisabled(true);
        segmentTracker->reset();
        fakeEsOut()->commandsQueue()->Abort(false);
        msg_Dbg(p_realdemux, "deactivating %s stream %s",
                format.str().c_str(), description.c_str());
        vlc_mutex_unlock(&lock);
        return BufferingStatus::End;
    }

    if(fakeEsOut()->commandsQueue()->isDraining())
    {
        vlc_mutex_unlock(&lock);
        return BufferingStatus::Suspended;
    }

    segmentTracker->setStartPosition();

    /* Reached end of live playlist */
    if(!segmentTracker->bufferingAvailable())
    {
        vlc_mutex_unlock(&lock);
        return BufferingStatus::Suspended;
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
                return BufferingStatus::Ongoing;
            }
            valid = false; /* Prevent further retries */
            fakeEsOut()->commandsQueue()->setEOF(true);
            vlc_mutex_unlock(&lock);
            return BufferingStatus::End;
        }
    }

    const vlc_tick_t i_total_buffering = i_min_buffering + i_extra_buffering;

    vlc_tick_t i_demuxed = fakeEsOut()->commandsQueue()->getDemuxedAmount(nz_deadline);
    segmentTracker->notifyBufferingLevel(i_min_buffering, i_demuxed, i_total_buffering);
    if(i_demuxed < i_total_buffering) /* not already demuxed */
    {
        vlc_tick_t nz_extdeadline = fakeEsOut()->commandsQueue()->getBufferingLevel() +
                                    (i_total_buffering - i_demuxed) / 4;
        nz_deadline = std::max(nz_deadline, nz_extdeadline);

        /* need to read, demuxer still buffering, ... */
        vlc_mutex_unlock(&lock);
        Demuxer::Status demuxStatus = demuxer->demux(nz_deadline);
        vlc_mutex_lock(&lock);
        if(demuxStatus != Demuxer::Status::Success)
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
                return BufferingStatus::Ongoing;
            }
            fakeEsOut()->commandsQueue()->setEOF(true);
            vlc_mutex_unlock(&lock);
            return BufferingStatus::End;
        }
        i_demuxed = fakeEsOut()->commandsQueue()->getDemuxedAmount(nz_deadline);
        segmentTracker->notifyBufferingLevel(i_min_buffering, i_demuxed, i_total_buffering);
    }
    vlc_mutex_unlock(&lock);

    if(i_demuxed < i_total_buffering) /* need to read more */
    {
        if(i_demuxed < i_min_buffering)
            return BufferingStatus::Lessthanmin; /* high prio */
        return BufferingStatus::Ongoing;
    }
    return BufferingStatus::Full;
}

AbstractStream::Status AbstractStream::dequeue(vlc_tick_t nz_deadline, vlc_tick_t *pi_pcr)
{
    vlc_mutex_locker locker(&lock);

    *pi_pcr = nz_deadline;

    if(fakeEsOut()->commandsQueue()->isDraining())
    {
        AdvDebug(vlc_tick_t pcrvalue = fakeEsOut()->commandsQueue()->getPCR();
                 vlc_tick_t dtsvalue = fakeEsOut()->commandsQueue()->getFirstDTS();
                 msg_Dbg(p_realdemux, "Stream %s pcr %" PRId64 " dts %" PRId64 " deadline %" PRId64 " [DRAINING]",
                         description.c_str(), pcrvalue, dtsvalue, nz_deadline));

        *pi_pcr = fakeEsOut()->commandsQueue()->Process(VLC_TICK_0 + nz_deadline);
        if(!fakeEsOut()->commandsQueue()->isEmpty())
            return Status::Demuxed;

        if(!fakeEsOut()->commandsQueue()->isEOF())
        {
            fakeEsOut()->commandsQueue()->Abort(true); /* reset buffering level and flags */
            return Status::Discontinuity;
        }
    }

    if(!valid || disabled || fakeEsOut()->commandsQueue()->isEOF())
    {
        *pi_pcr = nz_deadline;
        return Status::Eof;
    }

    vlc_tick_t bufferingLevel = fakeEsOut()->commandsQueue()->getBufferingLevel();

    AdvDebug(vlc_tick_t pcrvalue = fakeEsOut()->commandsQueue()->getPCR();
             vlc_tick_t dtsvalue = fakeEsOut()->commandsQueue()->getFirstDTS();
             msg_Dbg(p_realdemux, "Stream %s pcr %" PRId64 " dts %" PRId64 " deadline %" PRId64 " buflevel %" PRId64,
                     description.c_str(), pcrvalue, dtsvalue, nz_deadline, bufferingLevel));

    if(nz_deadline + VLC_TICK_0 <= bufferingLevel) /* demuxed */
    {
        *pi_pcr = fakeEsOut()->commandsQueue()->Process( VLC_TICK_0 + nz_deadline );
        return Status::Demuxed;
    }

    return Status::Buffering;
}

std::string AbstractStream::getContentType()
{
    if (currentChunk == nullptr && !eof)
    {
        const bool b_restarting = fakeEsOut()->restarting();
        currentChunk = segmentTracker->getNextChunk(!b_restarting, connManager);
    }
    if(currentChunk)
        return currentChunk->getContentType();
    else
        return std::string();
}

block_t * AbstractStream::readNextBlock()
{
    if (currentChunk == nullptr && !eof)
    {
        const bool b_restarting = fakeEsOut()->restarting();
        currentChunk = segmentTracker->getNextChunk(!b_restarting, connManager);
    }

    if(discontinuity && demuxfirstchunk)
    {
        /* clear up discontinuity on demux start (discontinuity on start segment bug) */
        discontinuity = false;
    }

    if(discontinuity || needrestart)
    {
        msg_Info(p_realdemux, "Encountered discontinuity");
        /* Force stream/demuxer to end for this call */
        return nullptr;
    }

    if(currentChunk == nullptr)
    {
        eof = true;
        return nullptr;
    }

    const bool b_segment_head_chunk = (currentChunk->getBytesRead() == 0);

    block_t *block = currentChunk->readBlock();
    if(block == nullptr)
    {
        if(currentChunk->getRequestStatus() == RequestStatus::NotFound &&
           ++notfound_sequence < 3)
        {
            discontinuity = true;
        }
        delete currentChunk;
        currentChunk = nullptr;
        return nullptr;
    }
    else notfound_sequence = 0;

    demuxfirstchunk = false;

    if (currentChunk->isEmpty())
    {
        delete currentChunk;
        currentChunk = nullptr;
    }

    block = checkBlock(block, b_segment_head_chunk);

    return block;
}

bool AbstractStream::setPosition(vlc_tick_t time, bool tryonly)
{
    if(!seekAble())
        return false;

    bool b_needs_restart = demuxer ? demuxer->needsRestartOnSeek() : true;
    bool ret = segmentTracker->setPositionByTime(time, b_needs_restart, tryonly);
    if(!tryonly && ret)
    {
// in some cases, media time seek != sent dts
//        es_out_Control(p_realdemux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
//                       VLC_TICK_0 + time);
    }
    return ret;
}

bool AbstractStream::getMediaPlaybackTimes(vlc_tick_t *start, vlc_tick_t *end,
                                           vlc_tick_t *length,
                                           vlc_tick_t *mediaStart,
                                           vlc_tick_t *demuxStart) const
{
    return (segmentTracker->getMediaPlaybackRange(start, end, length) &&
            fakeEsOut()->getStartTimestamps(mediaStart, demuxStart));
}

void AbstractStream::runUpdates()
{
    if(valid && !disabled)
        segmentTracker->updateSelected();
}

void AbstractStream::fillExtraFMTInfo( es_format_t *p_fmt ) const
{
    if(!p_fmt->psz_language && !language.empty())
        p_fmt->psz_language = strdup(language.c_str());
    if(!p_fmt->psz_description && !description.empty())
        p_fmt->psz_description = strdup(description.c_str());
}

AbstractDemuxer * AbstractStream::createDemux(const StreamFormat &format)
{
    AbstractDemuxer *ret = newDemux( VLC_OBJECT(p_realdemux), format,
                                     (es_out_t *)fakeEsOut(), demuxersource );
    if(ret && !ret->create())
    {
        delete ret;
        ret = nullptr;
    }
    else fakeEsOut()->commandsQueue()->Commit();

    return ret;
}

AbstractDemuxer *AbstractStream::newDemux(vlc_object_t *p_obj, const StreamFormat &format,
                                          es_out_t *out, AbstractSourceStream *source) const
{
    AbstractDemuxer *ret = nullptr;
    switch((unsigned)format)
    {
        case StreamFormat::MP4:
            ret = new Demuxer(p_obj, "mp4", out, source);
            break;

        case StreamFormat::MPEG2TS:
            ret = new Demuxer(p_obj, "ts", out, source);
            break;

        default:
        case StreamFormat::UNSUPPORTED:
            break;
    }
    return ret;
}

void AbstractStream::trackerEvent(const TrackerEvent &ev)
{
    switch(ev.getType())
    {
        case TrackerEvent::Type::Discontinuity:
            discontinuity = true;
            break;

        case TrackerEvent::Type::FormatChange:
        {
            const FormatChangedEvent &event =
                    static_cast<const FormatChangedEvent &>(ev);
            /* Check if our current demux is still valid */
            if(*event.format != format || format == StreamFormat(StreamFormat::UNKNOWN))
            {
                /* Format has changed between segments, we need to drain and change demux */
                msg_Info(p_realdemux, "Changing stream format %s -> %s",
                         format.str().c_str(), event.format->str().c_str());
                format = *event.format;

                /* This is an implict discontinuity */
                discontinuity = true;
            }
        }
            break;

        case TrackerEvent::Type::RepresentationSwitch:
        {
            const RepresentationSwitchEvent &event =
                    static_cast<const RepresentationSwitchEvent &>(ev);
            if(demuxer && !inrestart)
            {
                if(!demuxer->bitstreamSwitchCompatible() ||
                   (event.next &&
                   !event.next->getAdaptationSet()->isBitSwitchable()))
                    needrestart = true;
            }
            AdvDebug(msg_Dbg(p_realdemux, "Stream %s switching %s %s to %s %s",
                    description.c_str(),
                    event.prev ? event.prev->getID().str().c_str() : "",
                    event.prev ? event.prev->getStreamFormat().str().c_str() : "",
                    event.next ? event.next->getID().str().c_str() : "",
                    event.next ? event.next->getStreamFormat().str().c_str() : ""));
        }
            break;

        case TrackerEvent::Type::SegmentChange:
            if(demuxer && demuxer->needsRestartOnEachSegment() && !inrestart)
            {
                needrestart = true;
            }
            break;

        case TrackerEvent::Type::PositionChange:
            resetForNewPosition(segmentTracker->getPlaybackTime(true));
            break;

        default:
            break;
    }
}

void AbstractStream::declaredCodecs()
{
    CodecDescriptionList descs;
    segmentTracker->getCodecsDesc(&descs);
    for(auto it = descs.cbegin(); it != descs.cend(); ++it)
        fakeEsOut()->declareEs((*it)->getFmt());
}

FakeESOut::LockedFakeEsOut AbstractStream::fakeEsOut()
{
    return fakeesout->WithLock();
}

FakeESOut::LockedFakeEsOut AbstractStream::fakeEsOut() const
{
    return fakeesout->WithLock();
}
