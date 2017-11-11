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

#include <vlc_common.h>
#include <vlc_block.h>
#ifdef HAVE_GCRYPT
 #include <vlc_gcrypt.h>
#endif

using namespace hls::playlist;

SegmentEncryption::SegmentEncryption()
{
    method = SegmentEncryption::NONE;
}

HLSSegment::HLSSegment( ICanonicalUrl *parent, uint64_t seq ) :
    Segment( parent )
{
    setSequenceNumber(seq);
    utcTime = 0;
#ifdef HAVE_GCRYPT
    ctx = NULL;
#endif
}

HLSSegment::~HLSSegment()
{
#ifdef HAVE_GCRYPT
    if(ctx)
        gcry_cipher_close(ctx);
#endif
}

void HLSSegment::onChunkDownload(block_t **pp_block, SegmentChunk *chunk, BaseRepresentation *)
{
    block_t *p_block = *pp_block;

#ifndef HAVE_GCRYPT
    (void)chunk;
#else
    if(encryption.method == SegmentEncryption::AES_128)
    {
        block_t *p_block = *pp_block;
        /* first bytes */
        if(!ctx && chunk->getBytesRead() == p_block->i_buffer)
        {
            vlc_gcrypt_init();
            if (encryption.iv.size() != 16)
            {
                encryption.iv.clear();
                encryption.iv.resize(16);
                encryption.iv[15] = (getSequenceNumber() - Segment::SEQUENCE_FIRST) & 0xff;
                encryption.iv[14] = ((getSequenceNumber() - Segment::SEQUENCE_FIRST) >> 8)& 0xff;
                encryption.iv[13] = ((getSequenceNumber() - Segment::SEQUENCE_FIRST) >> 16)& 0xff;
                encryption.iv[12] = ((getSequenceNumber() - Segment::SEQUENCE_FIRST) >> 24)& 0xff;
            }

            if( gcry_cipher_open(&ctx, GCRY_CIPHER_AES, GCRY_CIPHER_MODE_CBC, 0) ||
                encryption.key.size() != 16 ||
                gcry_cipher_setkey(ctx, &encryption.key[0], 16) ||
                gcry_cipher_setiv(ctx, &encryption.iv[0], 16) )
            {
                gcry_cipher_close(ctx);
                ctx = NULL;
            }
        }

        if(ctx)
        {
            if ((p_block->i_buffer % 16) != 0 || p_block->i_buffer < 16 ||
                gcry_cipher_decrypt(ctx, p_block->p_buffer, p_block->i_buffer, NULL, 0))
            {
                p_block->i_buffer = 0;
                gcry_cipher_close(ctx);
                ctx = NULL;
            }
            else
            {
                /* last bytes */
                if(chunk->isEmpty())
                {
                    /* remove the PKCS#7 padding from the buffer */
                    const uint8_t pad = p_block->p_buffer[p_block->i_buffer - 1];
                    for(uint8_t i=0; i<pad && i<=16; i++)
                    {
                        if(p_block->p_buffer[p_block->i_buffer - i - 1] != pad)
                            break;

                        if(i==pad)
                            p_block->i_buffer -= pad;
                    }

                    gcry_cipher_close(ctx);
                    ctx = NULL;
                }
            }
        }
    }
    else
#endif
    if(encryption.method != SegmentEncryption::NONE)
    {
        p_block->i_buffer = 0;
    }
}

mtime_t HLSSegment::getUTCTime() const
{
    return utcTime;
}

void HLSSegment::setEncryption(SegmentEncryption &enc)
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
