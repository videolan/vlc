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

#include "AES3Audio.hpp"
#include <algorithm>
#include <cassert>

using namespace sdi_sout;

AES3AudioBuffer::AES3AudioBuffer(unsigned count)
{
    setSubFramesCount(count);
    block_BytestreamInit(&bytestream);
    toconsume = 0;
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

void AES3AudioBuffer::read(void *dstbuf, unsigned count,
                           const AES3AudioSubFrameIndex &dstbufsubframeidx,
                           const AES3AudioSubFrameIndex &srcchannelidx,
                           unsigned dstbufframeswidth)
{
    if(!srcchannelidx.isValid() || srcchannelidx.index() >= buffersubframes)
        return;

    if(dstbuf == NULL)
        return;
    bytestream_mutex.lock();
    uint8_t *dst = reinterpret_cast<uint8_t *>(dstbuf);
    for(unsigned i=0; i<count; i++)
    {
       size_t srcoffset = sizeof(uint16_t) * (i * buffersubframes + srcchannelidx.index());
       size_t dstoffset = sizeof(uint16_t) * (i * 2 * dstbufframeswidth + dstbufsubframeidx.index());
       block_PeekOffsetBytes(&bytestream, srcoffset, &dst[dstoffset], sizeof(uint16_t));
    }
    bytestream_mutex.unlock();
}

size_t AES3AudioBuffer::FramesToBytes(unsigned f) const
{
    return (size_t) f * sizeof(uint16_t) * buffersubframes;
}

vlc_tick_t AES3AudioBuffer::FramesToDuration(unsigned f) const
{
    return CLOCK_FREQ * f / 48000;
}

unsigned AES3AudioBuffer::BytesToFrames(size_t s) const
{
    return s / (sizeof(uint16_t) * buffersubframes);
}

unsigned AES3AudioBuffer::TicksDurationToFrames(vlc_tick_t t) const
{
    return t * 48000 / CLOCK_FREQ;
}

void AES3AudioBuffer::flushConsumed()
{
    if(toconsume)
    {
        size_t bytes = FramesToBytes(toconsume);
        bytestream_mutex.lock();
        block_SkipBytes(&bytestream, bytes);
        block_BytestreamFlush(&bytestream);
        bytestream_mutex.unlock();
        toconsume = 0;
    }
}

void AES3AudioBuffer::tagConsumed(unsigned f)
{
    assert(toconsume == 0 || toconsume == f);
    toconsume = f;
}

void AES3AudioBuffer::forwardTo(vlc_tick_t t)
{
    if(bufferStart() == VLC_TICK_INVALID)
        return;

    tagConsumed(TicksDurationToFrames(t - bytestream.p_block->i_pts));
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

vlc_tick_t AES3AudioBuffer::bufferEnd() const
{
    vlc_tick_t start = bufferStart();
    if(start != VLC_TICK_INVALID)
        start += CLOCK_FREQ * FramesToDuration(BytesToFrames(block_BytestreamRemaining(&bytestream)));
     return start;
}

unsigned AES3AudioBuffer::availableSamples() const
{
    bytestream_mutex.lock();
    unsigned samples = BytesToFrames(block_BytestreamRemaining(&bytestream));
    bytestream_mutex.unlock();
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

void AES3AudioSubFrameSource::copy(void *buf,
                                   unsigned count,
                                   const AES3AudioSubFrameIndex &srcsubframeidx,
                                   unsigned widthinframes)
{
    if(aes3AudioBuffer == NULL)
        return;
    aes3AudioBuffer->read(buf, count, srcsubframeidx, bufferSubFrameIdx, widthinframes);
}

void AES3AudioSubFrameSource::flushConsumed()
{
    if(aes3AudioBuffer)
        aes3AudioBuffer->flushConsumed();
}

void AES3AudioSubFrameSource::tagConsumed(unsigned count)
{
    if(aes3AudioBuffer)
        aes3AudioBuffer->tagConsumed(count);
}

const AES3AudioSubFrameIndex & AES3AudioSubFrameSource::index() const
{
    return bufferSubFrameIdx;
}

bool AES3AudioSubFrameSource::available() const
{
    return aes3AudioBuffer == NULL;
}

unsigned AES3AudioSubFrameSource::availableSamples() const
{
    if(aes3AudioBuffer == NULL)
        return 0;
    return aes3AudioBuffer->availableSamples();
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

unsigned AES3AudioFrameSource::availableSamples() const
{
    if(!subframe0.available() && !subframe1.available())
        return std::min(subframe0.availableSamples(), subframe1.availableSamples());
    else if(subframe1.available())
        return subframe0.availableSamples();
    else
        return subframe1.availableSamples();
}

void AES3AudioFrameSource::flushConsumed()
{
    subframe0.flushConsumed();
    subframe1.flushConsumed();
}

void AES3AudioFrameSource::tagConsumed(unsigned samples)
{
    subframe0.tagConsumed(samples);
    subframe1.tagConsumed(samples);
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
