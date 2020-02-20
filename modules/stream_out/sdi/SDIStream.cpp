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

AbstractQueueStreamOutputBuffer::AbstractQueueStreamOutputBuffer()
{
    b_draining = false;
}

AbstractQueueStreamOutputBuffer::~AbstractQueueStreamOutputBuffer()
{

}

void AbstractQueueStreamOutputBuffer::Enqueue(void *p)
{
    buffer_mutex.lock();
    queued.push(p);
    buffer_mutex.unlock();
}

void *AbstractQueueStreamOutputBuffer::Dequeue()
{
    void *p = NULL;
    buffer_mutex.lock();
    if(!queued.empty())
    {
        p = queued.front();
        queued.pop();
    }
    buffer_mutex.unlock();
    return p;
}

void AbstractQueueStreamOutputBuffer::Drain()
{
    buffer_mutex.lock();
    b_draining = true;
    buffer_mutex.unlock();
}

bool AbstractQueueStreamOutputBuffer::isEOS()
{
    buffer_mutex.lock();
    bool b = b_draining && queued.empty();
    buffer_mutex.unlock();
    return b;
}

BlockStreamOutputBuffer::BlockStreamOutputBuffer()
    : AbstractQueueStreamOutputBuffer()
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
    : AbstractQueueStreamOutputBuffer()
{
    vlc_sem_init(&pool_semaphore, 16);
}

PictureStreamOutputBuffer::~PictureStreamOutputBuffer()
{
}

void * PictureStreamOutputBuffer::Dequeue()
{
    void *p = AbstractQueueStreamOutputBuffer::Dequeue();
    if(p)
        vlc_sem_post(&pool_semaphore);
    return p;
}

void PictureStreamOutputBuffer::Enqueue(void *p)
{
    if(p)
        vlc_sem_wait(&pool_semaphore);
    AbstractQueueStreamOutputBuffer::Enqueue(p);
}

void PictureStreamOutputBuffer::FlushQueued()
{
    picture_t *p;
    while((p = reinterpret_cast<picture_t *>(Dequeue())))
        picture_Release(p);
}

vlc_tick_t PictureStreamOutputBuffer::NextPictureTime()
{
    vlc_tick_t t;
    buffer_mutex.lock();
    if(!queued.empty())
        t = reinterpret_cast<picture_t *>(queued.front())->date;
    else
        t = VLC_TICK_INVALID;
    buffer_mutex.unlock();
    return t;
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

bool StreamID::operator==(const StreamID &other) const
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
    vlc_decoder_device *dec_dev;
};

AbstractDecodedStream::AbstractDecodedStream(vlc_object_t *p_obj,
                                             const StreamID &id,
                                             AbstractStreamOutputBuffer *buffer)
    : AbstractStream(p_obj, id, buffer)
{
    p_decoder = NULL;
    es_format_Init(&requestedoutput, 0, 0);
    vlc_mutex_init(&inputLock);
    vlc_cond_init(&inputWait);
    threadEnd = false;
    status = DECODING;
    pcr = VLC_TICK_INVALID;
}

AbstractDecodedStream::~AbstractDecodedStream()
{
    Flush();
    deinit();
    es_format_Clean(&requestedoutput);
}

void AbstractDecodedStream::deinit()
{
    if(p_decoder)
    {
        Flush();
        vlc_mutex_lock(&inputLock);
        vlc_cond_signal(&inputWait);
        threadEnd = true;
        vlc_mutex_unlock(&inputLock);
        vlc_join(thread, NULL);
        ReleaseDecoder();
    }
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
    decoder_Init( p_decoder, p_fmt );

    setCallbacks();

    p_decoder->p_module = module_need_var(p_decoder, category, "codec");
    if(!p_decoder->p_module)
    {
        msg_Err(p_stream, "cannot find %s for %4.4s", category, (char *)&p_fmt->i_codec);
        ReleaseDecoder();
        return false;
    }

    if(vlc_clone(&thread, decoderThreadCallback, this, VLC_THREAD_PRIORITY_VIDEO))
    {
        es_format_Clean(&p_owner->decoder_out);
        es_format_Clean(&p_owner->last_fmt_update);
        decoder_Destroy( p_decoder );
        p_decoder = NULL;
        return false;
    }

    return true;
}

void * AbstractDecodedStream::decoderThreadCallback(void *me)
{
    reinterpret_cast<AbstractDecodedStream *>(me)->decoderThread();
    return NULL;
}

