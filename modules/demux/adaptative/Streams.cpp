/*
 * Streams.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN authors
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
#define __STDC_CONSTANT_MACROS
#include "Streams.hpp"
#include "StreamsType.hpp"
#include "http/HTTPConnection.hpp"
#include "http/HTTPConnectionManager.h"
#include "http/Chunk.h"
#include "logic/AbstractAdaptationLogic.h"
#include "SegmentTracker.hpp"
#include <vlc_stream.h>
#include <vlc_demux.h>

using namespace adaptative;
using namespace adaptative::http;
using namespace adaptative::logic;

Stream::Stream(const std::string &mime)
{
    init(mimeToType(mime), mimeToFormat(mime));
}

Stream::Stream(const StreamType type, const StreamFormat format)
{
    init(type, format);
}

void Stream::init(const StreamType type_, const StreamFormat format_)
{
    type = type_;
    format = format_;
    output = NULL;
    adaptationLogic = NULL;
    currentChunk = NULL;
    eof = false;
    segmentTracker = NULL;
}

Stream::~Stream()
{
    delete currentChunk;
    delete adaptationLogic;
    delete output;
    delete segmentTracker;
}

StreamType Stream::mimeToType(const std::string &mime)
{
    StreamType mimetype;
    if (!mime.compare(0, 6, "video/"))
        mimetype = StreamType::VIDEO;
    else if (!mime.compare(0, 6, "audio/"))
        mimetype = StreamType::AUDIO;
    else if (!mime.compare(0, 12, "application/"))
        mimetype = StreamType::APPLICATION;
    else /* unknown of unsupported */
        mimetype = StreamType::UNKNOWN;
    return mimetype;
}

StreamFormat Stream::mimeToFormat(const std::string &mime)
{
    StreamFormat format = StreamFormat::UNSUPPORTED;
    std::string::size_type pos = mime.find("/");
    if(pos != std::string::npos)
    {
        std::string tail = mime.substr(pos + 1);
        if(tail == "mp4")
            format = StreamFormat::MP4;
        else if (tail == "mp2t")
            format = StreamFormat::MPEG2TS;
    }
    return format;
}

void Stream::create(demux_t *demux, AbstractAdaptationLogic *logic, SegmentTracker *tracker)
{
    switch(format)
    {
        case StreamFormat::MP4:
            output = new MP4StreamOutput(demux);
            break;
        case StreamFormat::MPEG2TS:
            output = new MPEG2TSStreamOutput(demux);
            break;
        default:
            throw VLC_EBADVAR;
            break;
    }
    adaptationLogic = logic;
    segmentTracker = tracker;
}

bool Stream::isEOF() const
{
    return false;
}

mtime_t Stream::getPCR() const
{
    return output->getPCR();
}

int Stream::getGroup() const
{
    return output->getGroup();
}

int Stream::esCount() const
{
    return output->esCount();
}

bool Stream::operator ==(const Stream &stream) const
{
    return stream.type == type;
}

Chunk * Stream::getChunk()
{
    if (currentChunk == NULL)
    {
        currentChunk = segmentTracker->getNextChunk(type);
        if (currentChunk == NULL)
            eof = true;
    }
    return currentChunk;
}

bool Stream::seekAble() const
{
    return (output && output->seekAble());
}

Stream::status Stream::demux(HTTPConnectionManager *connManager, mtime_t nz_deadline)
{
    if(nz_deadline + VLC_TS_0 > output->getPCR()) /* not already demuxed */
    {
        /* need to read, demuxer still buffering, ... */
        if(read(connManager) <= 0)
            return Stream::status_eof;

        if(nz_deadline + VLC_TS_0 > output->getPCR()) /* need to read more */
            return Stream::status_buffering;
    }

    output->sendToDecoder(nz_deadline);
    return Stream::status_demuxed;
}

