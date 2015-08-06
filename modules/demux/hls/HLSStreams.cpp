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

AbstractStreamOutput *HLSStreamOutputFactory::create(demux_t *demux, const StreamFormat &format) const
{
    unsigned fmt = format;
    switch(fmt)
    {
        case HLSStreamFormat::PACKEDAAC:
            return new HLSPackedStreamOutput(demux, format, "any");
            break;

        default:
        case HLSStreamFormat::UNKNOWN:
        case HLSStreamFormat::MPEG2TS:
            return new BaseStreamOutput(demux, format, "ts");
    }
    return NULL;
}

HLSPackedStreamOutput::HLSPackedStreamOutput(demux_t *demux, const StreamFormat &format, const std::string &name) :
    BaseStreamOutput(demux, format, name)
{

}

void HLSPackedStreamOutput::pushBlock(block_t *p_block, bool b_first)
{
    if(b_first && p_block && p_block->i_buffer >= 10 && !memcmp(p_block->p_buffer, "ID3", 3))
    {
        uint32_t size = GetDWBE(&p_block->p_buffer[6]) + 10;
        size = __MIN(p_block->i_buffer, size);
        if(size >= 73 && timestamps_offset == VLC_TS_INVALID)
        {
            if(!memcmp(&p_block->p_buffer[10], "PRIV", 4) &&
               !memcmp(&p_block->p_buffer[20], "com.apple.streaming.transportStreamTimestamp", 45))
            {
                setTimestampOffset( GetQWBE(&p_block->p_buffer[65]) * 100 / 9 );
            }
        }

        /* Skip ID3 for demuxer */
        p_block->p_buffer += size;
        p_block->i_buffer -= size;
    }

    BaseStreamOutput::pushBlock(p_block, b_first);
}

void HLSPackedStreamOutput::setPosition(mtime_t nztime)
{
    BaseStreamOutput::setPosition(nztime);
    /* Should be correct, has a restarted demux shouldn't have been fed with data yet */
    setTimestampOffset(VLC_TS_INVALID - VLC_TS_0);
}
