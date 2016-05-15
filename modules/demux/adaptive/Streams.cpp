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
#include "http/HTTPConnection.hpp"
#include "http/HTTPConnectionManager.h"
#include "playlist/BaseRepresentation.h"
#include "playlist/SegmentChunk.hpp"
#include "plumbing/SourceStream.hpp"
#include "plumbing/CommandsQueue.hpp"
#include "tools/Debug.hpp"
#include <vlc_demux.h>

using namespace adaptive;
using namespace adaptive::http;

AbstractStream::AbstractStream(demux_t * demux_, const StreamFormat &format_)
{
    p_realdemux = demux_;
    format = format_;
    currentChunk = NULL;
    eof = false;
    dead = false;
    disabled = false;
    flushing = false;
    discontinuity = false;
    segmentTracker = NULL;
    pcr = VLC_TS_INVALID;

    demuxer = NULL;
    fakeesout = NULL;

    /* Don't even try if not supported */
    if((unsigned)format == StreamFormat::UNSUPPORTED)
        throw VLC_EGENERIC;

    demuxersource = new (std::nothrow) ChunksSourceStream( VLC_OBJECT(p_realdemux), this );
    if(!demuxersource)
        throw VLC_EGENERIC;

    CommandsFactory *factory = new CommandsFactory();
    fakeesout = new (std::nothrow) FakeESOut(p_realdemux->out, factory);
    if(!fakeesout)
    {
        delete demuxersource;
        throw VLC_EGENERIC;
    }
    fakeesout->setExtraInfoProvider( this );
}

AbstractStream::~AbstractStream()
{
    delete currentChunk;
    delete segmentTracker;

    delete demuxer;
    delete demuxersource;
    delete fakeesout;
}


void AbstractStream::bind(SegmentTracker *tracker, HTTPConnectionManager *conn)
{
    segmentTracker = tracker;
    segmentTracker->registerListener(this);
    connManager = conn;
}

