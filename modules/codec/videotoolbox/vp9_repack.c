/*****************************************************************************
 * vp9_repack.c: VP9 superframe repacker
 *****************************************************************************
 * Copyright (C) 2023-2026 VideoLabs, VLC authors and VideoLAN
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_block.h>

#include "vp9_repack.h"

void vp9_repacker_Clean(struct vp9_repacker *r)
{
    block_ChainRelease(r->p_head);
}

void vp9_repacker_Init(struct vp9_repacker *r)
{
    r->p_head = NULL;
    r->pp_append = &r->p_head;
    r->count = 0;
    r->maxsize = 0;
    r->total = 0;
}

block_t * vp9_repacker_Push(struct vp9_repacker *r, block_t *block,
                            bool show_frame)
{
    if(block->p_buffer[block->i_buffer - 1] >> 5 == 0x06)
    {
        /* discard aggregated */
        vp9_repacker_Clean(r);
        vp9_repacker_Init(r);
        return block;
    }

    if(show_frame && r->p_head == NULL)
        return block;

    if(r->count == 8 || /* max possible index reached. something is wrong */
       SIZE_MAX - r->total < block->i_buffer)
    {
        vp9_repacker_Clean(r);
        vp9_repacker_Init(r);
    }

    r->count++;
    r->total += block->i_buffer;
    if(block->i_buffer > r->maxsize)
        r->maxsize = block->i_buffer;
    block_ChainLastAppend(&r->pp_append, block);

    if(show_frame)
    {
        /* make superframe from aggregated content B.2 */
        uint8_t bpf = 1;
        for(size_t maxsize = r->maxsize >> 8; maxsize; maxsize >>= 8)
            bpf++;
        block_t *idxblock = block_Alloc(1 + r->count * bpf + 1);
        if(!idxblock)
        {
            vp9_repacker_Clean(r);
            vp9_repacker_Init(r);
            return NULL;
        }
        /* create index B.2.1 */
        /* set superframe headers B.2.2 */
        idxblock->p_buffer[0] = 0xC0 | ((bpf - 1) << 3) | (r->count - 1);
        idxblock->p_buffer[idxblock->i_buffer - 1] = idxblock->p_buffer[0];
        /* fill frames size index */
        uint8_t *p = &idxblock->p_buffer[1];
        for(block = r->p_head; block; block = block->p_next)
        {
            uint8_t buf[8];
            SetQWLE(buf, block->i_buffer);
            memcpy(p, buf, bpf);
            p += bpf;
        }
        /* merge */
        block_ChainLastAppend(&r->pp_append, idxblock);
        block = block_ChainGather(r->p_head);
        if(!block)
            vp9_repacker_Clean(r);
        vp9_repacker_Init(r);
        return block;
    }

    return NULL;
}
