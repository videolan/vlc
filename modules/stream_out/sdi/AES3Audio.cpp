/*****************************************************************************
 * AES3Buffer.cpp: AES3 audio buffer
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

#include "sdiout.hpp"
#include "AES3Audio.hpp"
#include <algorithm>
#include <cassert>

using namespace sdi_sout;

AES3AudioBuffer::AES3AudioBuffer(vlc_object_t *p_obj, unsigned count)
{
    obj = p_obj;
    setSubFramesCount(count);
    block_BytestreamInit(&bytestream);
    toconsume = 0;
    i_codec = VLC_CODEC_S16N;
}

AES3AudioBuffer::~AES3AudioBuffer()
{
    block_BytestreamRelease(&bytestream);
}

void AES3AudioBuffer::setSubFramesCount(uint8_t c)
{
    buffersubframes = c;
}

void AES3AudioBuffer::push(block_t *p_block)
{
    bytestream_mutex.lock();
    block_BytestreamPush(&bytestream, p_block);
    bytestream_mutex.unlock();
}

unsigned AES3AudioBuffer::read(void *dstbuf, unsigned count, vlc_tick_t from,
                               const AES3AudioSubFrameIndex &dstbufsubframeidx,
                               const AES3AudioSubFrameIndex &srcchannelidx,
                               unsigned dstbufframeswidth)
{
    if(!srcchannelidx.isValid() || srcchannelidx.index() >= buffersubframes)
        return 0;

#ifdef SDI_MULTIPLEX_DEBUG
    unsigned orig = count;
    assert(count);
#endif

    unsigned dstpad = 0;
    unsigned skip = 0;
    int offset = OffsetToBufferStart(from);
    if(llabs(offset) >= count)
        return 0;

    if(offset > 0) /* buffer is ahead in time */
    {
        dstpad = offset;
        count -= offset;
    }
    else if(offset < 0)  /* we're past buffer start */
    {
        skip = -offset;
        count += offset;
    }

#ifdef SDI_MULTIPLEX_DEBUG
    unsigned inbuffer = BytesToFrames(block_BytestreamRemaining(&bytestream));
    msg_Dbg(obj, "%4.4s inbuffer %u count %u/%u skip %u pad %u",
            reinterpret_cast<const char *>(&i_codec), inbuffer, count, orig, skip, dstpad);
    assert(count + skip <= inbuffer);
    assert(count + dstpad <= orig);
#endif

    bytestream_mutex.lock();
    uint8_t *dst = reinterpret_cast<uint8_t *>(dstbuf);
    for(unsigned i=0; i<count; i++)
    {
       size_t srcoffset = sizeof(uint16_t) * ((i + skip) * buffersubframes + srcchannelidx.index());
       size_t dstoffset = sizeof(uint16_t) * ((i + dstpad) * 2 * dstbufframeswidth + dstbufsubframeidx.index());
       if(i_codec != VLC_CODEC_S16N)
       {
           assert(bytestream.i_block_offset == 0 || skip == 0);
           assert(bytestream.p_block->i_buffer < 4 ||
                  GetWBE(&bytestream.p_block->p_buffer[4]) == 0xf872);
       }
       if(dst)
            block_PeekOffsetBytes(&bytestream, srcoffset, &dst[dstoffset], sizeof(uint16_t));
    }
    bytestream_mutex.unlock();

    return 0;
}

size_t AES3AudioBuffer::FramesToBytes(unsigned f) const
{
    return (size_t) f * sizeof(uint16_t) * buffersubframes;
}

vlc_tick_t AES3AudioBuffer::FramesToDuration(unsigned f) const
{
    return vlc_tick_from_samples(f, 48000);
}

unsigned AES3AudioBuffer::BytesToFrames(size_t s) const
{
    return s / (sizeof(uint16_t) * buffersubframes);
}

unsigned AES3AudioBuffer::TicksDurationToFrames(vlc_tick_t t) const
{
    return samples_from_vlc_tick(t, 48000);
}

int AES3AudioBuffer::OffsetToBufferStart(vlc_tick_t t) const
{
    vlc_tick_t bufferstart = bufferStart();
    if(bufferstart == VLC_TICK_INVALID)
        return 0;

    if(t >= bufferstart)
        return -TicksDurationToFrames(t - bufferstart);
    else
        return TicksDurationToFrames(bufferstart - t);
}

