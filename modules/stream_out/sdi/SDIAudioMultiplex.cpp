/*****************************************************************************
 * SDIAudioMultiplex.cpp: SDI Audio Multiplexing
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

#include "SDIAudioMultiplex.hpp"
#include <limits>

using namespace sdi_sout;

SDIAudioMultiplexBuffer::SDIAudioMultiplexBuffer()
    : AES3AudioBuffer(2), AbstractStreamOutputBuffer()
{

}

SDIAudioMultiplexBuffer::~SDIAudioMultiplexBuffer()
{
    FlushQueued();
}

void SDIAudioMultiplexBuffer::FlushQueued()
{

}

void SDIAudioMultiplexBuffer::Enqueue(void *p)
{
    AES3AudioBuffer::push(reinterpret_cast<block_t *>(p));
}

void * SDIAudioMultiplexBuffer::Dequeue()
{
    return NULL;
}

SDIAudioMultiplexConfig::Mapping::Mapping(const StreamID &id)
    : id(id)
{

}

SDIAudioMultiplexConfig::SDIAudioMultiplexConfig(uint8_t channels)
{
    subframeslotbitmap = 0;
    if(channels > 4)
        framewidth = 8;
    else if(channels > 2)
        framewidth = 4;
    else
        framewidth = 1;
}

SDIAudioMultiplexConfig::~SDIAudioMultiplexConfig()
{
    for(size_t i=0; i<mappings.size(); i++)
        delete mappings[i];
}

bool SDIAudioMultiplexConfig::SubFrameSlotUsed(uint8_t i) const
{
    return (1 << i) & subframeslotbitmap;
}

void SDIAudioMultiplexConfig::setSubFrameSlotUsed(uint8_t i)
{
    subframeslotbitmap |= (1 << i);
}

std::vector<uint8_t> SDIAudioMultiplexConfig::getFreeSubFrameSlots() const
{
    std::vector<uint8_t> slots;
    for(uint8_t i=0; i<getMultiplexedFramesCount() * 2; i++)
    {
        if(!SubFrameSlotUsed(i))
            slots.push_back(i);
    }

    return slots;
}

bool SDIAudioMultiplexConfig::addMapping(const StreamID &id, std::vector<uint8_t> subframeslots)
{
    for(size_t i=0; i<mappings.size(); i++)
        if(mappings[i]->id == id)
            return false;
    for(size_t i=0; i<subframeslots.size(); i++)
        if(SubFrameSlotUsed(subframeslots[i]))
            return false;

    Mapping *assoc = new Mapping(id);
    assoc->subframesslots = subframeslots;

    mappings.push_back(assoc);

    return true;
}

unsigned SDIAudioMultiplexConfig::getMaxSamplesForBlockSize(size_t s) const
{
    return s / (2 * sizeof(uint16_t) * getMultiplexedFramesCount());
}

SDIAudioMultiplexBuffer *
    SDIAudioMultiplexConfig::getBufferForStream(const StreamID &id)
{
    for(size_t i=0; i<mappings.size(); i++)
    {
        if(mappings[i]->id == id)
            return &mappings[i]->buffer;
    }
    return NULL;
}

SDIAudioMultiplex::SDIAudioMultiplex(uint8_t channels)
{
    config = SDIAudioMultiplexConfig(channels);
}

SDIAudioMultiplex::~SDIAudioMultiplex()
{

}

unsigned SDIAudioMultiplex::availableSamples() const
{
    unsigned samples = std::numeric_limits<unsigned>::max();
    for(size_t i=0; i<MAX_AES3_AUDIO_FRAMES; i++)
    {
        if(framesources[i].subframe0.available() &&
           framesources[i].subframe1.available())
            continue;
        samples = std::min(samples, framesources[i].availableSamples());
    }
    return samples < std::numeric_limits<unsigned>::max() ? samples : 0;
}

vlc_tick_t SDIAudioMultiplex::bufferStart() const
{
    vlc_tick_t start = VLC_TICK_INVALID;
    for(size_t i=0; i<MAX_AES3_AUDIO_FRAMES; i++)
    {
        if(framesources[i].subframe0.available() &&
           framesources[i].subframe1.available())
            continue;
        vlc_tick_t t = framesources[i].bufferStartTime();
        if(start == VLC_TICK_INVALID ||
           (t != VLC_TICK_INVALID && t<start))
            start = t;
    }
    return start;
}

unsigned SDIAudioMultiplex::getFreeSubFrameSlots() const
{
    unsigned bitfield = 0;
    for(unsigned i=0; i<MAX_AES3_AUDIO_FRAMES; i++)
    {
        const AES3AudioFrameSource *source = &framesources[i];
        if(source->subframe0.available())
            bitfield |= (1 << (i * 2 + 0));
        if(source->subframe1.available())
            bitfield |= (1 << (i * 2 + 1));
    }
    return bitfield;
}

void SDIAudioMultiplex::SetSubFrameSource(uint8_t n, AES3AudioBuffer *buf,
                                          AES3AudioSubFrameIndex idx)
{
    assert(n<MAX_AES3_AUDIO_SUBFRAMES);
    AES3AudioFrameSource *f = &framesources[n / 2];
    AES3AudioSubFrameSource *s = (n & 1) ? &f->subframe1 : &f->subframe0;
    assert(s->available());
    *s = AES3AudioSubFrameSource(buf, idx);
}

block_t * SDIAudioMultiplex::Extract(unsigned samples)
{
    vlc_tick_t start = bufferStart();

    uint8_t interleavedframes = config.getMultiplexedFramesCount();

    block_t *p_block = block_Alloc( interleavedframes * 2 * sizeof(uint16_t) * samples );
    if(!p_block)
        return NULL;
    memset(p_block->p_buffer, 0, p_block->i_buffer);

    p_block->i_pts = p_block->i_dts = start;
    p_block->i_nb_samples = samples;

    for(unsigned i=0; i<MAX_AES3_AUDIO_FRAMES; i++)
    {
        AES3AudioFrameSource *source = &framesources[i];
        unsigned avail = source->availableSamples();
        if(avail == 0)
            continue;

        unsigned toskip = 0;
        unsigned tocopy = std::min(samples, avail);

        toskip = source->samplesUpToTime(start);
        if(toskip > tocopy)
            continue;
        tocopy -= toskip;

        source->subframe0.copy(p_block->p_buffer, tocopy, (i * 2 + 0), interleavedframes);
        source->subframe1.copy(p_block->p_buffer, tocopy, (i * 2 + 1), interleavedframes);
    }

    for(unsigned i=0; i<MAX_AES3_AUDIO_FRAMES; i++)
        framesources[i].tagConsumed(samples);
    for(unsigned i=0; i<MAX_AES3_AUDIO_FRAMES; i++)
        framesources[i].flushConsumed();

    return p_block;
}
