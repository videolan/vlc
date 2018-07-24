/*****************************************************************************
 * SDIStream.cpp: SDI sout module for vlc
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

#include "SDIStream.hpp"
#include "sdiout.hpp"

#include <vlc_modules.h>
#include <vlc_meta.h>
#include <vlc_block.h>

#include <sstream>

using namespace sdi_sout;

AbstractStreamOutputBuffer::AbstractStreamOutputBuffer()
{
}

AbstractStreamOutputBuffer::~AbstractStreamOutputBuffer()
{
}

void AbstractStreamOutputBuffer::Enqueue(void *p)
{
    queue_mutex.lock();
    queued.push(p);
    queue_mutex.unlock();
}

void *AbstractStreamOutputBuffer::Dequeue()
{
    void *p = NULL;
    queue_mutex.lock();
    if(!queued.empty())
    {
        p = queued.front();
        queued.pop();
    }
    queue_mutex.unlock();
    return p;
}

BlockStreamOutputBuffer::BlockStreamOutputBuffer()
    : AbstractStreamOutputBuffer()
{

}

BlockStreamOutputBuffer::~BlockStreamOutputBuffer()
{

}

void BlockStreamOutputBuffer::FlushQueued()
{
    block_t *p;
    while((p = reinterpret_cast<block_t *>(Dequeue())))
        block_Release(p);
}


PictureStreamOutputBuffer::PictureStreamOutputBuffer()
    : AbstractStreamOutputBuffer()
{

}

PictureStreamOutputBuffer::~PictureStreamOutputBuffer()
{

}

void PictureStreamOutputBuffer::FlushQueued()
{
    picture_t *p;
    while((p = reinterpret_cast<picture_t *>(Dequeue())))
        picture_Release(p);
}

unsigned StreamID::i_next_sequence_id = 0;

StreamID::StreamID(int i_stream_id)
{
    stream_id = i_stream_id;
    sequence_id = i_next_sequence_id++;
}

StreamID::StreamID(int i_stream_id, int i_sequence)
{
    stream_id = i_stream_id;
    sequence_id = i_sequence;
}

std::string StreamID::toString() const
{
    std::stringstream ss;
    ss.imbue(std::locale("C"));
    ss << "Stream(";
    if(stream_id > -1)
        ss << "id #" << stream_id << ", ";
    ss << "seq " << sequence_id << ")";
    return ss.str();
}

StreamID& StreamID::operator=(const StreamID &other)
{
    stream_id = other.stream_id;
    sequence_id = other.sequence_id;
    return *this;
}

bool StreamID::operator==(const StreamID &other)
{
    if(stream_id == -1 || other.stream_id == -1)
        return sequence_id == other.sequence_id;
    else
        return stream_id == other.stream_id;
}

AbstractStream::AbstractStream(vlc_object_t *p_obj,
                               const StreamID &id,
                               AbstractStreamOutputBuffer *buffer)
    : id(id)
{
    p_stream = p_obj;
    outputbuffer = buffer;
}

AbstractStream::~AbstractStream()
{

}

const StreamID & AbstractStream::getID() const
{
    return id;
}

struct decoder_owner
{
    decoder_t dec;
    AbstractDecodedStream *id;
    bool b_error;
    es_format_t last_fmt_update;
    es_format_t decoder_out;
};

AbstractDecodedStream::AbstractDecodedStream(vlc_object_t *p_obj,
                                             const StreamID &id,
                                             AbstractStreamOutputBuffer *buffer)
    : AbstractStream(p_obj, id, buffer)
{
    p_decoder = NULL;
    es_format_Init(&requestedoutput, 0, 0);
}

AbstractDecodedStream::~AbstractDecodedStream()
{
    es_format_Clean(&requestedoutput);

    if(!p_decoder)
        return;

    struct decoder_owner *p_owner;
    p_owner = container_of(p_decoder, struct decoder_owner, dec);
    if(p_decoder->p_module)
        module_unneed(p_decoder, p_decoder->p_module);
    es_format_Clean(&p_owner->dec.fmt_in);
    es_format_Clean(&p_owner->dec.fmt_out);
    es_format_Clean(&p_owner->decoder_out);
    es_format_Clean(&p_owner->last_fmt_update);
    if(p_decoder->p_description)
        vlc_meta_Delete(p_decoder->p_description);
    vlc_object_release(p_decoder);
}

bool AbstractDecodedStream::init(const es_format_t *p_fmt)
{
    const char *category;
    if(p_fmt->i_cat == VIDEO_ES)
        category = "video decoder";
    else if(p_fmt->i_cat == AUDIO_ES)
        category = "audio decoder";
    else
        return false;

    /* Create decoder object */
    struct decoder_owner * p_owner =
            reinterpret_cast<struct decoder_owner *>(
                vlc_object_create(p_stream, sizeof(*p_owner)));
    if(!p_owner)
        return false;

    es_format_Init(&p_owner->decoder_out, p_fmt->i_cat, 0);
    es_format_Init(&p_owner->last_fmt_update, p_fmt->i_cat, 0);
    p_owner->b_error = false;
    p_owner->id = this;

    p_decoder = &p_owner->dec;
    p_decoder->p_module = NULL;
    es_format_Init(&p_decoder->fmt_out, p_fmt->i_cat, 0);
    es_format_Copy(&p_decoder->fmt_in, p_fmt);
    p_decoder->b_frame_drop_allowed = false;

    setCallbacks();

    p_decoder->pf_decode = NULL;
    p_decoder->pf_get_cc = NULL;

    p_decoder->p_module = module_need_var(p_decoder, category, "codec");
    if(!p_decoder->p_module)
    {
        msg_Err(p_stream, "cannot find %s for %4.4s", category, (char *)&p_fmt->i_codec);
        es_format_Clean(&p_decoder->fmt_in);
        es_format_Clean(&p_decoder->fmt_out);
        es_format_Clean(&p_owner->decoder_out);
        es_format_Clean(&p_owner->last_fmt_update);
        vlc_object_release(p_decoder);
        p_decoder = NULL;
        return false;
    }

    return true;
}

