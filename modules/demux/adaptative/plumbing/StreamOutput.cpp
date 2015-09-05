/*
 * StreamOutput.cpp
 *****************************************************************************
 * Copyright (C) 2014-2015 VideoLAN and VLC authors
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
#include "StreamOutput.hpp"
#include <vlc_stream.h>
#include <vlc_demux.h>

using namespace adaptative;

AbstractStreamOutput::AbstractStreamOutput(demux_t *demux, const StreamFormat &format_)
{
    realdemux = demux;
    format = format_;
}

void AbstractStreamOutput::setLanguage(const std::string &lang)
{
    language = lang;
}

void AbstractStreamOutput::setDescription(const std::string &desc)
{
    description = desc;
}

const StreamFormat & AbstractStreamOutput::getStreamFormat() const
{
    return format;
}

AbstractStreamOutput::~AbstractStreamOutput()
{
}

BaseStreamOutput::BaseStreamOutput(demux_t *demux, const StreamFormat &format, const std::string &name) :
    AbstractStreamOutput(demux, format)
{
    this->name = name;
    seekable = true;
    demuxstream = NULL;

    CommandsFactory *factory = new CommandsFactory();

    fakeesout = new (std::nothrow) FakeESOut( realdemux->out, factory );
    if (!fakeesout)
    {
        delete factory;
        throw VLC_ENOMEM;
    }
    fakeesout->setExtraInfoProvider( this );

    demuxstream = stream_DemuxNew(realdemux, name.c_str(), fakeesout->getEsOut());
    if(!demuxstream)
        throw VLC_EGENERIC;
}

BaseStreamOutput::~BaseStreamOutput()
{
    if (demuxstream)
        stream_Delete(demuxstream);

    delete fakeesout;
}

mtime_t BaseStreamOutput::getPCR() const
{
    return fakeesout->commandsqueue.getBufferingLevel();
}

mtime_t BaseStreamOutput::getFirstDTS() const
{
    return fakeesout->commandsqueue.getFirstDTS();
}

int BaseStreamOutput::esCount() const
{
    return fakeesout->esCount();
}

void BaseStreamOutput::pushBlock(block_t *block, bool)
{
    stream_DemuxSend(demuxstream, block);
}

bool BaseStreamOutput::seekAble() const
{
    bool b_canswitch = switchAllowed();
    return (demuxstream && seekable && b_canswitch);
}

void BaseStreamOutput::setPosition(mtime_t nztime)
{
    if(reinitsOnSeek())
    {
        restart();
        fakeesout->commandsqueue.Abort( true );
        fakeesout->recycleAll();
    }

    es_out_Control(realdemux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                   VLC_TS_0 + nztime);
}

bool BaseStreamOutput::restart()
{
    stream_Delete(demuxstream);
    demuxstream = stream_DemuxNew(realdemux, name.c_str(), fakeesout->getEsOut());
    if(!demuxstream)
        return false;
    return true;
}

bool BaseStreamOutput::reinitsOnSeek() const
{
    return true;
}

bool BaseStreamOutput::switchAllowed() const
{
    return !fakeesout->restarting();
}

bool BaseStreamOutput::isSelected() const
{
    return fakeesout->hasSelectedEs();
}

bool BaseStreamOutput::isEmpty() const
{
    return fakeesout->commandsqueue.isEmpty();
}

void BaseStreamOutput::sendToDecoder(mtime_t nzdeadline)
{
    fakeesout->commandsqueue.Process( realdemux->out, VLC_TS_0 + nzdeadline );
}

void BaseStreamOutput::setTimestampOffset(mtime_t offset)
{
    fakeesout->setTimestampOffset( offset );
}

void BaseStreamOutput::fillExtraFMTInfo( es_format_t *p_fmt ) const
{
    if(!p_fmt->psz_language && !language.empty())
        p_fmt->psz_language = strdup(language.c_str());
    if(!p_fmt->psz_description && !description.empty())
        p_fmt->psz_description = strdup(description.c_str());
}

