/*****************************************************************************
 * AES3Buffer.hpp: AES3 audio buffer
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
#ifndef AES3AUDIO_HPP
#define AES3AUDIO_HPP

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_block_helper.h>
#include <mutex>
#include <vlc_es.h>

#define MAX_AES3_AUDIO_FRAMES     8
#define MAX_AES3_AUDIO_SUBFRAMES (MAX_AES3_AUDIO_FRAMES * 2)

namespace sdi_sout
{
    class AES3AudioSubFrameIndex
    {
        public:
            AES3AudioSubFrameIndex(uint8_t = MAX_AES3_AUDIO_SUBFRAMES);
            uint8_t index() const;
            bool isValid() const;
        private:
            uint8_t subframeindex;
    };

    class AES3AudioBuffer
    {
        public:
            AES3AudioBuffer(vlc_object_t *, unsigned = 0);
            ~AES3AudioBuffer();
            void setSubFramesCount(uint8_t);
            vlc_tick_t bufferStart() const;
            unsigned availableVirtualSamples(vlc_tick_t) const;
            unsigned alignedInterleaveInSamples(vlc_tick_t, unsigned) const;
            void push(block_t *);
            unsigned read(void *, unsigned, vlc_tick_t,
                          const AES3AudioSubFrameIndex &,
                          const AES3AudioSubFrameIndex &, unsigned);
            void flushConsumed();
            void tagVirtualConsumed(vlc_tick_t, unsigned);
            void forwardTo(vlc_tick_t);
            void setCodec(vlc_fourcc_t);
            vlc_fourcc_t getCodec() const;

        private:
            vlc_object_t *obj;
            void tagConsumed(unsigned);
            size_t   FramesToBytes(unsigned) const;
            vlc_tick_t FramesToDuration(unsigned) const;
            int OffsetToBufferStart(vlc_tick_t t) const;
            unsigned BytesToFrames(size_t) const;
            unsigned TicksDurationToFrames(vlc_tick_t) const;
            block_bytestream_t bytestream;
            mutable std::mutex bytestream_mutex;
            uint8_t buffersubframes;
            unsigned toconsume;
            vlc_fourcc_t i_codec;
    };

    class AES3AudioSubFrameSource
    {
        public:
            AES3AudioSubFrameSource();
            AES3AudioSubFrameSource(AES3AudioBuffer *, AES3AudioSubFrameIndex);
            vlc_tick_t bufferStartTime() const;
            unsigned copy(void *, unsigned count, vlc_tick_t,
                          const AES3AudioSubFrameIndex &, unsigned width);
            void flushConsumed();
            void tagVirtualConsumed(vlc_tick_t, unsigned);
            void forwardTo(vlc_tick_t t);
            unsigned availableVirtualSamples(vlc_tick_t) const;
            unsigned alignedInterleaveInSamples(vlc_tick_t, unsigned) const;
            bool available() const;
            vlc_fourcc_t getCodec() const;

        private:
            AES3AudioBuffer *aes3AudioBuffer;
            AES3AudioSubFrameIndex bufferSubFrameIdx; /* alias channel */
    };

    class AES3AudioFrameSource
    {
        public:
            AES3AudioFrameSource();
            vlc_tick_t bufferStartTime() const;
            unsigned samplesUpToTime(vlc_tick_t) const;
            unsigned availableVirtualSamples(vlc_tick_t) const;
            unsigned alignedInterleaveInSamples(vlc_tick_t, unsigned) const;
            void flushConsumed();
            void tagVirtualConsumed(vlc_tick_t, unsigned);
            void forwardTo(vlc_tick_t t);
            AES3AudioSubFrameSource subframe0;
            AES3AudioSubFrameSource subframe1;
    };

}

#endif // AES3AUDIO_HPP