void AbstractDecodedStream::decoderThread()
{
    struct decoder_owner *p_owner =
            container_of(p_decoder, struct decoder_owner, dec);

    vlc_savecancel();
    vlc_mutex_lock(&inputLock);
    for(;;)
    {
        while(inputQueue.empty() && !threadEnd)
            vlc_cond_wait(&inputWait, &inputLock);
        if(threadEnd)
        {
            vlc_mutex_unlock(&inputLock);
            break;
        }

        block_t *p_block = inputQueue.front();
        inputQueue.pop();

        bool b_draincall = (status == DRAINING) && (p_block == NULL);
        vlc_mutex_unlock(&inputLock);

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

        vlc_mutex_lock(&inputLock);
        if(p_owner->b_error)
        {
            status = FAILED;
            outputbuffer->Drain();
        }
        else if(b_draincall)
        {
            status = DRAINED;
            outputbuffer->Drain();
        }
    }
}

int AbstractDecodedStream::Send(block_t *p_block)
{
    assert(p_decoder);
    vlc_mutex_lock(&inputLock);
    inputQueue.push(p_block);
    if(p_block)
    {
        vlc_tick_t t = std::min(p_block->i_dts, p_block->i_pts);
        if(t == VLC_TICK_INVALID)
            t = std::max(p_block->i_dts, p_block->i_pts);
        pcr = std::max(pcr, t);
    }
    vlc_cond_signal(&inputWait);
    vlc_mutex_unlock(&inputLock);
    return VLC_SUCCESS;
}

void AbstractDecodedStream::Flush()
{
    vlc_mutex_lock(&inputLock);
    while(!inputQueue.empty())
    {
        if(inputQueue.front())
            block_Release(inputQueue.front());
        inputQueue.pop();
    }
    vlc_mutex_unlock(&inputLock);
}

void AbstractDecodedStream::Drain()
{
    Send(NULL);
    vlc_mutex_lock(&inputLock);
    if(status != FAILED && status != DRAINED)
        status = DRAINING;
    vlc_mutex_unlock(&inputLock);
}

bool AbstractDecodedStream::isEOS()
{
    vlc_mutex_lock(&inputLock);
    bool b = (status == FAILED || status == DRAINED);
    vlc_mutex_unlock(&inputLock);
    return b;
}

bool AbstractDecodedStream::ReachedPlaybackTime(vlc_tick_t t)
{
    vlc_mutex_lock(&inputLock);
    bool b = (pcr != VLC_TICK_INVALID) && t < pcr;
    b |= (status == DRAINED) || (status == FAILED);
    vlc_mutex_unlock(&inputLock);
    return b;
}

void AbstractDecodedStream::ReleaseDecoder()
{
    struct decoder_owner *p_owner;
    p_owner = container_of(p_decoder, struct decoder_owner, dec);
    es_format_Clean(&p_owner->decoder_out);
    es_format_Clean(&p_owner->last_fmt_update);
    decoder_Destroy( p_decoder );
    p_decoder = NULL;
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
    captionsOutputBuffer = NULL;
}

VideoDecodedStream::~VideoDecodedStream()
{
    deinit();
    if(p_filters_chain)
        filter_chain_Delete(p_filters_chain);
}

