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
#include <vlc_demux.h>
#include "SourceStream.hpp"
#include "CommandsQueue.hpp"
#include "../ChunksSource.hpp"

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

Demuxer::Demuxer(demux_t *p_realdemux_, const std::string &name_, es_out_t *out, AbstractSourceStream *source)
    : AbstractDemuxer()
{
    p_es_out = out;
    name = name_;
    p_realdemux = p_realdemux_;
    p_demux = NULL;
    b_eof = false;
    sourcestream = source;

    if(name == "mp4")
    {
        b_startsfromzero = true;
    }

    if(!create())
        throw VLC_EGENERIC;
}

Demuxer::~Demuxer()
{
    if(p_demux)
    {
        p_demux->s = NULL; // otherwise tries to delete below inner stream
        demux_Delete(p_demux);
    }
}

bool Demuxer::create()
{
    p_demux = demux_New( VLC_OBJECT(p_realdemux), name.c_str(), "",
                         sourcestream->getStream(), p_es_out );
    if(!p_demux)
    {
        b_eof = true;
        return false;
    }
    return true;
}

bool Demuxer::restart(CommandsQueue &queue)
{
    if(p_demux)
    {
        queue.setDrop(true);
        p_demux->s = NULL; // otherwise tries to delete below inner stream
        demux_Delete(p_demux);
        p_demux = NULL;
        queue.setDrop(false);
    }
    sourcestream->Reset();
    return create();
}

void Demuxer::drain()
{
    while(p_demux && demux_Demux(p_demux) == VLC_DEMUXER_SUCCESS);
}

int Demuxer::demux()
{
    if(b_eof)
        return VLC_DEMUXER_EOF;
    int i_ret = demux_Demux(p_demux);
    if(i_ret != VLC_DEMUXER_SUCCESS)
        b_eof = true;
    return i_ret;
}