size_t Stream::read(HTTPConnectionManager *connManager)
{
    Chunk *chunk = getChunk();
    if(!chunk)
        return 0;

    if(!chunk->getConnection())
    {
       if(!connManager->connectChunk(chunk))
        return 0;
    }

    size_t readsize = 0;

    /* New chunk, do query */
    if(chunk->getBytesRead() == 0)
    {
        if(chunk->getConnection()->query(chunk->getPath()) != VLC_SUCCESS)
        {
            chunk->getConnection()->releaseChunk();
            currentChunk = NULL;
            delete chunk;
            return 0;
        }
    }

    /* Because we don't know Chunk size at start, we need to get size
       from content length */
    readsize = chunk->getBytesToRead();
    if (readsize > 32768)
        readsize = 32768;

    block_t *block = block_Alloc(readsize);
    if(!block)
        return 0;

    mtime_t time = mdate();
    ssize_t ret = chunk->getConnection()->read(block->p_buffer, readsize);
    time = mdate() - time;

    if(ret < 0)
    {
        block_Release(block);
        chunk->getConnection()->releaseChunk();
        currentChunk = NULL;
        delete chunk;
        return 0;
    }
    else
    {
        block->i_buffer = (size_t)ret;

        adaptationLogic->updateDownloadRate(block->i_buffer, time);

        if (chunk->getBytesToRead() == 0)
        {
            chunk->onDownload(block->p_buffer, block->i_buffer);
            chunk->getConnection()->releaseChunk();
            currentChunk = NULL;
            delete chunk;
        }
    }

    readsize = block->i_buffer;

    output->pushBlock(block);

    return readsize;
}

bool Stream::setPosition(mtime_t time, bool tryonly)
{
    bool ret = segmentTracker->setPosition(time, tryonly);
    if(!tryonly && ret)
        output->setPosition(time);
    return ret;
}

mtime_t Stream::getPosition() const
{
    return segmentTracker->getSegmentStart();
}

AbstractStreamOutput::AbstractStreamOutput(demux_t *demux)
{
    realdemux = demux;
    demuxstream = NULL;
    pcr = VLC_TS_0;
    group = 0;
    seekable = true;

    fakeesout = new es_out_t;
    if (!fakeesout)
        throw VLC_ENOMEM;

    vlc_mutex_init(&lock);

    fakeesout->pf_add = esOutAdd;
    fakeesout->pf_control = esOutControl;
    fakeesout->pf_del = esOutDel;
    fakeesout->pf_destroy = esOutDestroy;
    fakeesout->pf_send = esOutSend;
    fakeesout->p_sys = (es_out_sys_t*) this;
}

AbstractStreamOutput::~AbstractStreamOutput()
{
    if (demuxstream)
        stream_Delete(demuxstream);
    delete fakeesout;

    vlc_mutex_destroy(&lock);

    /* shouldn't be any */
    std::list<Demuxed *>::const_iterator it;
    for(it=queues.begin(); it!=queues.end();++it)
        delete *it;
}

mtime_t AbstractStreamOutput::getPCR() const
{
    return pcr;
}

int AbstractStreamOutput::getGroup() const
{
    return group;
}

int AbstractStreamOutput::esCount() const
{
    return queues.size();
}

void AbstractStreamOutput::pushBlock(block_t *block)
{
    stream_DemuxSend(demuxstream, block);
}

bool AbstractStreamOutput::seekAble() const
{
    return (demuxstream && seekable);
}

void AbstractStreamOutput::setPosition(mtime_t nztime)
{
    vlc_mutex_lock(&lock);
    std::list<Demuxed *>::const_iterator it;
    for(it=queues.begin(); it!=queues.end();++it)
    {
        Demuxed *pair = *it;
        if(pair->p_queue && pair->p_queue->i_dts > VLC_TS_0 + nztime)
            pair->drop();
    }
    pcr = VLC_TS_0;
    vlc_mutex_unlock(&lock);
    es_out_Control(realdemux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                   VLC_TS_0 + nztime);
}

void AbstractStreamOutput::sendToDecoder(mtime_t nzdeadline)
{
    vlc_mutex_lock(&lock);
    sendToDecoderUnlocked(nzdeadline);
    vlc_mutex_unlock(&lock);
}

void AbstractStreamOutput::sendToDecoderUnlocked(mtime_t nzdeadline)
{
    std::list<Demuxed *>::const_iterator it;
    for(it=queues.begin(); it!=queues.end();++it)
    {
        Demuxed *pair = *it;
        while(pair->p_queue && pair->p_queue->i_dts <= VLC_TS_0 + nzdeadline)
        {
            block_t *p_block = pair->p_queue;
            pair->p_queue = pair->p_queue->p_next;
            p_block->p_next = NULL;

            if(pair->pp_queue_last == &p_block->p_next)
                pair->pp_queue_last = &pair->p_queue;

            realdemux->out->pf_send(realdemux->out, pair->es_id, p_block);
        }
    }
}

