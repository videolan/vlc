/*****************************************************************************
 * vlc_boxes.h : Boxes/Atoms handling helpers
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2003, 2006, 2015 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin at videolan dot org>
 *          Rafaël Carré <funman at videolan dot org>
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
#ifndef VLC_BOXES_H
#define VLC_BOXES_H

#include <vlc_common.h>
#include <vlc_block.h>

/**
 * \file
 * This file defines functions, structures for handling boxes/atoms in vlc
 */

typedef struct bo_t
{
    block_t     *b;
    size_t      basesize;
} bo_t;

static inline bool bo_init(bo_t *p_bo, int i_size)
{
    p_bo->b = block_Alloc(i_size);
    if (p_bo->b == NULL)
        return false;

    p_bo->b->i_buffer = 0;
    p_bo->basesize = i_size;

    return true;
}

static inline void bo_deinit(bo_t *p_bo)
{
    if(p_bo->b)
        block_Release(p_bo->b);
}

static inline void bo_free(bo_t *p_bo)
{
    if(!p_bo)
        return;
    bo_deinit(p_bo);
    free(p_bo);
}

static inline int bo_extend(bo_t *p_bo, size_t i_total)
{
    if(!p_bo->b)
        return false;
    const size_t i_size = p_bo->b->i_size - (p_bo->b->p_buffer - p_bo->b->p_start);
    if (i_total >= i_size)
    {
        int i_growth = p_bo->basesize;
        while(i_total >= i_size + i_growth)
            i_growth += p_bo->basesize;

        int i = p_bo->b->i_buffer; /* Realloc would set payload size == buffer size */
        p_bo->b = block_Realloc(p_bo->b, 0, i_size + i_growth);
        if (!p_bo->b)
            return false;
        p_bo->b->i_buffer = i;
    }
    return true;
}

#define BO_SET_DECL_S(func, handler, type) static inline bool func(bo_t *p_bo, size_t i_offset, type val)\
    {\
        if (!bo_extend(p_bo, i_offset + sizeof(type)))\
            return false;\
        handler(&p_bo->b->p_buffer[i_offset], val);\
        return true;\
    }

#define BO_ADD_DECL_S(func, handler, type) static inline bool func(bo_t *p_bo, type val)\
    {\
        if(!p_bo->b || !handler(p_bo, p_bo->b->i_buffer, val))\
            return false;\
        p_bo->b->i_buffer += sizeof(type);\
        return true;\
    }

#define BO_FUNC_DECL(suffix, handler, type ) \
    BO_SET_DECL_S( bo_set_ ## suffix ## be, handler ## BE, type )\
    BO_SET_DECL_S( bo_set_ ## suffix ## le, handler ## LE, type )\
    BO_ADD_DECL_S( bo_add_ ## suffix ## be, bo_set_ ## suffix ## be, type )\
    BO_ADD_DECL_S( bo_add_ ## suffix ## le, bo_set_ ## suffix ## le, type )

static inline bool bo_set_8(bo_t *p_bo, size_t i_offset, uint8_t i)
{
    if (!bo_extend(p_bo, i_offset + 1))
        return false;
    p_bo->b->p_buffer[i_offset] = i;
    return true;
}

static inline bool bo_add_8(bo_t *p_bo, uint8_t i)
{
    if(!p_bo->b || !bo_set_8( p_bo, p_bo->b->i_buffer, i ))
        return false;
    p_bo->b->i_buffer++;
    return true;
}

/* declares all bo_[set,add]_[16,32,64] */
BO_FUNC_DECL( 16, SetW,  uint16_t )
BO_FUNC_DECL( 32, SetDW, uint32_t )
BO_FUNC_DECL( 64, SetQW, uint64_t )

#undef BO_FUNC_DECL
#undef BO_SET_DECL_S
#undef BO_ADD_DECL_S

static inline bool bo_add_24be(bo_t *p_bo, uint32_t i)
{
    if(!p_bo->b || !bo_extend(p_bo, p_bo->b->i_buffer + 3))
        return false;
    p_bo->b->p_buffer[p_bo->b->i_buffer++] = ((i >> 16) & 0xff);
    p_bo->b->p_buffer[p_bo->b->i_buffer++] = ((i >> 8) & 0xff);
    p_bo->b->p_buffer[p_bo->b->i_buffer++] = (i & 0xff);
    return true;
}

static inline void bo_swap_32be (bo_t *p_bo, size_t i_pos, uint32_t i)
{
    if (!p_bo->b || p_bo->b->i_buffer < i_pos + 4)
        return;
    p_bo->b->p_buffer[i_pos    ] = (i >> 24)&0xff;
    p_bo->b->p_buffer[i_pos + 1] = (i >> 16)&0xff;
    p_bo->b->p_buffer[i_pos + 2] = (i >>  8)&0xff;
    p_bo->b->p_buffer[i_pos + 3] = (i      )&0xff;
}

static inline bool bo_add_mem(bo_t *p_bo, size_t i_size, const void *p_mem)
{
    if(!p_bo->b || !bo_extend(p_bo, p_bo->b->i_buffer + i_size))
        return false;
    memcpy(&p_bo->b->p_buffer[p_bo->b->i_buffer], p_mem, i_size);
    p_bo->b->i_buffer += i_size;
    return true;
}

static inline size_t bo_size(const bo_t *p_bo)
{
    return (p_bo->b) ? p_bo->b->i_buffer : 0;
}

#define bo_add_fourcc(p_bo, fcc) bo_add_mem(p_bo, 4, fcc)

#endif // VLC_BOXES_H
