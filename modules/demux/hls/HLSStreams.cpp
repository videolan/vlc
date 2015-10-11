/*
 * HLSStreams.cpp
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
#include "HLSStreams.hpp"
#include "HLSStreamFormat.hpp"
#include <vlc_demux.h>

using namespace hls;

HLSStream::HLSStream(demux_t *demux, const StreamFormat &format)
    :AbstractStream(demux, format)
{
    b_timestamps_offset_set = false;
    i_aac_offset = 0;
}

bool HLSStream::setPosition(mtime_t time, bool tryonly)
{
    bool b_ret = AbstractStream::setPosition(time, tryonly);
    if(!tryonly && b_ret)
    {
        /* Should be correct, has a restarted demux shouldn't have been fed with data yet */
        fakeesout->setTimestampOffset( VLC_TS_INVALID );
        b_timestamps_offset_set = false;
    }
    return b_ret;
}

bool HLSStream::restartDemux()
{
    bool b_ret = AbstractStream::restartDemux();
    if(b_ret)
        b_timestamps_offset_set = false;
    return b_ret;
}

AbstractDemuxer * HLSStream::createDemux(const StreamFormat &format)
{
    AbstractDemuxer *ret = NULL;
    switch((unsigned)format)
    {
        case HLSStreamFormat::UNKNOWN:
            ret = new Demuxer(p_realdemux, "any", fakeesout->getEsOut(), demuxersource);
            break;

        case HLSStreamFormat::PACKEDAAC:
            ret = new Demuxer(p_realdemux, "avformat", fakeesout->getEsOut(), demuxersource);
            break;

        case HLSStreamFormat::MPEG2TS:
            ret = new Demuxer(p_realdemux, "ts", fakeesout->getEsOut(), demuxersource);
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

void HLSStream::prepareFormatChange()
{
    AbstractStream::prepareFormatChange();
    if((unsigned)format == HLSStreamFormat::PACKEDAAC)
    {
        fakeesout->setTimestampOffset( i_aac_offset );
    }
    else
    {
        fakeesout->setTimestampOffset( 0 );
    }
}

block_t * HLSStream::checkBlock(block_t *p_block, bool b_first)
{
    if(b_first && p_block &&
       p_block->i_buffer >= 10 && !memcmp(p_block->p_buffer, "ID3", 3))
    {
        uint32_t size = GetDWBE(&p_block->p_buffer[6]) + 10;
        size = __MIN(p_block->i_buffer, size);
        if(size >= 73 && !b_timestamps_offset_set)
        {
            if(!memcmp(&p_block->p_buffer[10], "PRIV", 4) &&
               !memcmp(&p_block->p_buffer[20], "com.apple.streaming.transportStreamTimestamp", 45))
            {
                i_aac_offset = GetQWBE(&p_block->p_buffer[65]) * 100 / 9;
                b_timestamps_offset_set = true;
            }
        }

        /* Skip ID3 for demuxer */
        p_block->p_buffer += size;
        p_block->i_buffer -= size;
    }

    return p_block;
}

AbstractStream * HLSStreamFactory::create(demux_t *realdemux, const StreamFormat &format,
                               AbstractAdaptationLogic *logic, SegmentTracker *tracker,
                               HTTPConnectionManager *manager) const
{
    HLSStream *stream;
    try
    {
        stream = new HLSStream(realdemux, format);
    } catch (int) {
        return NULL;
    }
    stream->bind(logic, tracker, manager);
    return stream;
}
