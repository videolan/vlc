/*****************************************************************************
 * hxxx_nal.h: Common helpers for AVC/HEVC NALU
 *****************************************************************************
 * Copyright (C) 2014-2015 VLC authors and VideoLAN
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
#ifndef HXXX_NAL_H
#define HXXX_NAL_H

#include <vlc_common.h>
#include <vlc_es.h>
#include "startcode_helper.h"

static const uint8_t  annexb_startcode4[] = { 0x00, 0x00, 0x00, 0x01 };
#define annexb_startcode3 (&annexb_startcode4[1])

/* strips any AnnexB startcode [0] 0 0 1 */
static inline bool hxxx_strip_AnnexB_startcode( const uint8_t **pp_data, size_t *pi_data )
{
    unsigned bitflow = 0;
    const uint8_t *p_data = *pp_data;
    size_t i_data = *pi_data;

    while( i_data && p_data[0] <= 1 )
    {
        bitflow = (bitflow << 1) | (!p_data[0]);
        p_data++;
        i_data--;
        if( !(bitflow & 0x01) )
        {
            if( (bitflow & 0x06) == 0x06 ) /* there was at least 2 leading zeros */
            {
                *pi_data = i_data;
                *pp_data = p_data;
                return true;
            }
            return false;
        }
    }
    return false;
}

/* Declarations */

typedef struct
{
    const uint8_t *p_head;
    const uint8_t *p_tail;
    uint8_t i_nal_length_size;
} hxxx_iterator_ctx_t;

static inline void hxxx_iterator_init( hxxx_iterator_ctx_t *p_ctx, const uint8_t *p_data, size_t i_data,
                                       uint8_t i_nal_length_size )
{
    p_ctx->p_head = p_data;
    p_ctx->p_tail = p_data + i_data;
    if( vlc_popcount(i_nal_length_size) == 1 && i_nal_length_size <= 4 )
        p_ctx->i_nal_length_size = i_nal_length_size;
    else
        p_ctx->i_nal_length_size = 0;
}

static inline bool hxxx_iterate_next( hxxx_iterator_ctx_t *p_ctx, const uint8_t **pp_start, size_t *pi_size )
{
    if( p_ctx->i_nal_length_size == 0 )
        return false;

    if( p_ctx->p_tail - p_ctx->p_head < p_ctx->i_nal_length_size )
        return false;

    uint32_t i_nal_size = 0;
    for( uint8_t i=0; i < p_ctx->i_nal_length_size ; i++ )
        i_nal_size = (i_nal_size << 8) | *p_ctx->p_head++;

    if( i_nal_size > (size_t)(p_ctx->p_tail - p_ctx->p_head) )
        return false;

    *pp_start = p_ctx->p_head;
    *pi_size = i_nal_size;
    p_ctx->p_head += i_nal_size;

    return true;
}

static inline bool hxxx_annexb_iterate_next( hxxx_iterator_ctx_t *p_ctx, const uint8_t **pp_start, size_t *pi_size )
{
    if( !p_ctx->p_head )
        return false;

    p_ctx->p_head = startcode_FindAnnexB( p_ctx->p_head, p_ctx->p_tail );
    if( !p_ctx->p_head )
        return false;

    const uint8_t *p_end = startcode_FindAnnexB( p_ctx->p_head + 3, p_ctx->p_tail );
    if( !p_end )
        p_end = p_ctx->p_tail;

    /* fix 3 to 4 startcode offset and strip any trailing zeros */
    while( p_end > p_ctx->p_head && p_end[-1] == 0 )
        p_end--;

    *pp_start = p_ctx->p_head;
    *pi_size = p_end - p_ctx->p_head;
    p_ctx->p_head = p_end;

    return hxxx_strip_AnnexB_startcode( pp_start, pi_size );
}

/* Takes any AnnexB NAL buffer and converts it to prefixed size (AVC/HEVC) */
block_t *hxxx_AnnexB_to_xVC( block_t *p_block, uint8_t i_nal_length_size );

#endif // HXXX_NAL_H