int AbstractDecodedStream::Send(block_t *p_block)
{
    assert(p_decoder);

    struct decoder_owner *p_owner =
            container_of(p_decoder, struct decoder_owner, dec);

     if(!p_owner->b_error)
    {
        int ret = p_decoder->pf_decode(p_decoder, p_block);
        switch(ret)
        {
            case VLCDEC_SUCCESS:
                break;
            case VLCDEC_ECRITICAL:
                p_owner->b_error = true;
                break;
            case VLCDEC_RELOAD:
                p_owner->b_error = true;
                if(p_block)
                    block_Release(p_block);
                break;
            default:
                vlc_assert_unreachable();
        }
    }

    return p_owner->b_error ? VLC_EGENERIC : VLC_SUCCESS;
}

void AbstractDecodedStream::Flush()
{
}

void AbstractDecodedStream::Drain()
{
    Send(NULL);
}

void AbstractDecodedStream::setOutputFormat(const es_format_t *p_fmt)
{
    es_format_Clean(&requestedoutput);
    es_format_Copy(&requestedoutput, p_fmt);
}

VideoDecodedStream::VideoDecodedStream(vlc_object_t *p_obj,
                                       const StreamID &id,
                                       AbstractStreamOutputBuffer *buffer)
    :AbstractDecodedStream(p_obj, id, buffer)
{
    p_filters_chain = NULL;
}

VideoDecodedStream::~VideoDecodedStream()
{
    if(p_filters_chain)
        filter_chain_Delete(p_filters_chain);
}

void VideoDecodedStream::setCallbacks()
{
    static struct decoder_owner_callbacks dec_cbs;
    memset(&dec_cbs, 0, sizeof(dec_cbs));
    dec_cbs.video.format_update = VideoDecCallback_update_format;
    dec_cbs.video.buffer_new = VideoDecCallback_new_buffer;
    dec_cbs.video.queue = VideoDecCallback_queue;
    dec_cbs.video.queue_cc = VideoDecCallback_queue_cc;

    p_decoder->cbs = &dec_cbs;
}

void VideoDecodedStream::setCaptionsOutputBuffer(AbstractStreamOutputBuffer *buf)
{
    captionsOutputBuffer = buf;
}

