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
#include <vlc_meta.h>

#include "../mpeg/timestamps.h"

extern "C"
{
    #include "../../meta_engine/ID3Tag.h"
    #include "../../meta_engine/ID3Meta.h"
}

using namespace hls;

HLSStream::HLSStream(demux_t *demux)
    : AbstractStream(demux)
{
    b_id3_timestamps_offset_set = false;
    p_meta = vlc_meta_New();
    b_meta_updated = false;
}

HLSStream::~HLSStream()
{
    if(p_meta)
        vlc_meta_Delete(p_meta);
}

void HLSStream::setMetadataTimeOffset(vlc_tick_t i_offset)
{
    if(i_offset >= 0)
    {
        if(!b_id3_timestamps_offset_set)
            fakeEsOut()->setAssociatedTimestamp(i_offset);
        b_id3_timestamps_offset_set = true;
    }
    else
    {
        fakeEsOut()->setAssociatedTimestamp(-1);
        b_id3_timestamps_offset_set = false;
    }
}

bool HLSStream::setPosition(vlc_tick_t ts , bool b)
{
    bool ok = AbstractStream::setPosition(ts ,b);
    if(b && ok)
        b_id3_timestamps_offset_set = false;
    return ok;
}

int HLSStream::ParseID3PrivTag(const uint8_t *p_payload, size_t i_payload)
{
    if(i_payload == 53 &&
       !memcmp( p_payload, "com.apple.streaming.transportStreamTimestamp", 45))
    {
        setMetadataTimeOffset(FROM_SCALE_NZ(GetQWBE(&p_payload[45])));
    }
    return VLC_SUCCESS;
}

int HLSStream::ParseID3Tag(uint32_t i_tag, const uint8_t *p_payload, size_t i_payload)
{
    if(i_tag == VLC_FOURCC('P','R','I','V'))
        (void) ParseID3PrivTag(p_payload, i_payload);
    else
        (void) ID3HandleTag(p_payload, i_payload, i_tag, p_meta, &b_meta_updated);
    return VLC_SUCCESS;
}

int HLSStream::ID3TAG_Parse_Handler(uint32_t i_tag, const uint8_t *p_payload, size_t i_payload, void *p_priv)
{
    HLSStream *hlsstream = static_cast<HLSStream *>(p_priv);
    return hlsstream->ParseID3Tag(i_tag, p_payload, i_payload);
}

block_t * HLSStream::checkBlock(block_t *p_block, bool b_first)
{
    if(b_first && p_block &&
       p_block->i_buffer >= 10 && ID3TAG_IsTag(p_block->p_buffer, false))
    {
        while( p_block->i_buffer )
        {
            size_t i_size = ID3TAG_Parse( p_block->p_buffer, p_block->i_buffer,
                                          ID3TAG_Parse_Handler, static_cast<void *>(this) );
            /* Skip ID3 for demuxer */
            p_block->p_buffer += i_size;
            p_block->i_buffer -= i_size;
            if( i_size == 0 )
                break;
        }
    }

    if( b_meta_updated )
    {
        b_meta_updated = false;
        AbstractCommand *command = fakeEsOut()->commandsQueue()->factory()->createEsOutMetaCommand( -1, p_meta );
        if( command )
            fakeEsOut()->commandsQueue()->Schedule( command );
    }

    return p_block;
}

AbstractDemuxer *HLSStream::newDemux(vlc_object_t *p_obj, const StreamFormat &format,
                                     es_out_t *out, AbstractSourceStream *source) const
{
    AbstractDemuxer *ret = NULL;
    switch((unsigned)format)
    {
        case StreamFormat::PACKEDAAC:
            ret = new Demuxer(p_obj, "aac", out, source);
            break;

        case StreamFormat::MPEG2TS:
            ret = new Demuxer(p_obj, "ts", out, source);
            if(ret)
                ret->setBitstreamSwitchCompatible(false); /* HLS and unique PAT/PMT versions */
            break;

        case StreamFormat::MP4:
            ret = AbstractStream::newDemux(p_obj, format, out, source);
            break;

/* Disabled until we can handle empty segments/cue and absolute time
        case StreamFormat::WEBVTT:
            ret = new Demuxer(p_obj, "webvttstream", out, source);
            if(ret)
                ret->setRestartsOnEachSegment(true);
            break;
*/

        case StreamFormat::UNKNOWN:
            ret = new MimeDemuxer(p_obj, this, out, source);
            break;

        default:
        case StreamFormat::UNSUPPORTED:
            break;
    }
    return ret;
}

AbstractStream * HLSStreamFactory::create(demux_t *realdemux, const StreamFormat &format,
                               SegmentTracker *tracker, AbstractConnectionManager *manager) const
{
    HLSStream *stream = new (std::nothrow) HLSStream(realdemux);
    if(stream && !stream->init(format, tracker, manager))
    {
        delete stream;
        return NULL;
    }
    return stream;
}
