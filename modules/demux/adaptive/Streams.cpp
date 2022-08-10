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
    format = StreamFormat::Type::Unknown;
    currentChunk = nullptr;
    eof = false;
    valid = true;
    disabled = false;
    contiguous = true;
    segmentgap = false;
    discontinuity = false;
    needrestart = false;
    inrestart = false;
    demuxfirstchunk = false;
    segmentTracker = nullptr;
    demuxersource = nullptr;
    demuxer = nullptr;
    fakeesout = nullptr;
    notfound_sequence = 0;
    mightalwaysstartfromzero = false;
    last_buffer_status = BufferingStatus::Lessthanmin;
    currentrep.width = 0;
    currentrep.height = 0;
    vlc_mutex_init(&lock);
}

bool AbstractStream::init(const StreamFormat &format_, SegmentTracker *tracker)
{
    /* Don't even try if not supported or already init */
    if(format_ == StreamFormat::Type::Unsupported || demuxersource)
        return false;

    demuxersource = new (std::nothrow) BufferedChunksSourceStream( VLC_OBJECT(p_realdemux), this );
    if(demuxersource)
    {
        CommandsFactory *factory = new (std::nothrow) CommandsFactory();
        AbstractCommandsQueue *commandsqueue = new (std::nothrow) CommandsQueue();
        if(factory && commandsqueue)
        {
            fakeesout = new (std::nothrow) FakeESOut(p_realdemux->out,
                                                     commandsqueue, factory);
            if(fakeesout)
            {
                /* All successful */
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
                if(mightalwaysstartfromzero)
                    fakeesout->setExpectedTimestamp(VLC_TICK_0 + segmentTracker->getPlaybackTime());
                declaredCodecs();
                return true;
            }
        }
        delete factory;
        delete commandsqueue;
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
    last_buffer_status = BufferingStatus::Lessthanmin;
    inrestart = false;
    needrestart = false;
    discontinuity = false;
    if(!demuxer || demuxer->needsRestartOnSeek()) /* needs (re)start */
    {
        delete currentChunk;
        currentChunk = nullptr;
        needrestart = false;
        segmentgap = false;

        fakeEsOut()->resetTimestamps();

        fakeEsOut()->commandsQueue()->Abort( true );
        startTimeContext = SegmentTimes();
        currentTimeContext = SegmentTimes();
        prevEndTimeContext = SegmentTimes();
        currentChunk = getNextChunk();
        if(mightalwaysstartfromzero)
            fakeEsOut()->setExpectedTimestamp(VLC_TICK_0 + seekMediaTime);
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

vlc_tick_t AbstractStream::getMinAheadTime() const
{
    if(!segmentTracker)
        return 0;
    return segmentTracker->getMinAheadTime();
}

Times AbstractStream::getFirstTimes() const
{
    vlc_mutex_locker locker(&lock);

    if(!valid || disabled)
        return Times();

    Times times = fakeEsOut()->commandsQueue()->getFirstTimes();
    if(times.continuous == VLC_TICK_INVALID)
        times = fakeEsOut()->commandsQueue()->getPCR();
    return times;
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

AbstractStream::StreamPosition::StreamPosition()
{
    number = std::numeric_limits<uint64_t>::max();
    pos = -1;
}

bool AbstractStream::reactivate(const StreamPosition &pos)
{
    vlc_mutex_locker locker(&lock);
    if(setPosition(pos, false))
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

    if(!currentChunk)
    {
        segmentgap = false;
        currentChunk = getNextChunk();
        needrestart = false;
        discontinuity = false;
    }

    demuxersource->Reset();
    demuxfirstchunk = true;
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

vlc_tick_t AbstractStream::getDemuxedAmount(Times from) const
{
    vlc_tick_t i_demuxed = fakeEsOut()->commandsQueue()->getDemuxedAmount(from).continuous;
    if(contiguous)
    {
        vlc_tick_t i_media_demuxed = fakeEsOut()->commandsQueue()->getDemuxedMediaAmount(from).segment.media;
        if(i_media_demuxed > i_demuxed)
            i_demuxed = i_media_demuxed;
    }
    return i_demuxed;
}

AbstractStream::BufferingStatus
AbstractStream::getBufferAndStatus(const Times &deadline,
                                   vlc_tick_t i_min_buffering,
                                   vlc_tick_t i_max_buffering,
                                   vlc_tick_t *pi_demuxed)
{
    if(last_buffer_status == BufferingStatus::End)
        return BufferingStatus::End;
    *pi_demuxed = getDemuxedAmount(deadline);

    if(*pi_demuxed < i_max_buffering) /* need to read more */
    {
        if(*pi_demuxed < i_min_buffering)
            return BufferingStatus::Lessthanmin; /* high prio */
        return BufferingStatus::Ongoing;
    }

    return BufferingStatus::Full;
}

AbstractStream::BufferingStatus AbstractStream::bufferize(Times deadline,
                                                           vlc_tick_t i_min_buffering,
                                                           vlc_tick_t i_extra_buffering,
                                                           vlc_tick_t i_target_buffering,
                                                           bool b_keep_alive)
{
    last_buffer_status = doBufferize(deadline, i_min_buffering, i_extra_buffering,
                                     i_target_buffering, b_keep_alive);
    return last_buffer_status;
}

AbstractStream::BufferingStatus AbstractStream::doBufferize(Times deadline,
                                                             vlc_tick_t i_min_buffering,
                                                             vlc_tick_t i_max_buffering,
                                                             vlc_tick_t i_target_buffering,
                                                             bool b_keep_alive)
{
    vlc_mutex_lock(&lock);

    /* Ensure it is configured */
    if(!segmentTracker || !valid)
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

    if(!contiguous)
    {
        if(!fakeEsOut()->hasSynchronizationReference())
        {
            if(!demuxer)
            {
                /* We always need a prepared chunk info for querying a syncref */
                if(!currentChunk)
                {
                    currentChunk = getNextChunk();
                    if(!currentChunk)
                    {
                        vlc_mutex_unlock(&lock);
                        return BufferingStatus::End;
                    }
                    segmentgap = false;
                    needrestart = false;
                    discontinuity = false;
                }
            }
            SynchronizationReference r;
            if(segmentTracker->getSynchronizationReference(currentSequence, startTimeContext.media, r))
            {
                fakeEsOut()->setSynchronizationReference(r);
            }
            else
            {
                msg_Dbg(p_realdemux, "Waiting sync reference for seq %" PRIu64, currentSequence);
                vlc_mutex_unlock(&lock);
                return BufferingStatus::Suspended;
            }
        }
    }

    if(!demuxer)
    {
        if(!startDemux())
        {
            valid = false; /* Prevent further retries */
            fakeEsOut()->commandsQueue()->setEOF(true);
            vlc_mutex_unlock(&lock);
            return BufferingStatus::End;
        }
    }

    vlc_tick_t i_demuxed = fakeEsOut()->commandsQueue()->getDemuxedAmount(deadline).continuous;
    if(!contiguous && prevEndTimeContext.media != VLC_TICK_INVALID
       && deadline.segment.media != VLC_TICK_INVALID)
    {
        vlc_tick_t i_mediaamount = fakeEsOut()->commandsQueue()->getDemuxedMediaAmount(deadline).segment.media;
        if(i_mediaamount > i_demuxed)
            i_demuxed = i_mediaamount;
    }

    segmentTracker->notifyBufferingLevel(i_min_buffering, i_max_buffering, i_demuxed, i_target_buffering);
    if(i_demuxed < i_max_buffering) /* not already demuxed */
    {
        Times extdeadline = fakeEsOut()->commandsQueue()->getBufferingLevel();
        extdeadline.offsetBy((i_max_buffering - i_demuxed) / 4);

        Times newdeadline = deadline;
        newdeadline.offsetBy(VLC_TICK_FROM_SEC(1));

        if(extdeadline.continuous < newdeadline.continuous)
            deadline = extdeadline;
        else
            deadline = newdeadline;

        /* need to read, demuxer still buffering, ... */
        vlc_mutex_unlock(&lock);
        Demuxer::Status demuxStatus = demuxer->demux(deadline.continuous);
        fakeEsOut()->scheduleNecessaryMilestone();
        vlc_mutex_lock(&lock);
        if(demuxStatus != Demuxer::Status::Success)
        {
            if(discontinuity || needrestart)
            {
                msg_Dbg(p_realdemux, "Restarting demuxer %d %d", needrestart, discontinuity);
                prepareRestart(discontinuity);
                if(discontinuity)
                {
                    msg_Dbg(p_realdemux, "Draining on discontinuity");
                    fakeEsOut()->commandsQueue()->setDraining();
                    fakeEsOut()->setSegmentStartTimes(startTimeContext);
                    assert(startTimeContext.media);
                }
                assert(startTimeContext.media);
                if(!fakeEsOut()->hasSegmentStartTimes())
                    fakeEsOut()->setSegmentStartTimes(startTimeContext);
                if(!fakeEsOut()->hasSynchronizationReference())
                {
                    SynchronizationReference r(currentSequence, Times());
                    fakeEsOut()->setSynchronizationReference(r);
                }
                discontinuity = false;
                needrestart = false;
                vlc_mutex_unlock(&lock);
                return BufferingStatus::Ongoing;
            }
            fakeEsOut()->commandsQueue()->setEOF(true);
            vlc_mutex_unlock(&lock);
            return BufferingStatus::End;
        }

        if(deadline.continuous != VLC_TICK_INVALID)
        {
            i_demuxed = fakeEsOut()->commandsQueue()->getDemuxedAmount(deadline).continuous;
            segmentTracker->notifyBufferingLevel(i_min_buffering, i_max_buffering, i_demuxed, i_target_buffering);
        }
        else
        {
            /* On initial pass, there's no demux time known, we need to fake it */
            if(fakeEsOut()->commandsQueue()->getBufferingLevel().continuous != VLC_TICK_INVALID)
                i_demuxed = i_min_buffering;
        }
    }
    vlc_mutex_unlock(&lock);

    Times first = fakeEsOut()->commandsQueue()->getFirstTimes();
    if(contiguous && first.continuous != VLC_TICK_INVALID && first.segment.demux != VLC_TICK_INVALID)
        segmentTracker->updateSynchronizationReference(currentSequence, first);

    if(i_demuxed < i_max_buffering) /* need to read more */
    {
        if(i_demuxed < i_min_buffering)
            return BufferingStatus::Lessthanmin; /* high prio */
        return BufferingStatus::Ongoing;
    }
    return BufferingStatus::Full;
}

AbstractStream::Status AbstractStream::dequeue(Times deadline, Times *times)
{
    vlc_mutex_locker locker(&lock);

    if(fakeEsOut()->commandsQueue()->isDraining())
    {
        AdvDebug(vlc_tick_t pcrvalue = fakeEsOut()->commandsQueue()->getPCR().continuous;
                 vlc_tick_t dtsvalue = fakeEsOut()->commandsQueue()->getFirstTimes().continuous;
                 vlc_tick_t bufferingLevel = fakeEsOut()->commandsQueue()->getBufferingLevel().continuous;
                 msg_Dbg(p_realdemux, "Stream pcr %" PRId64 " dts %" PRId64 " deadline %" PRId64 " buflevel %" PRId64 "(+%" PRId64 ") [DRAINING] :%s",
                         pcrvalue, dtsvalue, deadline.continuous, bufferingLevel,
                         pcrvalue ? bufferingLevel - pcrvalue : 0,
                         description.c_str()));

        *times = fakeEsOut()->commandsQueue()->Process(deadline);
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
        *times = deadline;
        return Status::Eof;
    }

    vlc_tick_t bufferingLevel = fakeEsOut()->commandsQueue()->getBufferingLevel().continuous;
    AdvDebug(vlc_tick_t pcrvalue = fakeEsOut()->commandsQueue()->getPCR().continuous;
             vlc_tick_t dtsvalue = fakeEsOut()->commandsQueue()->getFirstTimes().continuous;
             msg_Dbg(p_realdemux, "Stream pcr %" PRId64 " dts %" PRId64 " deadline %" PRId64 " buflevel %" PRId64 "(+%" PRId64 "): %s",
                     pcrvalue, dtsvalue, deadline.continuous, bufferingLevel,
                     pcrvalue ? bufferingLevel - pcrvalue : 0,
                     description.c_str()));

    if(deadline.continuous <= bufferingLevel) /* demuxed */
    {
        *times = fakeEsOut()->commandsQueue()->Process(deadline);
        return Status::Demuxed;
    }
    else if(!contiguous &&
            fakeEsOut()->commandsQueue()->getDemuxedMediaAmount(deadline).segment.media > 0)
    {
        *times = deadline;
        fakeEsOut()->commandsQueue()->Process(Times()); /* handle untimed events (es add) */
        return Status::Demuxed;
    }

    return Status::Buffering;
}

ChunkInterface * AbstractStream::getNextChunk() const
{
    const bool b_restarting = fakeEsOut()->restarting();
    ChunkInterface *ck = segmentTracker->getNextChunk(!b_restarting);
    if(ck && !fakeEsOut()->hasSegmentStartTimes())
        fakeEsOut()->setSegmentStartTimes(startTimeContext);

    if(ck && !fakeEsOut()->hasSynchronizationReference())
    {
        assert(fakeEsOut()->hasSegmentStartTimes());
        SynchronizationReference r;
        if(segmentTracker->getSynchronizationReference(currentSequence, startTimeContext.media, r))
            fakeEsOut()->setSynchronizationReference(r);
    }
    return ck;
}

block_t * AbstractStream::readNextBlock()
{
    if (currentChunk == nullptr && !eof)
    {
        segmentgap = false;
        currentChunk = getNextChunk();
    }

    if(demuxfirstchunk)
    {
        /* clear up discontinuity on demux start (discontinuity on start segment bug) */
        discontinuity = false;
        needrestart = false;
    }
    else if(discontinuity || needrestart)
    {
        msg_Info(p_realdemux, "Ending demuxer stream. %s%s",
                 discontinuity ? "[discontinuity]" : "",
                 needrestart ? "[needrestart]" : "");
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
            segmentgap = true;
        }
        delete currentChunk;
        currentChunk = nullptr;
        return nullptr;
    }
    else notfound_sequence = 0;

    demuxfirstchunk = false;

    if (!currentChunk->hasMoreData())
    {
        delete currentChunk;
        currentChunk = nullptr;
    }

    block = checkBlock(block, b_segment_head_chunk);

    return block;
}

bool AbstractStream::setPosition(const StreamPosition &pos, bool tryonly)
{
    if(!seekAble())
        return false;

    bool b_needs_restart = demuxer ? demuxer->needsRestartOnSeek() : true;
    bool ret = segmentTracker->setPositionByTime(pos.times.segment.media,
                                                 b_needs_restart, tryonly);
    if(!tryonly && ret)
    {
// in some cases, media time seek != sent dts
//        es_out_Control(p_realdemux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
//                       VLC_TICK_0 + time);
    }
    return ret;
}

bool AbstractStream::getMediaPlaybackTimes(vlc_tick_t *start, vlc_tick_t *end,
                                           vlc_tick_t *length) const
{
    return segmentTracker->getMediaPlaybackRange(start, end, length);
}

bool AbstractStream::getMediaAdvanceAmount(vlc_tick_t *duration) const
{
    if(startTimeContext.media == VLC_TICK_INVALID)
        return false;
    *duration = currentTimeContext.media - startTimeContext.media;
    return true;
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
    if( p_fmt->i_cat == VIDEO_ES && p_fmt->video.i_visible_width == 0)
    {
        p_fmt->video.i_visible_width = currentrep.width;
        p_fmt->video.i_visible_height = currentrep.height;
    }
}

block_t *AbstractStream::checkBlock(block_t *p_block, bool)
{
    return p_block;
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
    switch(format)
    {
        case StreamFormat::Type::MP4:
            ret = new Demuxer(p_obj, "mp4", out, source);
            break;

        case StreamFormat::Type::MPEG2TS:
            ret = new Demuxer(p_obj, "ts", out, source);
            break;

        default:
        case StreamFormat::Type::Unsupported:
            break;
    }
    return ret;
}

void AbstractStream::trackerEvent(const TrackerEvent &ev)
{
    switch(ev.getType())
    {
        case TrackerEvent::Type::Discontinuity:
        {
            const DiscontinuityEvent &event =
                    static_cast<const DiscontinuityEvent &>(ev);
            discontinuity = true;
            currentSequence = event.discontinuitySequenceNumber;
        }
            break;

        case TrackerEvent::Type::SegmentGap:
            segmentgap = true;
            prevEndTimeContext = SegmentTimes();
            currentTimeContext = SegmentTimes(); /* fired before segmentchanged */
            break;

        case TrackerEvent::Type::FormatChange:
        {
            const FormatChangedEvent &event =
                    static_cast<const FormatChangedEvent &>(ev);
            /* Check if our current demux is still valid */
            if(*event.format != format)
            {
                /* Format has changed between segments, we need to drain and change demux */
                msg_Info(p_realdemux, "Changing stream format %s -> %s",
                         format.str().c_str(), event.format->str().c_str());
                format = *event.format;
                needrestart = true;
            }
        }
            break;

        case TrackerEvent::Type::RepresentationSwitch:
        {
            const RepresentationSwitchEvent &event =
                    static_cast<const RepresentationSwitchEvent &>(ev);
            if(demuxer && !inrestart && event.prev)
            {
                if(!demuxer->bitstreamSwitchCompatible() ||
                   /* HLS variants can move from TS to Raw AAC */
                   format == StreamFormat(StreamFormat::Type::Unknown) ||
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
            if(event.next)
            {
                currentrep.width = event.next->getWidth() > 0 ? event.next->getWidth() : 0;
                currentrep.height = event.next->getHeight() > 0 ? event.next->getHeight() : 0;
            }
            else
            {
                currentrep.width = 0;
                currentrep.height = 0;
            }
        }
            break;

        case TrackerEvent::Type::RepresentationUpdated:
        {
            if(last_buffer_status == BufferingStatus::Suspended)
                last_buffer_status = BufferingStatus::Lessthanmin;
        }
            break;

        case TrackerEvent::Type::RepresentationUpdateFailed:
        {
            fakeEsOut()->commandsQueue()->setEOF(true);
            msg_Err(p_realdemux, "Could not update %s anymore, disabling", description.c_str());
        }
            break;

        case TrackerEvent::Type::SegmentChange:
        {
            const SegmentChangedEvent &event =
                    static_cast<const SegmentChangedEvent &>(ev);
            if(demuxer && demuxer->needsRestartOnEachSegment() && !inrestart)
            {
                needrestart = true;
            }
            prevEndTimeContext = currentTimeContext;
            prevEndTimeContext.offsetBy(currentDuration);
            fakeEsOut()->setSegmentProgressTimes(prevEndTimeContext);
            currentTimeContext.media = event.starttime;
            currentTimeContext.display = event.displaytime;
            currentSequence = event.sequence;
            currentDuration = event.duration;
            if(startTimeContext.media == VLC_TICK_INVALID)
                startTimeContext = currentTimeContext;
        }
            break;

        case TrackerEvent::Type::PositionChange:
        {
            const PositionChangedEvent &event =
                    static_cast<const PositionChangedEvent &>(ev);
            resetForNewPosition(event.resumeTime);
        }
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
    {
        const es_format_t *fmt = (*it)->getFmt();
        if(fmt->i_cat != UNKNOWN_ES)
            fakeEsOut()->declareEs(fmt);
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
