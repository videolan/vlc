/*****************************************************************************
 * av1_unpack.h: AV1 samples expander
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
#ifndef VLC_AV1_UNPACK_H
#define VLC_AV1_UNPACK_H

#include "../packetizer/av1_obu.h"
#include <vlc_common.h>
#include <vlc_block.h>

static inline uint8_t leb128_expected(uint32_t v)
{
    if     (v < (1U << 7))  return 1;
    else if(v < (1U << 14)) return 2;
    else if(v < (1U << 21)) return 3;
    else if(v < (1U << 28)) return 4;
    else                    return 5;
}

static inline void leb128_write(uint32_t v, uint8_t *p)
{
    for(;;)
    {
        *p = v & 0x7F;
        v >>= 7;
        if(v == 0)
            break;
        *p++ |= 0x80;
    }
}

static inline block_t * AV1_Unpack_Sample_ExpandSize(block_t *p_block)
{
    AV1_OBU_iterator_ctx_t ctx;
    AV1_OBU_iterator_init(&ctx, p_block->p_buffer, p_block->i_buffer);
    const uint8_t *p_obu = NULL; size_t i_obu;
    while(AV1_OBU_iterate_next(&ctx, &p_obu, &i_obu))
    {
        if(AV1_OBUHasSizeField(p_obu))
            continue;
        const uint8_t i_header = 1 + AV1_OBUHasExtensionField(p_obu);
        const uint8_t i_sizelen = leb128_expected(i_obu - i_header);
        const size_t i_obu_offset = p_obu - p_block->p_buffer;

        /* Make room for i_sizelen after header */
        if(2 * (i_obu_offset + i_header) + i_sizelen < p_block->i_buffer)
        {
            /* move data to the left */
            p_block = block_Realloc(p_block, i_sizelen, p_block->i_buffer);
            if(p_block)
                memmove(p_block->p_buffer, &p_block->p_buffer[i_sizelen],
                        i_obu_offset + i_header);
        }
        else
        {
            /* move data to the right */
            p_block = block_Realloc(p_block, 0, p_block->i_buffer + i_sizelen);
            if(p_block)
            {
                const size_t i_off = i_obu_offset + i_header;
                memmove(&p_block->p_buffer[i_off + i_sizelen], &p_block->p_buffer[i_off],
                        p_block->i_buffer - i_off - i_sizelen);
            }
        }

        if(likely(p_block))
        {
            leb128_write(i_obu - i_header, &p_block->p_buffer[i_obu_offset + i_header]);
            p_block->p_buffer[i_obu_offset] |= 0x02;
        }

        break;
    }
    return p_block;
}

/*
    Restores TD OBU and last size field from MP4/MKV sample format
    to full AV1 low overhead format.

    AV1 ISOBMFF Mapping 2.4
    https://aomediacodec.github.io/av1-isobmff/#sampleformat
    Matroska/WebM
    https://github.com/Matroska-Org/matroska-specification/blob/master/codec/av1.md
*/
static inline block_t * AV1_Unpack_Sample(block_t *p_block)
{
    /* Restore last size field if missing */
    p_block = AV1_Unpack_Sample_ExpandSize(p_block);
    /* Reinsert removed TU: See av1-isobmff 2.4 */
    if(p_block &&
       (p_block->p_buffer[0] & 0x81) == 0 && /* reserved flags */
       (p_block->p_buffer[0] & 0x7A) != 0x12) /* no TEMPORAL_DELIMITER */
    {
        p_block = block_Realloc(p_block, 2, p_block->i_buffer);
        if(p_block)
        {
            p_block->p_buffer[0] = 0x12;
            p_block->p_buffer[1] = 0x00;
        }
    }
    return p_block;
}

#endif