void AES3AudioBuffer::flushConsumed()
{
    if(toconsume)
    {
        size_t bytes = FramesToBytes(toconsume);
        bytestream_mutex.lock();
        if(block_SkipBytes(&bytestream, bytes) == VLC_SUCCESS)
            block_BytestreamFlush(&bytestream);
        else
            block_BytestreamEmpty(&bytestream);
        bytestream_mutex.unlock();
#ifdef SDI_MULTIPLEX_DEBUG
        msg_Dbg(obj, "%4.4s flushed off %zd -> pts %ld",
                reinterpret_cast<const char *>(&i_codec),
                bytestream.i_block_offset, bufferStart());
#endif
        toconsume = 0;
    }
}

void AES3AudioBuffer::tagVirtualConsumed(vlc_tick_t from, unsigned f)
{
    if(bufferStart() == VLC_TICK_INVALID)
    {
        f = 0;
    }
    else
    {
        int offset = OffsetToBufferStart(from);
        if(offset > 0)
        {
            if((unsigned)offset >= f)
                f = 0;
            else
                f -= offset;
        }
        else if (offset < 0)
        {
            if((unsigned)(-offset) > f)
                f = 0;
            else
                f += offset;
        }
    }
    tagConsumed(f);
}

void AES3AudioBuffer::tagConsumed(unsigned f)
{
    assert(toconsume == 0 || toconsume == f);
    toconsume = f;
}

void AES3AudioBuffer::forwardTo(vlc_tick_t t)
{
    if(bufferStart() == VLC_TICK_INVALID || t <= bufferStart())
        return;

    tagConsumed(TicksDurationToFrames(t - bufferStart()));
    flushConsumed();
}

void AES3AudioBuffer::setCodec(vlc_fourcc_t i_codec)
{
    this->i_codec = i_codec;
}

vlc_fourcc_t AES3AudioBuffer::getCodec() const
{
    return i_codec;
}

vlc_tick_t AES3AudioBuffer::bufferStart() const
{
    vlc_tick_t start = VLC_TICK_INVALID;
    bytestream_mutex.lock();
    if(bytestream.p_block)
        start = bytestream.p_block->i_pts +
                FramesToDuration(BytesToFrames(bytestream.i_block_offset));
    bytestream_mutex.unlock();
    return start;
}

unsigned AES3AudioBuffer::availableVirtualSamples(vlc_tick_t from) const
{
    vlc_tick_t start = bufferStart();
    if(start == VLC_TICK_INVALID)
        return 0;

    bytestream_mutex.lock();
    /* FIXME */
    unsigned samples = BytesToFrames(block_BytestreamRemaining(&bytestream));
    bytestream_mutex.unlock();

    int offset = OffsetToBufferStart(from);
    if(offset > 0)
    {
        samples += offset;
    }
    else if(offset < 0)
    {
        if((unsigned)-offset > samples)
            samples = 0;
        else
            samples += offset;
    }

    return samples;
}

unsigned AES3AudioBuffer::alignedInterleaveInSamples(vlc_tick_t from, unsigned i_wanted) const
{
    if(i_codec == VLC_CODEC_S16N)
        return i_wanted;
    if(!bytestream.p_block)
        return i_wanted; /* no care, won't be able to read */
    unsigned samples = BytesToFrames(bytestream.p_block->i_buffer - bytestream.i_block_offset);
    int offsetsamples = OffsetToBufferStart(from);
    if(offsetsamples > 0)
    {
        /* align to our start */
        samples = offsetsamples;
    }
    else if(offsetsamples < 0)
    {
        /* align to our end */
    }
#ifdef SDI_MULTIPLEX_DEBUG
    msg_Dbg(obj, "%4.4s interleave samples %u -- ibuf %zd off %zd",
            reinterpret_cast<const char *>(&i_codec), samples,
            bytestream.p_block->i_buffer, bytestream.i_block_offset);
#endif
    return samples;
}

AES3AudioSubFrameSource::AES3AudioSubFrameSource()
{
    aes3AudioBuffer = NULL;
}

AES3AudioSubFrameSource::AES3AudioSubFrameSource(AES3AudioBuffer *buf, AES3AudioSubFrameIndex idx)
{
    aes3AudioBuffer = buf;
    bufferSubFrameIdx = idx;
}

vlc_tick_t AES3AudioSubFrameSource::bufferStartTime() const
{
    if(available())
        return VLC_TICK_INVALID;
    else return aes3AudioBuffer->bufferStart();
}

