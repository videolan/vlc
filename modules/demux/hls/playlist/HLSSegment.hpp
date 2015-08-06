/*
 * HLSSegment.hpp
 *****************************************************************************
 * Copyright (C) 2015 VideoLAN and VLC authors
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
#ifndef HLSSEGMENT_HPP
#define HLSSEGMENT_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "../adaptative/playlist/Segment.h"
#include <vector>
#ifdef HAVE_GCRYPT
 #include <gcrypt.h>
#endif

namespace hls
{
    namespace playlist
    {
        using namespace adaptative::playlist;

        class SegmentEncryption
        {
            public:
                SegmentEncryption();
                enum
                {
                    NONE,
                    AES_128,
                    AES_SAMPLE,
                } method;
                std::vector<uint8_t> key;
                std::vector<uint8_t> iv;
        };

        class HLSSegment : public Segment
        {
            public:
                HLSSegment( ICanonicalUrl *parent, uint64_t sequence );
                virtual ~HLSSegment();
                void setEncryption(SegmentEncryption &);
                void debug(vlc_object_t *, int) const; /* reimpl */
                virtual int compare(ISegment *) const; /* reimpl */

            protected:
                virtual void onChunkDownload(block_t **, SegmentChunk *, BaseRepresentation *); /* reimpl */
                void checkFormat(block_t *, SegmentChunk *, BaseRepresentation *);

                uint64_t sequence;
                SegmentEncryption encryption;
#ifdef HAVE_GCRYPT
                gcry_cipher_hd_t ctx;
#endif
        };
    }
}


#endif // HLSSEGMENT_HPP