void AbstractStream::prepareFormatChange()
{
    if(demuxer)
    {
        /* Enqueue Del Commands for all current ES */
        demuxer->drain();
        /* Enqueue Del Commands for all current ES */
        fakeesout->scheduleAllForDeletion();
        fakeesout->schedulePCRReset();
        fakeesout->commandsqueue.Commit();
        /* ignoring demuxer's own Del commands */
        fakeesout->commandsqueue.setDrop(true);
        delete demuxer;
        fakeesout->commandsqueue.setDrop(false);
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

bool AbstractStream::isEOF() const
{
    return dead;
}

mtime_t AbstractStream::getPCR() const
{
    return pcr;
}

mtime_t AbstractStream::getMinAheadTime() const
{
    if(!segmentTracker)
        return 0;
    return segmentTracker->getMinAheadTime();
}

mtime_t AbstractStream::getBufferingLevel() const
{
    return fakeesout->commandsqueue.getBufferingLevel();
}

mtime_t AbstractStream::getFirstDTS() const
{
    return fakeesout->commandsqueue.getFirstDTS();
}

int AbstractStream::esCount() const
{
    return fakeesout->esCount();
}

bool AbstractStream::seekAble() const
{
    return (demuxer &&
            !fakeesout->restarting() &&
            !discontinuity &&
            !flushing );
}

bool AbstractStream::isSelected() const
{
    return fakeesout->hasSelectedEs();
}

bool AbstractStream::reactivate(mtime_t basetime)
{
    if(setPosition(basetime, false))
    {
        disabled = false;
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

    try
    {
        demuxersource->Reset();
        demuxer = createDemux(format);
    } catch(int) {
        msg_Err(p_realdemux, "Failed to create demuxer");
    }

    return !!demuxer;
}

bool AbstractStream::restartDemux()
{
    if(!demuxer)
    {
        return startDemux();
    }
    else if(demuxer->reinitsOnSeek())
    {
        /* Push all ES as recycling candidates */
        fakeesout->recycleAll();
        /* Restart with ignoring pushes to queue */
        return demuxer->restart(fakeesout->commandsqueue);
    }
    fakeesout->commandsqueue.Commit();
    return true;
}

bool AbstractStream::isDisabled() const
{
    return disabled;
}

AbstractStream::status AbstractStream::demux(mtime_t nz_deadline, bool send)
{
    /* Ensure it is configured */
    if(!segmentTracker || !connManager || dead)
        return AbstractStream::status_eof;

    if(flushing)
    {
        if(!send)
            return AbstractStream::status_buffering;

        pcr = fakeesout->commandsqueue.Process(p_realdemux->out, VLC_TS_0 + nz_deadline);
        if(!fakeesout->commandsqueue.isEmpty())
            return AbstractStream::status_demuxed;

        fakeesout->commandsqueue.Abort(true); /* reset buffering level */
        flushing = false;
        pcr = 0;
        return AbstractStream::status_dis;
    }

    if(!demuxer)
    {
        format = segmentTracker->initialFormat();
        if(!startDemux())
        {
            /* If demux fails because of probing failure / wrong format*/
            if(discontinuity)
            {
                msg_Dbg( p_realdemux, "Flushing on format change" );
                prepareFormatChange();
                discontinuity = false;
                flushing = true;
                return AbstractStream::status_buffering;
            }
            dead = true; /* Prevent further retries */
            return AbstractStream::status_eof;
        }
    }

    if(nz_deadline + VLC_TS_0 > getBufferingLevel()) /* not already demuxed */
    {
        if(!segmentTracker->segmentsListReady()) /* Live Streams */
            return AbstractStream::status_buffering_ahead;

        /* need to read, demuxer still buffering, ... */
        if(demuxer->demux(nz_deadline) != VLC_DEMUXER_SUCCESS)
        {
            if(discontinuity)
            {
                msg_Dbg( p_realdemux, "Flushing on discontinuity" );
                prepareFormatChange();
                discontinuity = false;
                flushing = true;
                return AbstractStream::status_buffering;
            }

            fakeesout->commandsqueue.Commit();
            if(fakeesout->commandsqueue.isEmpty())
                return AbstractStream::status_eof;
        }
        else if(nz_deadline + VLC_TS_0 > getBufferingLevel()) /* need to read more */
        {
            return AbstractStream::status_buffering;
        }
    }

    AdvDebug(msg_Dbg(p_realdemux, "Stream %s pcr %ld dts %ld deadline %ld buflevel %ld",
             description.c_str(), getPCR(), getFirstDTS(), nz_deadline, getBufferingLevel()));

    if(send)
        pcr = fakeesout->commandsqueue.Process( p_realdemux->out, VLC_TS_0 + nz_deadline );

    /* Disable streams that are not selected (alternate streams) */
    if(esCount() && !isSelected() && !fakeesout->restarting())
    {
        disabled = true;
        segmentTracker->reset();
    }

    return AbstractStream::status_demuxed;
}

block_t * AbstractStream::readNextBlock()
{
    if (currentChunk == NULL && !eof)
        currentChunk = segmentTracker->getNextChunk(!fakeesout->restarting(), connManager);

    if(discontinuity)
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
        delete currentChunk;
        currentChunk = NULL;
        return NULL;
    }

    if (currentChunk->isEmpty())
    {
        delete currentChunk;
        currentChunk = NULL;
    }

    block = checkBlock(block, b_segment_head_chunk);

    return block;
}

bool AbstractStream::setPosition(mtime_t time, bool tryonly)
{
    if(!demuxer)
        return false;

    bool ret = segmentTracker->setPositionByTime(time, demuxer->reinitsOnSeek(), tryonly);
    if(!tryonly && ret)
    {
        if(demuxer->reinitsOnSeek())
        {
            if(currentChunk)
                delete currentChunk;
            currentChunk = NULL;

            if( !restartDemux() )
                dead = true;

            /* Check if we need to set an offset as the demuxer
             * will start from zero from seek point */
            if(demuxer->alwaysStartsFromZero())
                fakeesout->setTimestampOffset(time);
        }

        pcr = VLC_TS_INVALID;
        es_out_Control(p_realdemux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                       VLC_TS_0 + time);
    }
    return ret;
}

mtime_t AbstractStream::getPlaybackTime() const
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
        default:
            break;
    }
}