void VideoDecodedStream::VideoDecCallback_queue(decoder_t *p_dec, picture_t *p_pic)
{
    struct decoder_owner *p_owner;
    p_owner = container_of(p_dec, struct decoder_owner, dec);
    static_cast<VideoDecodedStream *>(p_owner->id)->Output(p_pic);
}

void VideoDecodedStream::VideoDecCallback_queue_cc(decoder_t *p_dec, block_t *p_block,
                                                   const decoder_cc_desc_t *)
{
    struct decoder_owner *p_owner;
    p_owner = container_of(p_dec, struct decoder_owner, dec);
    static_cast<VideoDecodedStream *>(p_owner->id)->QueueCC(p_block);
}

int VideoDecodedStream::VideoDecCallback_update_format(decoder_t *p_dec)
{
    struct decoder_owner *p_owner;
    p_owner = container_of(p_dec, struct decoder_owner, dec);

    /* fixup */
    p_dec->fmt_out.video.i_chroma = p_dec->fmt_out.i_codec;

    es_format_Clean(&p_owner->last_fmt_update);
    es_format_Copy(&p_owner->last_fmt_update, &p_dec->fmt_out);

    return VLC_SUCCESS;
}

picture_t *VideoDecodedStream::VideoDecCallback_new_buffer(decoder_t *p_dec)
{
    return picture_NewFromFormat(&p_dec->fmt_out.video);
}


static picture_t *transcode_video_filter_buffer_new(filter_t *p_filter)
{
    p_filter->fmt_out.video.i_chroma = p_filter->fmt_out.i_codec;
    return picture_NewFromFormat(&p_filter->fmt_out.video);
}

static const struct filter_video_callbacks transcode_filter_video_cbs =
{
    .buffer_new = transcode_video_filter_buffer_new,
};

filter_chain_t * VideoDecodedStream::VideoFilterCreate(const es_format_t *p_srcfmt)
{
    filter_chain_t *p_chain;
    filter_owner_t owner;
    memset(&owner, 0, sizeof(owner));
    owner.video = &transcode_filter_video_cbs;

    p_chain = filter_chain_NewVideo(p_stream, false, &owner);
    if(!p_chain)
        return NULL;
    filter_chain_Reset(p_chain, p_srcfmt, &requestedoutput);

    if(p_srcfmt->video.i_chroma != requestedoutput.video.i_chroma)
    {
        if(filter_chain_AppendConverter(p_chain, p_srcfmt, &requestedoutput) != VLC_SUCCESS)
        {
            filter_chain_Delete(p_chain);
            return NULL;
        }
    }

    const es_format_t *p_fmt_out = filter_chain_GetFmtOut(p_chain);
    if(!es_format_IsSimilar(&requestedoutput, p_fmt_out))
    {
        filter_chain_Delete(p_chain);
        return NULL;
    }

    return p_chain;
}

void VideoDecodedStream::Output(picture_t *p_pic)
{
    struct decoder_owner *p_owner;
    p_owner = container_of(p_decoder, struct decoder_owner, dec);

    if(!es_format_IsSimilar(&p_owner->last_fmt_update, &p_owner->decoder_out))
    {

        msg_Dbg(p_stream, "decoder output format now %4.4s",
                (char*)&p_owner->last_fmt_update.i_codec);

        if(p_filters_chain)
            filter_chain_Delete(p_filters_chain);
        p_filters_chain = VideoFilterCreate(&p_owner->last_fmt_update);
        if(!p_filters_chain)
        {
            picture_Release(p_pic);
            return;
        }

        es_format_Clean(&p_owner->decoder_out);
        es_format_Copy(&p_owner->decoder_out, &p_owner->last_fmt_update);
    }

    if(p_filters_chain)
        p_pic = filter_chain_VideoFilter(p_filters_chain, p_pic);

    if(p_pic)
        outputbuffer->Enqueue(p_pic);
}

void VideoDecodedStream::QueueCC(block_t *p_block)
{
    captionsOutputBuffer->Enqueue(p_block);
}

AudioDecodedStream::AudioDecodedStream(vlc_object_t *p_obj,
                                       const StreamID &id,
                                       AbstractStreamOutputBuffer *buffer)
    :AbstractDecodedStream(p_obj, id, buffer)
{
    p_filters = NULL;
}