unsigned AES3AudioSubFrameSource::copy(void *buf,
                                       unsigned count,
                                       vlc_tick_t from,
                                       const AES3AudioSubFrameIndex &srcsubframeidx,
                                       unsigned widthinframes)
{
    if(aes3AudioBuffer == NULL)
        return 0;
    return aes3AudioBuffer->read(buf, count, from, srcsubframeidx, bufferSubFrameIdx, widthinframes);
}

void AES3AudioSubFrameSource::flushConsumed()
{
    if(aes3AudioBuffer)
        aes3AudioBuffer->flushConsumed();
}

void AES3AudioSubFrameSource::tagVirtualConsumed(vlc_tick_t from, unsigned count)
{
    if(aes3AudioBuffer)
        aes3AudioBuffer->tagVirtualConsumed(from, count);
}

void AES3AudioSubFrameSource::forwardTo(vlc_tick_t t)
{
    if(aes3AudioBuffer)
        aes3AudioBuffer->forwardTo(t);
}

bool AES3AudioSubFrameSource::available() const
{
    return aes3AudioBuffer == NULL;
}

vlc_fourcc_t AES3AudioSubFrameSource::getCodec() const
{
    if(aes3AudioBuffer == NULL)
        return 0;
    return aes3AudioBuffer->getCodec();
}

unsigned AES3AudioSubFrameSource::availableVirtualSamples(vlc_tick_t from) const
{
    if(aes3AudioBuffer == NULL)
        return 0;
    return aes3AudioBuffer->availableVirtualSamples(from);
}

unsigned AES3AudioSubFrameSource::alignedInterleaveInSamples(vlc_tick_t from, unsigned n) const
{
    if(aes3AudioBuffer == NULL)
        return 0;
    return aes3AudioBuffer->alignedInterleaveInSamples(from, n);
}

AES3AudioFrameSource::AES3AudioFrameSource()
{

}

vlc_tick_t AES3AudioFrameSource::bufferStartTime() const
{
    vlc_tick_t ret0 = subframe0.bufferStartTime();
    vlc_tick_t ret1 = subframe1.bufferStartTime();
    if(ret0 == VLC_TICK_INVALID)
       return ret1;
    else if(ret1 == VLC_TICK_INVALID || ret1 > ret0)
       return ret0;
    else
       return ret1;
}

unsigned AES3AudioFrameSource::samplesUpToTime(vlc_tick_t t) const
{
    int64_t diff = t - bufferStartTime();
    if(diff <= 0)
        return 0;
    return diff / (48000 * 2 * 2);
}

unsigned AES3AudioFrameSource::availableVirtualSamples(vlc_tick_t from) const
{
    if(!subframe0.available() && !subframe1.available())
        return std::min(subframe0.availableVirtualSamples(from), subframe1.availableVirtualSamples(from));
    else if(subframe1.available())
        return subframe0.availableVirtualSamples(from);
    else
        return subframe1.availableVirtualSamples(from);
}

unsigned AES3AudioFrameSource::alignedInterleaveInSamples(vlc_tick_t from, unsigned i_wanted) const
{
    unsigned a0 = i_wanted, a1 = i_wanted;
    if(!subframe0.available())
        a0 = subframe0.alignedInterleaveInSamples(from, i_wanted);
    if(!subframe1.available())
        a1 = subframe1.alignedInterleaveInSamples(from, i_wanted);
    return std::max(a0, a1);
}

void AES3AudioFrameSource::flushConsumed()
{
    subframe0.flushConsumed();
    subframe1.flushConsumed();
}

void AES3AudioFrameSource::tagVirtualConsumed(vlc_tick_t from, unsigned samples)
{
    subframe0.tagVirtualConsumed(from, samples);
    subframe1.tagVirtualConsumed(from, samples);
}

void AES3AudioFrameSource::forwardTo(vlc_tick_t t)
{
    subframe0.forwardTo(t);
    subframe1.forwardTo(t);
}

AES3AudioSubFrameIndex::AES3AudioSubFrameIndex(uint8_t v)
{
    subframeindex = v;
}

uint8_t AES3AudioSubFrameIndex::index() const
{
    return subframeindex;
}

bool AES3AudioSubFrameIndex::isValid() const
{
    return subframeindex < MAX_AES3_AUDIO_SUBFRAMES;
}
