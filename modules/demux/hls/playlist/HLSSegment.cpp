/*
 * HLSSegment.cpp
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "HLSSegment.hpp"
#include "../adaptive/playlist/SegmentChunk.hpp"
#include "../adaptive/playlist/BaseRepresentation.h"
#include "../adaptive/encryption/CommonEncryption.hpp"

#include <vlc_common.h>
#include <vlc_block.h>

using namespace hls::playlist;

HLSSegment::HLSSegment( ICanonicalUrl *parent, uint64_t seq ) :
    Segment( parent )
{
    setSequenceNumber(seq);
    utcTime = 0;
}

HLSSegment::~HLSSegment()
{
}

void HLSSegment::onChunkDownload(block_t **pp_block, SegmentChunk *chunk, BaseRepresentation *)
{
    block_t *p_block = *pp_block;

#ifndef HAVE_GCRYPT
    (void)chunk;
#else
    if(encryption.method == CommonEncryption::Method::AES_128)
    {
        if (encryption.iv.size() != 16)
        {
            encryption.iv.clear();
            encryption.iv.resize(16);
            encryption.iv[15] = (getSequenceNumber() - Segment::SEQUENCE_FIRST) & 0xff;
            encryption.iv[14] = ((getSequenceNumber() - Segment::SEQUENCE_FIRST) >> 8)& 0xff;
            encryption.iv[13] = ((getSequenceNumber() - Segment::SEQUENCE_FIRST) >> 16)& 0xff;
            encryption.iv[12] = ((getSequenceNumber() - Segment::SEQUENCE_FIRST) >> 24)& 0xff;
        }

        block_t *p_block = *pp_block;
        /* first bytes */
        if(chunk->getBytesRead() == p_block->i_buffer)
        {
            if(!encryptSession.start(encryption))
            {
                p_block->i_buffer = 0;
                return;
            }
        }

        bool b_last = chunk->isEmpty();
        p_block->i_buffer = encryptSession.decrypt(p_block->p_buffer, p_block->i_buffer, b_last);
        if(b_last)
            encryptSession.close();
    }
    else
#endif
    if(encryption.method != CommonEncryption::Method::NONE)
    {
        p_block->i_buffer = 0;
    }
}

vlc_tick_t HLSSegment::getUTCTime() const
{
    return utcTime;
}

void HLSSegment::setEncryption(CommonEncryption &enc)
{
    encryption = enc;
}

int HLSSegment::compare(ISegment *segment) const
{
    HLSSegment *hlssegment = dynamic_cast<HLSSegment *>(segment);
    if(hlssegment)
    {
        if (getSequenceNumber() > hlssegment->getSequenceNumber())
            return 1;
        else if(getSequenceNumber() < hlssegment->getSequenceNumber())
            return -1;
        else
            return 0;
    }
    else return ISegment::compare(segment);
}
