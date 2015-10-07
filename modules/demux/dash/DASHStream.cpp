/*
 * DASHStream.cpp
 *****************************************************************************
 * Copyright (C) 2015 - VideoLAN and VLC authors
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
#include "DASHStream.hpp"
#include "DASHStreamFormat.hpp"

using namespace dash;

DASHStream::DASHStream(demux_t *demux, const StreamFormat &format)
    :AbstractStream(demux, format)
{
}

block_t * DASHStream::checkBlock(block_t *p_block, bool)
{
    return p_block;
}

AbstractDemuxer * DASHStream::createDemux(const StreamFormat &format)
{
    AbstractDemuxer *ret = NULL;
    switch((unsigned)format)
    {
        case DASHStreamFormat::MP4:
            ret = new Demuxer(p_realdemux, "mp4", fakeesout->getEsOut(), demuxersource);
            break;

        case DASHStreamFormat::MPEG2TS:
            ret = new Demuxer(p_realdemux, "ts", fakeesout->getEsOut(), demuxersource);
            break;

        case DASHStreamFormat::WEBVTT:
            ret = new SlaveDemuxer(p_realdemux, "subtitle", fakeesout->getEsOut(), demuxersource);
            break;

        case DASHStreamFormat::TTML:
            ret = new SlaveDemuxer(p_realdemux, "ttml", fakeesout->getEsOut(), demuxersource);
            break;

        default:
        case StreamFormat::UNSUPPORTED:
            break;
    }

    if(ret && !ret->create())
    {
        delete ret;
        ret = NULL;
    }
    else fakeesout->commandsqueue.Commit();

    return ret;
}

AbstractStream * DASHStreamFactory::create(demux_t *realdemux, const StreamFormat &format,
                                   AbstractAdaptationLogic *logic, SegmentTracker *tracker,
                                   HTTPConnectionManager *manager) const
{
    AbstractStream *stream;
    try
    {
        stream = new DASHStream(realdemux, format);
    } catch (int) {
        return NULL;
    }
    stream->bind(logic, tracker, manager);
    return stream;
}
