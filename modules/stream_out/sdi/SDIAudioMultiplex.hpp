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
#include "sdiout.hpp"

#include <vector>

#define SAMPLES_PER_FRAME        (1536U/4)

namespace sdi_sout
{
    class SDIAudioMultiplexBuffer : public AES3AudioBuffer,
                                    public AbstractStreamOutputBuffer
    {
        public:
            SDIAudioMultiplexBuffer(vlc_object_t *);
            virtual ~SDIAudioMultiplexBuffer();
            virtual void FlushQueued(); /* impl */
            virtual void Enqueue(void *); /* impl */
            virtual void * Dequeue(); /* impl */
            virtual void Drain(); /* impl */
            virtual bool isEOS(); /* impl */
        private:
            bool b_draining;
    };

    class SDIAudioMultiplexConfig
    {
        public:
            SDIAudioMultiplexConfig(vlc_object_t *obj, uint8_t channels = 2);
            ~SDIAudioMultiplexConfig();
            SDIAudioMultiplexBuffer *getBufferForStream(const StreamID &);
            const es_format_t * getConfigurationForStream(const StreamID &) const;
            const es_format_t * updateFromRealESConfig(const StreamID &,
                                                       const es_format_t *);
            bool decode(const StreamID &) const;
            bool SubFrameSlotUsed(uint8_t) const;
            void setSubFrameSlotUsed(uint8_t);
            void parseConfiguration(vlc_object_t *, const char *);
            uint8_t getMultiplexedFramesCount() const { return framewidth; }
            std::vector<uint8_t> getFreeSubFrameSlots(bool = false) const;
            std::vector<uint8_t> getConfiguredSlots(const StreamID &) const;

            bool addMapping(const StreamID &, const es_format_t *);
            bool addMapping(const StreamID &, std::vector<uint8_t>);
            bool addMappingEmbed(const StreamID &, std::vector<uint8_t> = std::vector<uint8_t>());
            unsigned getMaxSamplesForBlockSize(size_t) const;

        private:
            bool addMapping(const StreamID &, unsigned);
            class Mapping
            {
                public:
                    Mapping(vlc_object_t *, const StreamID &);
                    ~Mapping();
                    StreamID id;
                    es_format_t fmt;
                    bool b_decode;
                    SDIAudioMultiplexBuffer buffer;
                    std::vector<uint8_t> subframesslots;
            };
            std::vector<Mapping *> mappings;
            Mapping *getMappingByID(const StreamID &);
            const Mapping *getMappingByID(const StreamID &) const;
            unsigned subframeslotbitmap;
            uint8_t framewidth;
            bool b_accept_any;
            vlc_object_t *obj;
    };

    class SDIAudioMultiplex
    {
        public:
            SDIAudioMultiplex(vlc_object_t *, uint8_t channels);
            ~SDIAudioMultiplex();
            vlc_tick_t bufferStart() const;
            unsigned availableVirtualSamples(vlc_tick_t) const;
            unsigned alignedInterleaveInSamples(vlc_tick_t, unsigned) const;
            block_t * Extract(unsigned);
            unsigned getFreeSubFrameSlots() const;
            void SetSubFrameSource(uint8_t, AES3AudioBuffer *, AES3AudioSubFrameIndex);
#ifdef SDI_MULTIPLEX_DEBUG
            void Debug() const;
#endif

            SDIAudioMultiplexConfig config;
            vlc_tick_t head;

        private:
            vlc_object_t *p_obj;
            unsigned count;
            AES3AudioFrameSource framesources[MAX_AES3_AUDIO_FRAMES];
    };
}


#endif // SDIAUDIOMULTIPLEX_HPP
