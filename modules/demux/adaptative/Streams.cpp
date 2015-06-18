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

void Stream::create(demux_t *demux, AbstractAdaptationLogic *logic,
                    SegmentTracker *tracker, AbstractStreamOutputFactory &factory)
{
    output = factory.create(demux, format);
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

mtime_t Stream::getFirstDTS() const
{
    return output->getFirstDTS();
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
        currentChunk = segmentTracker->getNextChunk(type, output->switchAllowed());
        if (currentChunk == NULL)
            eof = true;
    }
    return currentChunk;
}

bool Stream::seekAble() const
{
    return (output && output->seekAble());
}

Stream::status Stream::demux(HTTPConnectionManager *connManager, mtime_t nz_deadline, bool send)
{
    if(nz_deadline + VLC_TS_0 > output->getPCR()) /* not already demuxed */
    {
        /* need to read, demuxer still buffering, ... */
        if(read(connManager) <= 0)
            return Stream::status_eof;

        if(nz_deadline + VLC_TS_0 > output->getPCR()) /* need to read more */
            return Stream::status_buffering;
    }

    if(send)
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
        chunk->onDownload(&block);

        if (chunk->getBytesToRead() == 0)
        {
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
    bool ret = segmentTracker->setPosition(time, output->reinitsOnSeek(), tryonly);
    if(!tryonly && ret)
    {
        output->setPosition(time);
        if(output->reinitsOnSeek())
        {
            if(currentChunk)
            {
                currentChunk->getConnection()->releaseChunk();
                delete currentChunk;
            }
            currentChunk = NULL;
        }
    }
    return ret;
}

mtime_t Stream::getPosition() const
{
    return segmentTracker->getSegmentStart();
}

void Stream::prune()
{
    segmentTracker->pruneFromCurrent();
}

AbstractStreamOutput::AbstractStreamOutput(demux_t *demux)
{
    realdemux = demux;
    pcr = VLC_TS_0;
    group = 0;
}

AbstractStreamOutput::~AbstractStreamOutput()
{
}

mtime_t AbstractStreamOutput::getPCR() const
{
    return pcr;
}

int AbstractStreamOutput::getGroup() const
{
    return group;
}

BaseStreamOutput::BaseStreamOutput(demux_t *demux, const std::string &name) :
    AbstractStreamOutput(demux)
{
    this->name = name;
    seekable = true;
    restarting = false;
    demuxstream = NULL;
    b_drop = false;

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

    demuxstream = stream_DemuxNew(realdemux, name.c_str(), fakeesout);
    if(!demuxstream)
        throw VLC_EGENERIC;
}

BaseStreamOutput::~BaseStreamOutput()
{
    if (demuxstream)
        stream_Delete(demuxstream);

    /* shouldn't be any */
    std::list<Demuxed *>::const_iterator it;
    for(it=queues.begin(); it!=queues.end();++it)
        delete *it;

    delete fakeesout;
    vlc_mutex_destroy(&lock);
}

mtime_t BaseStreamOutput::getFirstDTS() const
{
    mtime_t ret = VLC_TS_INVALID;
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    std::list<Demuxed *>::const_iterator it;
    for(it=queues.begin(); it!=queues.end();++it)
    {
        const Demuxed *pair = *it;
        const block_t *p_block = pair->p_queue;
        while( p_block && p_block->i_dts == VLC_TS_INVALID )
        {
            p_block = p_block->p_next;
        }

        if(p_block)
        {
            ret = p_block->i_dts;
            break;
        }
    }
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return ret;
}

int BaseStreamOutput::esCount() const
{
    return queues.size();
}

void BaseStreamOutput::pushBlock(block_t *block)
{
    stream_DemuxSend(demuxstream, block);
}

bool BaseStreamOutput::seekAble() const
{
    bool b_canswitch = switchAllowed();
    return (demuxstream && seekable && b_canswitch);
}

void BaseStreamOutput::setPosition(mtime_t nztime)
{
    vlc_mutex_lock(&lock);
    std::list<Demuxed *>::const_iterator it;
    for(it=queues.begin(); it!=queues.end();++it)
    {
        Demuxed *pair = *it;
        if(pair->p_queue && pair->p_queue->i_dts > VLC_TS_0 + nztime)
            pair->drop();
    }
    /* disable appending until restarted */
    b_drop = true;
    vlc_mutex_unlock(&lock);

    if(reinitsOnSeek())
        restart();

    vlc_mutex_lock(&lock);
    b_drop = false;
    pcr = VLC_TS_INVALID;
    vlc_mutex_unlock(&lock);

    es_out_Control(realdemux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                   VLC_TS_0 + nztime);
}

bool BaseStreamOutput::restart()
{
    stream_t *newdemuxstream = stream_DemuxNew(realdemux, name.c_str(), fakeesout);
    if(!newdemuxstream)
        return false;

    vlc_mutex_lock(&lock);
    restarting = true;
    stream_t *olddemuxstream = demuxstream;
    demuxstream = newdemuxstream;
    vlc_mutex_unlock(&lock);

    if(olddemuxstream)
        stream_Delete(olddemuxstream);

    return true;
}

bool BaseStreamOutput::reinitsOnSeek() const
{
    return true;
}

bool BaseStreamOutput::switchAllowed() const
{
    bool b_allowed;
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    b_allowed = !restarting;
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return b_allowed;
}

void BaseStreamOutput::sendToDecoder(mtime_t nzdeadline)
{
    vlc_mutex_lock(&lock);
    sendToDecoderUnlocked(nzdeadline);
    vlc_mutex_unlock(&lock);
}

void BaseStreamOutput::sendToDecoderUnlocked(mtime_t nzdeadline)
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

BaseStreamOutput::Demuxed::Demuxed(es_out_id_t *id, const es_format_t *fmt)
{
    p_queue = NULL;
    pp_queue_last = &p_queue;
    es_id = id;
    es_format_Init(&fmtcpy, UNKNOWN_ES, 0);
    es_format_Copy(&fmtcpy, fmt);
}

BaseStreamOutput::Demuxed::~Demuxed()
{
    es_format_Clean(&fmtcpy);
    drop();
}

void BaseStreamOutput::Demuxed::drop()
{
    block_ChainRelease(p_queue);
    p_queue = NULL;
    pp_queue_last = &p_queue;
}

/* Static callbacks */
es_out_id_t * BaseStreamOutput::esOutAdd(es_out_t *fakees, const es_format_t *p_fmt)
{
    BaseStreamOutput *me = (BaseStreamOutput *) fakees->p_sys;

    es_out_id_t *p_es = NULL;

    vlc_mutex_lock(&me->lock);

    std::list<Demuxed *>::iterator it;
    bool b_hasestorecyle = false;
    for(it=me->queues.begin(); it!=me->queues.end();++it)
    {
        Demuxed *pair = *it;
        b_hasestorecyle |= pair->recycle;

        if( p_es )
            continue;

        if( me->restarting )
        {
            /* If we're recycling from same format */
            if( es_format_IsSimilar(p_fmt, &pair->fmtcpy) &&
                    p_fmt->i_extra == pair->fmtcpy.i_extra &&
                    !memcmp(p_fmt->p_extra, pair->fmtcpy.p_extra, p_fmt->i_extra) )
            {
                msg_Err(me->realdemux, "using recycled");
                pair->recycle = false;
                p_es = pair->es_id;
            }
        }
    }

    if(!b_hasestorecyle)
    {
        me->restarting = false;
    }

    if(!p_es)
    {
        p_es = me->realdemux->out->pf_add(me->realdemux->out, p_fmt);
        if(p_es)
        {
            Demuxed *pair = new (std::nothrow) Demuxed(p_es, p_fmt);
            if(pair)
                me->queues.push_back(pair);
        }
    }
    vlc_mutex_unlock(&me->lock);

    return p_es;
}

int BaseStreamOutput::esOutSend(es_out_t *fakees, es_out_id_t *p_es, block_t *p_block)
{
    BaseStreamOutput *me = (BaseStreamOutput *) fakees->p_sys;
    vlc_mutex_lock(&me->lock);
    if(me->b_drop)
    {
        block_ChainRelease( p_block );
    }
    else
    {
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
    }
    vlc_mutex_unlock(&me->lock);
    return VLC_SUCCESS;
}

void BaseStreamOutput::esOutDel(es_out_t *fakees, es_out_id_t *p_es)
{
    BaseStreamOutput *me = (BaseStreamOutput *) fakees->p_sys;
    vlc_mutex_lock(&me->lock);
    std::list<Demuxed *>::iterator it;
    for(it=me->queues.begin(); it!=me->queues.end();++it)
    {
        if((*it)->es_id == p_es)
        {
            me->sendToDecoderUnlocked(INT64_MAX - VLC_TS_0);
            break;
        }
    }

    if(it != me->queues.end())
    {
        if(me->restarting)
        {
            (*it)->recycle = true;
        }
        else
        {
            delete *it;
            me->queues.erase(it);
        }
    }

    if(!me->restarting)
        me->realdemux->out->pf_del(me->realdemux->out, p_es);

    vlc_mutex_unlock(&me->lock);
}

int BaseStreamOutput::esOutControl(es_out_t *fakees, int i_query, va_list args)
{
    BaseStreamOutput *me = (BaseStreamOutput *) fakees->p_sys;
    if (i_query == ES_OUT_SET_PCR )
    {
        vlc_mutex_lock(&me->lock);
        me->pcr = (int64_t)va_arg( args, int64_t );
        vlc_mutex_unlock(&me->lock);
        return VLC_SUCCESS;
    }
    else if( i_query == ES_OUT_SET_GROUP_PCR )
    {
        vlc_mutex_lock(&me->lock);
        me->group = (int) va_arg( args, int );
        me->pcr = (int64_t)va_arg( args, int64_t );
        vlc_mutex_unlock(&me->lock);
        return VLC_SUCCESS;
    }
    else if( i_query == ES_OUT_GET_ES_STATE )
    {
        va_arg( args, es_out_id_t * );
        bool *pb = va_arg( args, bool * );
        *pb = true;
        return VLC_SUCCESS;
    }

    vlc_mutex_lock(&me->lock);
    bool b_restarting = me->restarting;
    vlc_mutex_unlock(&me->lock);

    if( b_restarting )
    {
        return VLC_EGENERIC;
    }

    return me->realdemux->out->pf_control(me->realdemux->out, i_query, args);
}

void BaseStreamOutput::esOutDestroy(es_out_t *fakees)
{
    BaseStreamOutput *me = (BaseStreamOutput *) fakees->p_sys;
    me->realdemux->out->pf_destroy(me->realdemux->out);
}
/* !Static callbacks */

AbstractStreamOutput *DefaultStreamOutputFactory::create(demux_t *demux, int format) const
{
    switch(format)
    {
        case StreamFormat::MP4:
            return new BaseStreamOutput(demux, "mp4");

        case StreamFormat::MPEG2TS:
            return new BaseStreamOutput(demux, "ts");

        default:
            throw VLC_EBADVAR;
            break;
    }
    return NULL;
}

