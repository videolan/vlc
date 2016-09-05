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

extern "C"
{
    #include "../meta_engine/ID3Tag.h"
}

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

int HLSStream::ID3PrivTagHandler(const uint8_t *p_payload, size_t i_payload)
{
    if(i_payload == 53 &&
       !memcmp( p_payload, "com.apple.streaming.transportStreamTimestamp", 45))
    {
        i_aac_offset = GetQWBE(&p_payload[45]) * 100 / 9;
        b_timestamps_offset_set = true;
        return VLC_EGENERIC; /* stop parsing */
    }
    return VLC_SUCCESS;
}

static int ID3TAG_Parse_Handler(uint32_t i_tag, const uint8_t *p_payload, size_t i_payload, void *p_priv)
{
    HLSStream *hlsstream = static_cast<HLSStream *>(p_priv);

    if(i_tag == VLC_FOURCC('P', 'R', 'I', 'V'))
        return hlsstream->ID3PrivTagHandler(p_payload, i_payload);

    return VLC_SUCCESS;
}

block_t * HLSStream::checkBlock(block_t *p_block, bool b_first)
{
    if(b_first && p_block &&
       p_block->i_buffer >= 10 && ID3TAG_IsTag(p_block->p_buffer, false))
    {
        size_t i_size = ID3TAG_Parse( p_block->p_buffer, p_block->i_buffer,
                                      ID3TAG_Parse_Handler, static_cast<void *>(this) );

        /* Skip ID3 for demuxer */
        p_block->p_buffer += i_size;
        p_block->i_buffer -= i_size;
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
