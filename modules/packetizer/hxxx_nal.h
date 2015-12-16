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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

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

static inline uint8_t * hxxx_AnnexB_NAL_to_rbsp(const uint8_t *p_src, size_t i_src, size_t *pi_ret)
{
    if(!hxxx_strip_AnnexB_startcode(&p_src, &i_src))
        return NULL;
    return hxxx_ep3b_to_rbsp(p_src, i_src, pi_ret);
}

static inline uint8_t * hxxx_xvc1_NAL_to_rbsp(const uint8_t *p_src, size_t i_src,
                                              uint8_t i_nal_length_size, size_t *pi_ret)
{
    if(i_src < i_nal_length_size)
        return NULL;
    p_src += i_nal_length_size;
    i_src -= i_nal_length_size;
    return hxxx_ep3b_to_rbsp(p_src, i_src, pi_ret);
}

#endif // HXXX_NAL_H
