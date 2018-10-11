/*****************************************************************************
 * av1_pack.h: AV1 sample packer
 *****************************************************************************
 * Copyright (C) 2018 VideoLabs, VLC authors and VideoLAN
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
#ifndef VLC_AV1_PACK_H
#define VLC_AV1_PACK_H

#include <vlc_common.h>
#include <vlc_block.h>

#include "../packetizer/av1_obu.h"

/*
    Strips unwanted OBU and last size field
    AV1 ISOBMFF Mapping 2.4
    https://aomediacodec.github.io/av1-isobmff/#sampleformat
    Matroska/WebM
    https://github.com/Matroska-Org/matroska-specification/blob/master/codec/av1.md
*/

static inline block_t *AV1_Pack_Sample(block_t *p_block)
{
    AV1_OBU_iterator_ctx_t ctx;
    AV1_OBU_iterator_init(&ctx, p_block->p_buffer, p_block->i_buffer);
    const uint8_t *p_obu = NULL; size_t i_obu;
    while(AV1_OBU_iterate_next(&ctx, &p_obu, &i_obu))
    {
        switch(AV1_OBUGetType(p_obu))
        {
            /* drops */
            case AV1_OBU_TEMPORAL_DELIMITER:
            case AV1_OBU_PADDING:
            case AV1_OBU_REDUNDANT_FRAME_HEADER:
            case AV1_OBU_TILE_LIST:
            {
                size_t i_offset = p_obu - p_block->p_buffer;
                if(i_offset < p_block->i_buffer - i_offset - i_obu)
                {
                    memmove(&p_block->p_buffer[i_obu], p_block->p_buffer, i_offset);
                    p_block->p_buffer += i_obu;
                    p_block->i_buffer -= i_obu;
                }
                else
                {
                    memmove(&p_block->p_buffer[i_offset], &p_obu[i_obu], i_obu);
                    p_block->i_buffer -= i_obu;
                }
                AV1_OBU_iterator_init(&ctx, p_block->p_buffer,
                                            p_block->i_buffer);
                p_obu = NULL;
            } break;
            default:
                break;
        }
    }

    if(p_obu && AV1_OBUHasSizeField(p_obu))
    {
        const size_t i_offset = p_obu - p_block->p_buffer;
        const uint8_t i_header = AV1_OBUHasExtensionField(p_obu) ? 2 : 1;
        uint8_t i_len = 0;
        (void) AV1_OBUSize(p_obu, p_block->i_buffer - i_offset, &i_len);
        if(i_len)
        {
            memmove(&p_block->p_buffer[i_offset + i_header],
                    &p_block->p_buffer[i_offset + i_header + i_len],
                    p_block->i_buffer - i_offset - i_header - i_len);
            p_block->p_buffer[i_offset] &= 0xFD;
            p_block->i_buffer -= i_len;
        }
    }

    if(!p_block->i_buffer)
    {
        block_Release(p_block);
        return NULL;
    }
    return p_block;
}

#endif