AudioDecodedStream::~AudioDecodedStream()
{
    if(p_filters)
        aout_FiltersDelete(p_stream, p_filters);
}

void AudioDecodedStream::AudioDecCallback_queue(decoder_t *p_dec, block_t *p_block)
{
    struct decoder_owner *p_owner;
    p_owner = container_of(p_dec, struct decoder_owner, dec);
    static_cast<AudioDecodedStream *>(p_owner->id)->Output(p_block);
}

void AudioDecodedStream::Output(block_t *p_block)
{
    struct decoder_owner *p_owner;
    p_owner = container_of(p_decoder, struct decoder_owner, dec);

    if(!es_format_IsSimilar(&p_owner->last_fmt_update, &p_owner->decoder_out))
    {
        msg_Dbg(p_stream, "decoder output format now %4.4s %u channels",
                (char*)&p_owner->last_fmt_update.i_codec,
                p_owner->last_fmt_update.audio.i_channels);

        if(p_filters)
            aout_FiltersDelete(p_stream, p_filters);
        p_filters = AudioFiltersCreate(&p_owner->last_fmt_update);
        if(!p_filters)
        {
            msg_Err(p_stream, "filter creation failed");
            block_Release(p_block);
            return;
        }

        es_format_Clean(&p_owner->decoder_out);
        es_format_Copy(&p_owner->decoder_out, &p_owner->last_fmt_update);
    }

    /* Run filter chain */
    if(p_filters)
        p_block = aout_FiltersPlay(p_filters, p_block, 1.f);

    if(p_block && !p_block->i_nb_samples &&
       p_owner->last_fmt_update.audio.i_bytes_per_frame )
    {
        p_block->i_nb_samples = p_block->i_buffer /
                p_owner->last_fmt_update.audio.i_bytes_per_frame;
    }

    if(p_block)
        outputbuffer->Enqueue(p_block);
}

aout_filters_t * AudioDecodedStream::AudioFiltersCreate(const es_format_t *afmt)
{
    return aout_FiltersNew(p_stream, &afmt->audio, &requestedoutput.audio, NULL, NULL);
}

int AudioDecodedStream::AudioDecCallback_update_format(decoder_t *p_dec)
{
    struct decoder_owner *p_owner;
    p_owner = container_of(p_dec, struct decoder_owner, dec);

    if( !AOUT_FMT_LINEAR(&p_dec->fmt_out.audio) )
        return VLC_EGENERIC;

    /* fixup */
    p_dec->fmt_out.audio.i_format = p_dec->fmt_out.i_codec;
    aout_FormatPrepare(&p_dec->fmt_out.audio);

    es_format_Clean(&p_owner->last_fmt_update);
    es_format_Copy(&p_owner->last_fmt_update, &p_dec->fmt_out);

    p_owner->last_fmt_update.audio.i_format = p_owner->last_fmt_update.i_codec;

    return VLC_SUCCESS;
}

void AudioDecodedStream::setCallbacks()
{
    static struct decoder_owner_callbacks dec_cbs;
    memset(&dec_cbs, 0, sizeof(dec_cbs));
    dec_cbs.audio.format_update = AudioDecCallback_update_format;
    dec_cbs.audio.queue = AudioDecCallback_queue;
    p_decoder->cbs = &dec_cbs;
}

CaptionsStream::CaptionsStream(vlc_object_t *p_obj, const StreamID &id,
                               AbstractStreamOutputBuffer *buffer)
    : AbstractStream(p_obj, id, buffer)
{

}

CaptionsStream::~CaptionsStream()
{
    FlushQueued();
}

bool CaptionsStream::init(const es_format_t *fmt)
{
    return (fmt->i_codec == VLC_CODEC_CEA608);
}

int CaptionsStream::Send(block_t *p_block)
{
    if(p_block->i_buffer)
        outputbuffer->Enqueue(p_block);
    else
        block_Release(p_block);
    return VLC_SUCCESS;
}

void CaptionsStream::Flush()
{

}

void CaptionsStream::Drain()
{

}

void CaptionsStream::FlushQueued()
{
    block_t *p;
    while((p = reinterpret_cast<block_t *>(outputbuffer->Dequeue())))
        block_Release(p);
}