void VideoDecodedStream::setCallbacks()
{
    static struct decoder_owner_callbacks dec_cbs;
    memset(&dec_cbs, 0, sizeof(dec_cbs));
    dec_cbs.video.get_device = VideoDecCallback_get_device;
    dec_cbs.video.format_update = VideoDecCallback_update_format;
    dec_cbs.video.queue = VideoDecCallback_queue;
    dec_cbs.video.queue_cc = captionsOutputBuffer ? VideoDecCallback_queue_cc : NULL;

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

vlc_decoder_device * VideoDecodedStream::VideoDecCallback_get_device(decoder_t *p_dec)
{
    struct decoder_owner *p_owner;
    p_owner = container_of(p_dec, struct decoder_owner, dec);
    if (p_owner->dec_dev == NULL)
    {
        p_owner->dec_dev = vlc_decoder_device_Create(&p_dec->obj, NULL);
    }
    return p_owner->dec_dev ? vlc_decoder_device_Hold(p_owner->dec_dev) : NULL;
}

void VideoDecodedStream::ReleaseDecoder()
{
    AbstractDecodedStream::ReleaseDecoder();

    struct decoder_owner *p_owner;
    p_owner = container_of(p_decoder, struct decoder_owner, dec);
    if (p_owner->dec_dev)
    {
        vlc_decoder_device_Release(p_owner->dec_dev);
        p_owner->dec_dev = NULL;
    }
}

int VideoDecodedStream::VideoDecCallback_update_format(decoder_t *p_dec,
                                                       vlc_video_context *)
{
    struct decoder_owner *p_owner;
    p_owner = container_of(p_dec, struct decoder_owner, dec);

    es_format_Clean(&p_owner->last_fmt_update);
    es_format_Copy(&p_owner->last_fmt_update, &p_dec->fmt_out);

    return VLC_SUCCESS;
}

static picture_t *transcode_video_filter_buffer_new(filter_t *p_filter)
{
    p_filter->fmt_out.video.i_chroma = p_filter->fmt_out.i_codec;
    return picture_NewFromFormat(&p_filter->fmt_out.video);
}

static const struct filter_video_callbacks transcode_filter_video_cbs =
{
    transcode_video_filter_buffer_new, NULL,
};

filter_chain_t * VideoDecodedStream::VideoFilterCreate(const es_format_t *p_srcfmt, vlc_video_context *vctx)
{
    filter_chain_t *p_chain;
    filter_owner_t owner;
    memset(&owner, 0, sizeof(owner));
    owner.video = &transcode_filter_video_cbs;

    p_chain = filter_chain_NewVideo(p_stream, false, &owner);
    if(!p_chain)
        return NULL;
    filter_chain_Reset(p_chain, p_srcfmt, vctx, &requestedoutput);

    if(p_srcfmt->video.i_chroma != requestedoutput.video.i_chroma)
    {
        if(filter_chain_AppendConverter(p_chain, &requestedoutput) != VLC_SUCCESS)
        {
            filter_chain_Delete(p_chain);
            return NULL;
        }

        const es_format_t *p_fmt_out = filter_chain_GetFmtOut(p_chain);
        if(!es_format_IsSimilar(&requestedoutput, p_fmt_out))
        {
            filter_chain_Delete(p_chain);
            return NULL;
        }
    }

    return p_chain;
}

void VideoDecodedStream::Output(picture_t *p_pic)
{
    struct decoder_owner *p_owner;
    p_owner = container_of(p_decoder, struct decoder_owner, dec);

    if(!es_format_IsSimilar(&p_owner->decoder_out, &p_owner->last_fmt_update))
    {
        es_format_Clean(&p_owner->decoder_out);
        es_format_Copy(&p_owner->decoder_out, &p_owner->last_fmt_update);

        msg_Dbg(p_stream, "decoder output format now %4.4s",
                (char*)&p_owner->decoder_out.i_codec);

        if(p_filters_chain)
            filter_chain_Delete(p_filters_chain);
        p_filters_chain = VideoFilterCreate(&p_owner->decoder_out,
                                            picture_GetVideoContext(p_pic));
        if(!p_filters_chain)
        {
            picture_Release(p_pic);
            return;
        }
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
    deinit();
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

    if(!es_format_IsSimilar(&p_owner->decoder_out, &p_owner->last_fmt_update))
    {
        es_format_Clean(&p_owner->decoder_out);
        es_format_Copy(&p_owner->decoder_out, &p_owner->last_fmt_update);

        msg_Dbg(p_stream, "decoder output format now %4.4s %u channels",
                (char*)&p_owner->decoder_out.i_codec,
                p_owner->decoder_out.audio.i_channels);

        if(p_filters)
            aout_FiltersDelete(p_stream, p_filters);
        p_filters = AudioFiltersCreate(&p_owner->decoder_out);
        if(!p_filters)
        {
            msg_Err(p_stream, "filter creation failed");
            block_Release(p_block);
            return;
        }
    }

    /* Run filter chain */
    if(p_filters)
        p_block = aout_FiltersPlay(p_filters, p_block, 1.f);

    if(p_block && !p_block->i_nb_samples &&
       p_owner->decoder_out.audio.i_bytes_per_frame )
    {
        p_block->i_nb_samples = p_block->i_buffer /
                p_owner->decoder_out.audio.i_bytes_per_frame;
    }

    if(p_block)
        outputbuffer->Enqueue(p_block);
}

aout_filters_t * AudioDecodedStream::AudioFiltersCreate(const es_format_t *afmt)
{
    return aout_FiltersNew(p_stream, &afmt->audio, &requestedoutput.audio, NULL);
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


AbstractRawStream::AbstractRawStream(vlc_object_t *p_obj, const StreamID &id,
                               AbstractStreamOutputBuffer *buffer)
    : AbstractStream(p_obj, id, buffer)
{
    pcr = VLC_TICK_INVALID;
    b_draining = false;
}

AbstractRawStream::~AbstractRawStream()
{
    FlushQueued();
}

int AbstractRawStream::Send(block_t *p_block)
{
    vlc_tick_t t = std::min(p_block->i_dts, p_block->i_pts);
    if(t == VLC_TICK_INVALID)
        t = std::max(p_block->i_dts, p_block->i_pts);
    if(p_block->i_buffer)
        outputbuffer->Enqueue(p_block);
    else
        block_Release(p_block);
    buffer_mutex.lock();
    pcr = std::max(pcr, t);
    buffer_mutex.unlock();
    return VLC_SUCCESS;
}

void AbstractRawStream::Flush()
{
    FlushQueued();
}

void AbstractRawStream::Drain()
{
    buffer_mutex.lock();
    b_draining = true;
    buffer_mutex.unlock();
}

bool AbstractRawStream::ReachedPlaybackTime(vlc_tick_t t)
{
    buffer_mutex.lock();
    bool b = (pcr != VLC_TICK_INVALID) && t < pcr;
    b |= b_draining;
    buffer_mutex.unlock();
    return b;
}

bool AbstractRawStream::isEOS()
{
    buffer_mutex.lock();
    bool b = b_draining;
    buffer_mutex.unlock();
    return b;
}

void AbstractRawStream::FlushQueued()
{
    block_t *p;
    while((p = reinterpret_cast<block_t *>(outputbuffer->Dequeue())))
        block_Release(p);
}


AbstractReorderedStream::AbstractReorderedStream(vlc_object_t *p_obj, const StreamID &id,
                                                 AbstractStreamOutputBuffer *buffer)
    : AbstractRawStream(p_obj, id, buffer)
{
    reorder_depth = 0;
    do_reorder = false;
}

AbstractReorderedStream::~AbstractReorderedStream()
{
}

int AbstractReorderedStream::Send(block_t *p_block)
{
    auto it = reorder.begin();
    if(do_reorder)
    {
        for(; it != reorder.end(); ++it)
        {
            if((*it)->i_pts == VLC_TICK_INVALID)
                continue;
            if(p_block->i_pts < (*it)->i_pts)
            {
                /* found insertion point */
                if(it == reorder.begin() &&
                   reorder_depth < 16 && reorder.size() < reorder_depth)
                      reorder_depth++;
                break;
            }
        }

        reorder.insert(it, p_block);

        if(reorder.size() <= reorder_depth + 1)
            return VLC_SUCCESS;

        p_block = reorder.front();
        reorder.pop_front();
    }

    return AbstractRawStream::Send(p_block);
}

void AbstractReorderedStream::Flush()
{
    Drain();
    FlushQueued();
}

void AbstractReorderedStream::Drain()
{
    while(!reorder.empty())
    {
        AbstractRawStream::Send(reorder.front());
        reorder.pop_front();
    }
}

void AbstractReorderedStream::setReorder(size_t r)
{
    reorder_depth = r;
    do_reorder = true;
}

AudioCompressedStream::AudioCompressedStream(vlc_object_t *p_obj, const StreamID &id,
                                             AbstractStreamOutputBuffer *buffer)
    : AbstractRawStream(p_obj, id, buffer)
{
}

AudioCompressedStream::~AudioCompressedStream()
{
}

int AudioCompressedStream::Send(block_t *p_block)
{
    const size_t i_payload = p_block->i_buffer;
    const size_t i_pad = (p_block->i_buffer & 1) ? 1 : 0;
    p_block = block_Realloc(p_block, 12, p_block->i_buffer + i_pad);
    if(!p_block)
        return VLC_EGENERIC;
    /* Convert to AES3 Payload */
    SetWBE(&p_block->p_buffer[0], 0x0000); /* Extra 0000 */
    SetWBE(&p_block->p_buffer[2], 0x0000); /* see S337 Annex B */
    SetWBE(&p_block->p_buffer[4], 0xF872); /* Pa Start code/Preamble */
    SetWBE(&p_block->p_buffer[6], 0x4E1F); /* Pb Start code/Preamble */
    SetWBE(&p_block->p_buffer[8], 0x0001); /* A52 Burst code */
    SetWBE(&p_block->p_buffer[10], i_payload);
    if(i_pad)
        p_block->p_buffer[p_block->i_buffer - 1] = 0x00;
    return AbstractRawStream::Send(p_block);
}

bool AudioCompressedStream::init(const es_format_t *fmt)
{
    return (fmt->i_codec == VLC_CODEC_A52);
}

CaptionsStream::CaptionsStream(vlc_object_t *p_obj, const StreamID &id,
                               AbstractStreamOutputBuffer *buffer)
    : AbstractReorderedStream(p_obj, id, buffer)
{

}

CaptionsStream::~CaptionsStream()
{
}

bool CaptionsStream::init(const es_format_t *fmt)
{
    if(fmt->subs.cc.i_reorder_depth >= 0)
        setReorder(fmt->subs.cc.i_reorder_depth);
    return (fmt->i_codec == VLC_CODEC_CEA608);
}

