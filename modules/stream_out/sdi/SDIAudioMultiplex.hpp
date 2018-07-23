/*****************************************************************************
 * SDIAudioMultiplex.hpp: SDI Audio Multiplexing
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
#ifndef SDIAUDIOMULTIPLEX_HPP
#define SDIAUDIOMULTIPLEX_HPP

#include "AES3Audio.hpp"
#include "SDIStream.hpp"

#include <vector>

#define SAMPLES_PER_FRAME        (1536/4)

namespace sdi_sout
{
    class SDIAudioMultiplexBuffer : public AES3AudioBuffer,
                                    public AbstractStreamOutputBuffer
    {
        public:
            SDIAudioMultiplexBuffer();
            virtual ~SDIAudioMultiplexBuffer();
            virtual void FlushQueued(); /* impl */
            virtual void Enqueue(void *); /* impl */
            virtual void * Dequeue(); /* impl */
    };

    class SDIAudioMultiplexConfig
    {
        public:
            SDIAudioMultiplexConfig(uint8_t channels = 2);
            ~SDIAudioMultiplexConfig();
            SDIAudioMultiplexBuffer *getBufferForStream(const StreamID &);
            bool SubFrameSlotUsed(uint8_t) const;
            void setSubFrameSlotUsed(uint8_t);
            uint8_t getMultiplexedFramesCount() const { return framewidth; }
            std::vector<uint8_t> getFreeSubFrameSlots() const;

            bool addMapping(const StreamID &, std::vector<uint8_t>);
            unsigned getMaxSamplesForBlockSize(size_t) const;

        private:
            class Mapping
            {
                public:
                    Mapping(const StreamID &);
                    StreamID id;
                    SDIAudioMultiplexBuffer buffer;
                    std::vector<uint8_t> subframesslots;
            };
            std::vector<Mapping *> mappings;
            unsigned subframeslotbitmap;
            uint8_t framewidth;
    };

    class SDIAudioMultiplex
    {
        public:
            SDIAudioMultiplex(uint8_t channels);
            ~SDIAudioMultiplex();
            vlc_tick_t bufferStart() const;
            unsigned availableSamples() const;
            block_t * Extract(unsigned);
            unsigned getFreeSubFrameSlots() const;
            void SetSubFrameSource(uint8_t, AES3AudioBuffer *, AES3AudioSubFrameIndex);

            SDIAudioMultiplexConfig config;

        private:
            unsigned count;
            AES3AudioFrameSource framesources[MAX_AES3_AUDIO_FRAMES];
    };
}


#endif // SDIAUDIOMULTIPLEX_HPP
