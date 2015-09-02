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
#include "Streams.hpp"
#include "http/HTTPConnection.hpp"
#include "http/HTTPConnectionManager.h"
#include "logic/AbstractAdaptationLogic.h"
#include "playlist/SegmentChunk.hpp"
#include "plumbing/StreamOutput.hpp"
#include "SegmentTracker.hpp"
#include <vlc_demux.h>

using namespace adaptative;
using namespace adaptative::http;
using namespace adaptative::logic;

Stream::Stream(demux_t * demux_, const StreamFormat &format_)
{
    p_demux = demux_;
    type = UNKNOWN;
    format = format_;
    output = NULL;
    adaptationLogic = NULL;
    currentChunk = NULL;
    eof = false;
    disabled = false;
    segmentTracker = NULL;
    streamOutputFactory = NULL;
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
        mimetype = VIDEO;
    else if (!mime.compare(0, 6, "audio/"))
        mimetype = AUDIO;
    else if (!mime.compare(0, 12, "application/"))
        mimetype = APPLICATION;
    else /* unknown of unsupported */
        mimetype = UNKNOWN;
    return mimetype;
}

void Stream::create(AbstractAdaptationLogic *logic, SegmentTracker *tracker,
                    const AbstractStreamOutputFactory *factory)
{
    adaptationLogic = logic;
    segmentTracker = tracker;
    streamOutputFactory = factory;
    updateFormat(format);
}

void Stream::updateFormat(StreamFormat &newformat)
{
    if( format == newformat && output )
        return;

    delete output;
    format = newformat;
    output = streamOutputFactory->create(p_demux, format);
    if(!output)
        throw VLC_EGENERIC;
    output->setLanguage(language);
    output->setDescription(description);
}

void Stream::setLanguage(const std::string &lang)
{
    language = lang;
}

void Stream::setDescription(const std::string &desc)
{
    description = desc;
}

bool Stream::isEOF() const
{
    return false;
}

mtime_t Stream::getPCR() const
{
    if(!output)
        return 0;
    return output->getPCR();
}

mtime_t Stream::getFirstDTS() const
{
    if(!output)
        return 0;
    return output->getFirstDTS();
}

int Stream::esCount() const
{
    if(!output)
        return 0;
    return output->esCount();
}

bool Stream::operator ==(const Stream &stream) const
{
    return stream.type == type;
}

SegmentChunk * Stream::getChunk()
{
    if (currentChunk == NULL && output)
    {
        if(esCount() && !isSelected())
        {
            disabled = true;
            return NULL;
        }
        currentChunk = segmentTracker->getNextChunk(output->switchAllowed());
        if (currentChunk == NULL)
            eof = true;
    }
    return currentChunk;
}

bool Stream::seekAble() const
{
    return (output && output->seekAble());
}

bool Stream::isSelected() const
{
    return output && output->isSelected();
}

bool Stream::reactivate(mtime_t basetime)
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

bool Stream::isDisabled() const
{
    return disabled;
}

Stream::status Stream::demux(HTTPConnectionManager *connManager, mtime_t nz_deadline, bool send)
{
    if(!output)
        return Stream::status_eof;

    if(nz_deadline + VLC_TS_0 > output->getPCR()) /* not already demuxed */
    {
        /* need to read, demuxer still buffering, ... */
        if(read(connManager) <= 0)
        {
            if(output->isEmpty())
                return Stream::status_eof;
        }
        else if(nz_deadline + VLC_TS_0 > output->getPCR()) /* need to read more */
        {
            return Stream::status_buffering;
        }
    }

    if(send)
        output->sendToDecoder(nz_deadline);

    return Stream::status_demuxed;
}

size_t Stream::read(HTTPConnectionManager *connManager)
{
    SegmentChunk *chunk = getChunk();
    if(!chunk)
        return 0;

    if(!chunk->getConnection())
    {
       if(!connManager->connectChunk(chunk))
        return 0;
    }

    size_t readsize = 0;
    bool b_segment_head_chunk = false;

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
        b_segment_head_chunk = true;
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

        StreamFormat chunkStreamFormat = chunk->getStreamFormat();
        if(output && chunkStreamFormat != output->getStreamFormat())
        {
            msg_Info(p_demux, "Changing stream format");
            updateFormat(chunkStreamFormat);
        }

        if (chunk->getBytesToRead() == 0)
        {
            chunk->getConnection()->releaseChunk();
            currentChunk = NULL;
            delete chunk;
        }
    }

    readsize = block->i_buffer;

    if(output)
        output->pushBlock(block, b_segment_head_chunk);
    else
        block_Release(block);

    return readsize;
}

bool Stream::setPosition(mtime_t time, bool tryonly)
{
    if(!output)
        return false;

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