AbstractStreamOutput::Demuxed::Demuxed()
{
    p_queue = NULL;
    pp_queue_last = &p_queue;
    es_id = NULL;
}

AbstractStreamOutput::Demuxed::~Demuxed()
{
    drop();
}

void AbstractStreamOutput::Demuxed::drop()
{
    block_ChainRelease(p_queue);
    p_queue = NULL;
    pp_queue_last = &p_queue;
}

/* Static callbacks */
es_out_id_t * AbstractStreamOutput::esOutAdd(es_out_t *fakees, const es_format_t *p_fmt)
{
    AbstractStreamOutput *me = (AbstractStreamOutput *) fakees->p_sys;
    es_out_id_t *p_es = me->realdemux->out->pf_add(me->realdemux->out, p_fmt);
    if(p_es)
    {
        vlc_mutex_lock(&me->lock);
        Demuxed *pair = new (std::nothrow) Demuxed();
        if(pair)
        {
            pair->es_id = p_es;
            me->queues.push_back(pair);
        }
        vlc_mutex_unlock(&me->lock);
    }
    return p_es;
}

int AbstractStreamOutput::esOutSend(es_out_t *fakees, es_out_id_t *p_es, block_t *p_block)
{
    AbstractStreamOutput *me = (AbstractStreamOutput *) fakees->p_sys;
    vlc_mutex_lock(&me->lock);
    std::list<Demuxed *>::const_iterator it;
    for(it=me->queues.begin(); it!=me->queues.end();++it)
    {
        Demuxed *pair = *it;
        if(pair->es_id == p_es)
        {
            block_ChainLastAppend(&pair->pp_queue_last, p_block);
            break;
        }
    }
    vlc_mutex_unlock(&me->lock);
    return VLC_SUCCESS;
}

void AbstractStreamOutput::esOutDel(es_out_t *fakees, es_out_id_t *p_es)
{
    AbstractStreamOutput *me = (AbstractStreamOutput *) fakees->p_sys;
    vlc_mutex_lock(&me->lock);
    std::list<Demuxed *>::iterator it;
    for(it=me->queues.begin(); it!=me->queues.end();++it)
    {
        if((*it)->es_id == p_es)
        {
            me->sendToDecoderUnlocked(INT64_MAX - VLC_TS_0);
            delete *it;
            me->queues.erase(it);
            break;
        }
    }
    vlc_mutex_unlock(&me->lock);
    me->realdemux->out->pf_del(me->realdemux->out, p_es);
}

int AbstractStreamOutput::esOutControl(es_out_t *fakees, int i_query, va_list args)
{
    AbstractStreamOutput *me = (AbstractStreamOutput *) fakees->p_sys;
    if (i_query == ES_OUT_SET_PCR )
    {
        me->pcr = (int64_t)va_arg( args, int64_t );
        return VLC_SUCCESS;
    }
    else if( i_query == ES_OUT_SET_GROUP_PCR )
    {
        me->group = (int) va_arg( args, int );
        me->pcr = (int64_t)va_arg( args, int64_t );
        return VLC_SUCCESS;
    }

    return me->realdemux->out->pf_control(me->realdemux->out, i_query, args);
}

void AbstractStreamOutput::esOutDestroy(es_out_t *fakees)
{
    AbstractStreamOutput *me = (AbstractStreamOutput *) fakees->p_sys;
    me->realdemux->out->pf_destroy(me->realdemux->out);
}
/* !Static callbacks */

MP4StreamOutput::MP4StreamOutput(demux_t *demux) :
    AbstractStreamOutput(demux)
{
    demuxstream = stream_DemuxNew(demux, "mp4", fakeesout);
    if(!demuxstream)
        throw VLC_EGENERIC;
}

MPEG2TSStreamOutput::MPEG2TSStreamOutput(demux_t *demux) :
    AbstractStreamOutput(demux)
{
    demuxstream = stream_DemuxNew(demux, "ts", fakeesout);
    if(!demuxstream)
        throw VLC_EGENERIC;
}
