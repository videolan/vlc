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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Demuxer.hpp"

#include <vlc_stream.h>
#include <vlc_demux.h>
#include "SourceStream.hpp"
#include "../StreamFormat.hpp"
#include "CommandsQueue.hpp"
#include "../ChunksSource.hpp"

using namespace adaptive;

AbstractDemuxer::AbstractDemuxer()
{
    b_startsfromzero = false;
    b_reinitsonseek = true;
    b_candetectswitches = true;
    b_alwaysrestarts = false;
}

AbstractDemuxer::~AbstractDemuxer()
{

}

bool AbstractDemuxer::alwaysStartsFromZero() const
{
    return b_startsfromzero;
}

bool AbstractDemuxer::needsRestartOnSwitch() const
{
    return !b_candetectswitches;
}

bool AbstractDemuxer::needsRestartOnEachSegment() const
{
    return b_alwaysrestarts;
}

void AbstractDemuxer::setCanDetectSwitches( bool b )
{
    b_candetectswitches = b;
}

void AbstractDemuxer::setRestartsOnEachSegment( bool b )
{
    b_alwaysrestarts = b;
}

bool AbstractDemuxer::needsRestartOnSeek() const
{
    return b_reinitsonseek;
}

MimeDemuxer::MimeDemuxer(demux_t *p_realdemux_,
                         const DemuxerFactoryInterface *factory_,
                         es_out_t *out, AbstractSourceStream *source)
    : AbstractDemuxer()
{
    p_es_out = out;
    factory = factory_;
    p_realdemux = p_realdemux_;
    demuxer = NULL;
    sourcestream = source;
}

MimeDemuxer::~MimeDemuxer()
{
    if( demuxer )
        delete demuxer;
}

bool MimeDemuxer::create()
{
    stream_t *p_newstream = sourcestream->makeStream();
    if(!p_newstream)
        return false;

    char *type = stream_ContentType(p_newstream);
    if(type)
    {
        demuxer = factory->newDemux( p_realdemux, StreamFormat(std::string(type)),
                                     p_es_out, sourcestream );
        free(type);
    }
    vlc_stream_Delete(p_newstream);

    if(!demuxer || !demuxer->create())
        return false;

    return true;
}

void MimeDemuxer::destroy()
{
    if(demuxer)
    {
        delete demuxer;
        demuxer = NULL;
    }
    sourcestream->Reset();
}

void MimeDemuxer::drain()
{
    if(demuxer)
        demuxer->drain();
}

int MimeDemuxer::demux(mtime_t t)
{
    if(!demuxer)
        return VLC_DEMUXER_EOF;
    return demuxer->demux(t);
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
        b_candetectswitches = false;
        b_startsfromzero = true;
    }
    else if(name == "aac")
    {
        b_candetectswitches = false;
    }
}

Demuxer::~Demuxer()
{
    if(p_demux)
        demux_Delete(p_demux);
}

bool Demuxer::create()
{
    stream_t *p_newstream = sourcestream->makeStream();
    if(!p_newstream)
        return false;

    p_demux = demux_New( VLC_OBJECT(p_realdemux), name.c_str(), "",
                         p_newstream, p_es_out );
    if(!p_demux)
    {
        vlc_stream_Delete(p_newstream);
        b_eof = true;
        return false;
    }
    else
    {
        b_eof = false;
    }

    return true;
}

void Demuxer::destroy()
{
    if(p_demux)
    {
        demux_Delete(p_demux);
        p_demux = NULL;
    }
    sourcestream->Reset();
}

void Demuxer::drain()
{
    while(p_demux && demux_Demux(p_demux) == VLC_DEMUXER_SUCCESS);
}

int Demuxer::demux(mtime_t)
{
    if(!p_demux || b_eof)
        return VLC_DEMUXER_EOF;
    int i_ret = demux_Demux(p_demux);
    if(i_ret != VLC_DEMUXER_SUCCESS)
        b_eof = true;
    return i_ret;
}

SlaveDemuxer::SlaveDemuxer(demux_t *p_realdemux, const std::string &name, es_out_t *out, AbstractSourceStream *source)
    : Demuxer(p_realdemux, name, out, source)
{
    length = VLC_TS_INVALID;
    b_reinitsonseek = false;
    b_startsfromzero = false;
}

SlaveDemuxer::~SlaveDemuxer()
{

}

bool SlaveDemuxer::create()
{
    if(Demuxer::create())
    {
        length = VLC_TS_INVALID;
        if(demux_Control(p_demux, DEMUX_GET_LENGTH, &length) != VLC_SUCCESS)
            b_eof = true;
        return true;
    }
    return false;
}

int SlaveDemuxer::demux(mtime_t nz_deadline)
{
    /* Always call with increment or buffering will get slow stuck */
    mtime_t i_next_demux_time = VLC_TS_0 + nz_deadline + CLOCK_FREQ / 4;
    if( demux_Control(p_demux, DEMUX_SET_NEXT_DEMUX_TIME, i_next_demux_time ) != VLC_SUCCESS )
    {
        b_eof = true;
        return VLC_DEMUXER_EOF;
    }
    int ret = Demuxer::demux(i_next_demux_time);
    es_out_Control(p_es_out, ES_OUT_SET_GROUP_PCR, 0, i_next_demux_time);
    return ret;
}
