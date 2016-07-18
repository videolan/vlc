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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "HLSStreams.hpp"
#include <vlc_demux.h>

using namespace hls;

HLSStream::HLSStream(demux_t *demux)
    : AbstractStream(demux)
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

        case StreamFormat::PACKEDAAC:
            ret = new Demuxer(p_realdemux, "avformat", fakeesout->getEsOut(), demuxersource);
            break;

        case StreamFormat::MPEG2TS:
            ret = new Demuxer(p_realdemux, "ts", fakeesout->getEsOut(), demuxersource);
            if(ret)
                ret->setCanDetectSwitches(false);
            break;

        case StreamFormat::MP4:
            ret = new Demuxer(p_realdemux, "mp4", fakeesout->getEsOut(), demuxersource);
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
    else commandsqueue->Commit();

    return ret;
}

void HLSStream::prepareRestart(bool b_discontinuity)
{
    AbstractStream::prepareRestart(b_discontinuity);
    if((unsigned)format == StreamFormat::PACKEDAAC)
    {
        fakeesout->setTimestampOffset( i_aac_offset );
    }
    else
    {
        fakeesout->setTimestampOffset( 0 );
    }
}

static uint32_t ReadID3Size(const uint8_t *p_buffer)
{
    return ( (uint32_t)p_buffer[3] & 0x7F ) |
          (( (uint32_t)p_buffer[2] & 0x7F ) << 7) |
          (( (uint32_t)p_buffer[1] & 0x7F ) << 14) |
          (( (uint32_t)p_buffer[0] & 0x7F ) << 21);
}

static bool IsID3Tag(const uint8_t *p_buffer, bool b_footer)
{
    return( memcmp(p_buffer, (b_footer) ? "3DI" : "ID3", 3) == 0 &&
            p_buffer[3] < 0xFF &&
            p_buffer[4] < 0xFF &&
           ((GetDWBE(&p_buffer[6]) & 0x80808080) == 0) );
}

block_t * HLSStream::checkBlock(block_t *p_block, bool b_first)
{
    if(b_first && p_block &&
       p_block->i_buffer >= 10 && IsID3Tag(p_block->p_buffer, false))
    {
        uint32_t size = ReadID3Size(&p_block->p_buffer[6]);
        size = __MIN(p_block->i_buffer, size + 10);
        const uint8_t *p_frame = &p_block->p_buffer[10];
        uint32_t i_left = (size >= 10) ? size - 10 : 0;
        while(i_left >= 10 && !b_timestamps_offset_set)
        {
            uint32_t i_framesize = ReadID3Size(&p_frame[4]) + 10;
            if( i_framesize > i_left )
                break;
            if(i_framesize == 63 && !memcmp(p_frame, "PRIV", 4))
            {
                if(!memcmp(&p_frame[10], "com.apple.streaming.transportStreamTimestamp", 45))
                {
                    i_aac_offset = GetQWBE(&p_frame[55]) * 100 / 9;
                    b_timestamps_offset_set = true;
                }
            }
            i_left -= i_framesize;
            p_frame += i_framesize;
        }

        /* Skip ID3 for demuxer */
        p_block->p_buffer += size;
        p_block->i_buffer -= size;

        /* Skip ID3 footer */
        if(p_block->i_buffer >= 10 && IsID3Tag(p_block->p_buffer, true))
        {
            p_block->p_buffer += 10;
            p_block->i_buffer -= 10;
        }
    }

    return p_block;
}

AbstractStream * HLSStreamFactory::create(demux_t *realdemux, const StreamFormat &,
                               SegmentTracker *tracker, HTTPConnectionManager *manager) const
{
    HLSStream *stream = new (std::nothrow) HLSStream(realdemux);
    if(stream && !stream->init(StreamFormat(StreamFormat::UNKNOWN), tracker, manager))
    {
        delete stream;
        return NULL;
    }
    return stream;
}
