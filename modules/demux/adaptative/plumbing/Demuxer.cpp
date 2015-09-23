/*
 * Demuxer.cpp
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC Authors
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
#include "Demuxer.hpp"
#include <vlc_stream.h>

using namespace adaptative;

AbstractDemuxer::AbstractDemuxer()
{
    b_startsfromzero = false;
    b_reinitsonseek =true;
}

AbstractDemuxer::~AbstractDemuxer()
{

}

bool AbstractDemuxer::alwaysStartsFromZero() const
{
    return b_startsfromzero;
}

bool AbstractDemuxer::reinitsOnSeek() const
{
    return b_reinitsonseek;
}

StreamDemux::StreamDemux(demux_t *p_realdemux_, const std::string &name_, es_out_t *out)
    : AbstractDemuxer()
{
    demuxstream = NULL;
    p_es_out = out;
    name = name_;
    p_realdemux = p_realdemux_;

    if(name == "mp4")
    {
        b_startsfromzero = true;
    }

    restart();

    if(!demuxstream)
        throw VLC_EGENERIC;
}

StreamDemux::~StreamDemux()
{
    if (demuxstream)
        stream_Delete(demuxstream);
}

bool StreamDemux::feed(block_t *p_block, bool)
{
    stream_DemuxSend(demuxstream, p_block);
    return true;
}

bool StreamDemux::restart()
{
    if(demuxstream)
        stream_Delete(demuxstream);

    demuxstream = stream_DemuxNew(p_realdemux, name.c_str(), p_es_out);
    if(!demuxstream)
        return false;
    return true;
}
