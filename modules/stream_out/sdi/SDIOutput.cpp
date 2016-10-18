/*****************************************************************************
 * SDIOutput.cpp: SDI sout module for vlc
 *****************************************************************************
 * Copyright © 2018 VideoLabs, VideoLAN and VideoLAN Authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#include "SDIOutput.hpp"
#include "SDIStream.hpp"
#include "sdiout.hpp"

#include <vlc_sout.h>
#include <vlc_picture.h>

using namespace sdi_sout;

SDIOutput::SDIOutput(sout_stream_t *p_stream_)
{
    p_stream = p_stream_;
    p_stream->pf_add     = SoutCallback_Add;
    p_stream->pf_del     = SoutCallback_Del;
    p_stream->pf_send    = SoutCallback_Send;
    p_stream->pf_flush   = SoutCallback_Flush;
    p_stream->pf_control = SoutCallback_Control;
    p_stream->pace_nocontrol = true;

    es_format_Init(&video.configuredfmt, VIDEO_ES, 0);
    es_format_Init(&audio.configuredfmt, AUDIO_ES, 0);
    video.tenbits = var_InheritBool(p_stream, CFG_PREFIX "tenbits");
    video.nosignal_delay = var_InheritInteger(p_stream, CFG_PREFIX "nosignal-delay");
    video.pic_nosignal = NULL;
    audio.i_channels = var_InheritInteger(p_stream, CFG_PREFIX "channels");;
    ancillary.afd = var_InheritInteger(p_stream, CFG_PREFIX "afd");
    ancillary.ar = var_InheritInteger(p_stream, CFG_PREFIX "ar");
    ancillary.afd_line = var_InheritInteger(p_stream, CFG_PREFIX "afd-line");
    videoStream = NULL;
    audioStream = NULL;
}

SDIOutput::~SDIOutput()
{
    videoBuffer.FlushQueued();
    audioBuffer.FlushQueued();
    if(video.pic_nosignal)
        picture_Release(video.pic_nosignal);
    es_format_Clean(&video.configuredfmt);
    es_format_Clean(&audio.configuredfmt);
}

AbstractStream *SDIOutput::Add(const es_format_t *fmt)
{
    AbstractStream *s = NULL;
    StreamID id(fmt->i_id);
    if(fmt->i_cat == VIDEO_ES && !videoStream)
    {
        if(ConfigureVideo(&fmt->video) == VLC_SUCCESS)
            s = videoStream = dynamic_cast<VideoDecodedStream *>(createStream(id, fmt, &videoBuffer));
        if(videoStream)
            videoStream->setOutputFormat(&video.configuredfmt);
    }
    else if(fmt->i_cat == AUDIO_ES && audio.i_channels && !audioStream)
    {
        if(ConfigureAudio(&fmt->audio) == VLC_SUCCESS)
            s = audioStream = dynamic_cast<AudioDecodedStream *>(createStream(id, fmt, &audioBuffer));
        if(audioStream)
            audioStream->setOutputFormat(&audio.configuredfmt);
    }
    return s;
}

int SDIOutput::Send(AbstractStream *id, block_t *p)
{
    int ret = id->Send(p);
    Process();
    return ret;
}

void SDIOutput::Del(AbstractStream *s)
{
    s->Drain();
    Process();
    if(videoStream == s)
        videoStream = NULL;
    else if(audioStream == s)
        audioStream = NULL;
    delete s;
}

int SDIOutput::Control(int, va_list)
{
    return VLC_EGENERIC;
}

AbstractStream *SDIOutput::createStream(const StreamID &id,
                                        const es_format_t *fmt,
                                        AbstractStreamOutputBuffer *buffer)
{
    AbstractStream *s;
    if(fmt->i_cat == VIDEO_ES)
        s = new VideoDecodedStream(VLC_OBJECT(p_stream), id, buffer);
    else if(fmt->i_cat == AUDIO_ES)
        s = new AudioDecodedStream(VLC_OBJECT(p_stream), id, buffer);
    else
        s = NULL;

     if(s && !s->init(fmt))
     {
         delete s;
         return NULL;
     }
     return s;
}

void *SDIOutput::SoutCallback_Add(sout_stream_t *p_stream, const es_format_t *fmt)
{
    SDIOutput *me = reinterpret_cast<SDIOutput *>(p_stream->p_sys);
    return me->Add(fmt);
}

void SDIOutput::SoutCallback_Del(sout_stream_t *p_stream, void *id)
{
    SDIOutput *me = reinterpret_cast<SDIOutput *>(p_stream->p_sys);
    me->Del(reinterpret_cast<AbstractStream *>(id));
}

int SDIOutput::SoutCallback_Send(sout_stream_t *p_stream, void *id, block_t *p_block)
{
    SDIOutput *me = reinterpret_cast<SDIOutput *>(p_stream->p_sys);
    return me->Send(reinterpret_cast<AbstractStream *>(id), p_block);
}

int SDIOutput::SoutCallback_Control(sout_stream_t *p_stream, int query, va_list args)
{
    SDIOutput *me = reinterpret_cast<SDIOutput *>(p_stream->p_sys);
    return me->Control(query, args);
}

void SDIOutput::SoutCallback_Flush(sout_stream_t *, void *id)
{
    reinterpret_cast<AbstractStream *>(id)->Flush();
}
