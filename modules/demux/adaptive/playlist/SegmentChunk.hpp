/*
 * SegmentChunk.hpp
 *****************************************************************************
 * Copyright (C) 2014 - 2015 VideoLAN Authors
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
#ifndef SEGMENTCHUNK_HPP
#define SEGMENTCHUNK_HPP

#include <string>
#include "../http/Chunk.h"
#include "../StreamFormat.hpp"

namespace adaptive
{
    namespace encryption
    {
        class CommonEncryptionSession;
    }

    namespace playlist
    {
        using namespace http;
        using namespace encryption;

        class BaseRepresentation;

        class SegmentChunk : public AbstractChunk
        {
        public:
            SegmentChunk(AbstractChunkSource *, BaseRepresentation *);
            virtual ~SegmentChunk();
            void         setEncryptionSession(CommonEncryptionSession *);
            StreamFormat getStreamFormat() const;
            bool discontinuity;

        protected:
            bool         decrypt(block_t **);
            virtual void onDownload(block_t **); /* impl */
            BaseRepresentation *rep;
            CommonEncryptionSession *encryptionSession;
        };

    }
}
#endif // SEGMENTCHUNK_HPP
