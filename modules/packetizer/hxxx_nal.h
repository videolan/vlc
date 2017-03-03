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

/* Annex E: Colour primaries */
enum hxxx_colour_primaries
{
    HXXX_PRIMARIES_RESERVED0        = 0,
    HXXX_PRIMARIES_BT709            = 1,
    HXXX_PRIMARIES_UNSPECIFIED      = 2,
    HXXX_PRIMARIES_RESERVED3        = 3,
    HXXX_PRIMARIES_BT470M           = 4,
    HXXX_PRIMARIES_BT470BG          = 5,
    HXXX_PRIMARIES_BT601_525        = 6,
    HXXX_PRIMARIES_SMTPE_240M       = 7,
    HXXX_PRIMARIES_GENERIC_FILM     = 8,
    HXXX_PRIMARIES_BT2020           = 9,
    HXXX_PRIMARIES_SMPTE_ST_428     = 10,
};

static inline video_color_primaries_t
hxxx_colour_primaries_to_vlc( enum hxxx_colour_primaries i_colour )
{
    switch( i_colour )
    {
    case HXXX_PRIMARIES_BT470BG:
        return COLOR_PRIMARIES_BT601_625;

    case HXXX_PRIMARIES_BT601_525:
    case HXXX_PRIMARIES_SMTPE_240M:
        return COLOR_PRIMARIES_BT601_625;

    case HXXX_PRIMARIES_BT709:
        return COLOR_PRIMARIES_BT709;

    case HXXX_PRIMARIES_BT2020:
        return COLOR_PRIMARIES_BT2020;

    case HXXX_PRIMARIES_BT470M:
    case HXXX_PRIMARIES_RESERVED0:
    case HXXX_PRIMARIES_UNSPECIFIED:
    case HXXX_PRIMARIES_RESERVED3:
    case HXXX_PRIMARIES_GENERIC_FILM:
    case HXXX_PRIMARIES_SMPTE_ST_428:
    default:
        return COLOR_PRIMARIES_UNDEF;
    }
}

/* Annex E: Transfer characteristics */
enum hxxx_transfer_characteristics
{
    HXXX_TRANSFER_RESERVED0         = 0,
    HXXX_TRANSFER_BT709             = 1,
    HXXX_TRANSFER_UNSPECIFIED       = 2,
    HXXX_TRANSFER_RESERVED3         = 3,
    HXXX_TRANSFER_BT470M            = 4,
    HXXX_TRANSFER_BT470BG           = 5,
    HXXX_TRANSFER_BT601_525         = 6,
    HXXX_TRANSFER_SMTPE_240M        = 7,
    HXXX_TRANSFER_LINEAR            = 8,
    HXXX_TRANSFER_LOG               = 9,
    HXXX_TRANSFER_LOG_SQRT          = 10,
    HXXX_TRANSFER_IEC61966_2_4      = 11,
    HXXX_TRANSFER_BT1361            = 12,
    HXXX_TRANSFER_IEC61966_2_1      = 13,
    HXXX_TRANSFER_BT2020_V14        = 14,
    HXXX_TRANSFER_BT2020_V15        = 15,
    HXXX_TRANSFER_SMPTE_ST_2084     = 16,
    HXXX_TRANSFER_SMPTE_ST_428      = 17,
    HXXX_TRANSFER_ARIB_STD_B67      = 18,
};

static inline video_transfer_func_t
hxxx_transfer_characteristics_to_vlc( enum hxxx_transfer_characteristics i_transfer )
{
    switch( i_transfer )
    {
    case HXXX_TRANSFER_LINEAR:
        return TRANSFER_FUNC_LINEAR;

    case HXXX_TRANSFER_BT470M:
        return TRANSFER_FUNC_SRGB;

    case HXXX_TRANSFER_BT709:
    case HXXX_TRANSFER_BT601_525:
    case HXXX_TRANSFER_BT2020_V14:
    case HXXX_TRANSFER_BT2020_V15:
        return TRANSFER_FUNC_BT709;

    case HXXX_TRANSFER_SMPTE_ST_2084:
        return TRANSFER_FUNC_SMPTE_ST2084;

    case HXXX_TRANSFER_ARIB_STD_B67:
        return TRANSFER_FUNC_ARIB_B67;

    case HXXX_TRANSFER_RESERVED0:
    case HXXX_TRANSFER_UNSPECIFIED:
    case HXXX_TRANSFER_RESERVED3:
    case HXXX_TRANSFER_BT470BG:
    case HXXX_TRANSFER_SMTPE_240M:
    case HXXX_TRANSFER_LOG:
    case HXXX_TRANSFER_LOG_SQRT:
    case HXXX_TRANSFER_IEC61966_2_4:
    case HXXX_TRANSFER_BT1361:
    case HXXX_TRANSFER_IEC61966_2_1:
    case HXXX_TRANSFER_SMPTE_ST_428:
    default:
        return TRANSFER_FUNC_UNDEF;
    };
}

