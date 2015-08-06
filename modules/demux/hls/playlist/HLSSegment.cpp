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
#include "HLSSegment.hpp"
#include "../adaptative/playlist/SegmentChunk.hpp"
#include "../adaptative/playlist/BaseRepresentation.h"
#include "../HLSStreamFormat.hpp"

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
    sequence = seq;
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

void HLSSegment::checkFormat(block_t *p_block, SegmentChunk *, BaseRepresentation *rep)
{
    if(rep->getStreamFormat() == StreamFormat(HLSStreamFormat::UNKNOWN))
    {
        if(p_block->i_buffer > 3 && !memcmp(p_block->p_buffer, "ID3", 3))
        {
            rep->setMimeType("audio/aac");
        }
        else
        {
            rep->setMimeType("video/mp2t");
        }
    }
}

void HLSSegment::onChunkDownload(block_t **pp_block, SegmentChunk *chunk, BaseRepresentation *rep)
{
    block_t *p_block = *pp_block;

    checkFormat(p_block, chunk, rep);

#ifdef HAVE_GCRYPT
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
                encryption.iv[15] = sequence & 0xff;
                encryption.iv[14] = (sequence >> 8)& 0xff;
                encryption.iv[13] = (sequence >> 16)& 0xff;
                encryption.iv[12] = (sequence >> 24)& 0xff;
            }

            if( gcry_cipher_open(&ctx, GCRY_CIPHER_AES, GCRY_CIPHER_MODE_CBC, 0) ||
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
                if(chunk->getBytesToRead() == 0)
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

void HLSSegment::setEncryption(SegmentEncryption &enc)
{
    encryption = enc;
}

void HLSSegment::debug(vlc_object_t *obj, int indent) const
{
    std::stringstream ss;
    ss << std::string(indent, ' ') << debugName <<
    " #" << sequence <<
    " url=" << getUrlSegment().toString();
    if(startByte!=endByte)
        ss << " @" << startByte << ".." << endByte;
    msg_Dbg(obj, "%s", ss.str().c_str());
}

int HLSSegment::compare(ISegment *segment) const
{
    HLSSegment *hlssegment = dynamic_cast<HLSSegment *>(segment);
    if(hlssegment)
    {
        if (sequence > hlssegment->sequence)
            return 1;
        else if(sequence < hlssegment->sequence)
            return -1;
        else
            return 0;
    }
    else return ISegment::compare(segment);
}