/* Annex E: Matrix coefficients */
enum hxxx_matrix_coeffs
{
    HXXX_MATRIX_IDENTITY            = 0,
    HXXX_MATRIX_BT709               = 1,
    HXXX_MATRIX_UNSPECIFIED         = 2,
    HXXX_MATRIX_RESERVED            = 3,
    HXXX_MATRIX_FCC                 = 4,
    HXXX_MATRIX_BT470BG             = 5,
    HXXX_MATRIX_BT601_525           = 6,
    HXXX_MATRIX_SMTPE_240M          = 7,
    HXXX_MATRIX_YCGCO               = 8,
    HXXX_MATRIX_BT2020_NCL          = 9,
    HXXX_MATRIX_BT2020_CL           = 10,
};

static inline video_color_space_t
hxxx_matrix_coeffs_to_vlc( enum hxxx_matrix_coeffs i_transfer )
{
    switch( i_transfer )
    {
    case HXXX_MATRIX_BT470BG:
    case HXXX_MATRIX_BT601_525:
        return COLOR_SPACE_BT601;

    case HXXX_MATRIX_BT709:
        return COLOR_SPACE_BT709;

    case HXXX_MATRIX_BT2020_NCL:
    case HXXX_MATRIX_BT2020_CL:
        return COLOR_SPACE_BT2020;

    case HXXX_MATRIX_IDENTITY:
    case HXXX_MATRIX_UNSPECIFIED:
    case HXXX_MATRIX_RESERVED:
    case HXXX_MATRIX_FCC:
    case HXXX_MATRIX_SMTPE_240M:
    case HXXX_MATRIX_YCGCO:
    default:
        return COLOR_SPACE_UNDEF;
    }
}

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

/* vlc_bits's bs_t forward callback for stripping emulation prevention three bytes */
static inline uint8_t *hxxx_bsfw_ep3b_to_rbsp( uint8_t *p, uint8_t *end, void *priv, size_t i_count )
{
    unsigned *pi_prev = (unsigned *) priv;
    for( size_t i=0; i<i_count; i++ )
    {
        if( ++p >= end )
            return p;

        *pi_prev = (*pi_prev << 1) | (!*p);

        if( *p == 0x03 &&
           ( p + 1 ) != end ) /* Never escape sequence if no next byte */
        {
            if( (*pi_prev & 0x06) == 0x06 )
            {
                ++p;
                *pi_prev = ((*pi_prev >> 1) << 1) | (!*p);
            }
        }
    }
    return p;
}

#if 0
/* Discards emulation prevention three bytes */
static inline uint8_t * hxxx_ep3b_to_rbsp(const uint8_t *p_src, size_t i_src, size_t *pi_ret)
{
    uint8_t *p_dst;
    if(!p_src || !(p_dst = malloc(i_src)))
        return NULL;

    size_t j = 0;
    for (size_t i = 0; i < i_src; i++) {
        if (i < i_src - 3 &&
            p_src[i] == 0 && p_src[i+1] == 0 && p_src[i+2] == 3) {
            p_dst[j++] = 0;
            p_dst[j++] = 0;
            i += 2;
            continue;
        }
        p_dst[j++] = p_src[i];
    }
    *pi_ret = j;
    return p_dst;
}
#endif

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
    if( popcount(i_nal_length_size) == 1 && i_nal_length_size <= 4 )
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

    size_t i_nal_size = 0;
    for( uint8_t i=0; i < p_ctx->i_nal_length_size ; i++ )
        i_nal_size = (i_nal_size << 8) | *p_ctx->p_head++;

    if( (ptrdiff_t) i_nal_size > p_ctx->p_tail - p_ctx->p_head )
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

    p_ctx->p_head = startcode_FindAnyAnnexB( p_ctx->p_head, p_ctx->p_tail );
    if( !p_ctx->p_head )
        return false;

    const uint8_t *p_end = startcode_FindAnyAnnexB( p_ctx->p_head + 3, p_ctx->p_tail );
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
