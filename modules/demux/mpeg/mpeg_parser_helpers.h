/*****************************************************************************
 * packetizer_helper.h: Packetizer helpers
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Denis Charmet <typx@videolan.org>
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
#ifndef MPEG_PARSER_HELPERS_H
#define MPEG_PARSER_HELPERS_H
#include <stdint.h>
#include <vlc_bits.h>

static inline void hevc_skip_profile_tiers_level( bs_t * bs, int32_t max_sub_layer_minus1 )
{
    uint8_t sub_layer_profile_present_flag[8];
    uint8_t sub_layer_level_present_flag[8];

    /* skipping useless fields of the VPS see https://www.itu.int/rec/dologin_pub.asp?lang=e&id=T-REC-H.265-201304-I!!PDF-E&type=item */
    bs_skip( bs, 2 + 1 + 5 + 32 + 1 + 1 + 1 + 1 + 44 + 8 );

    for( int32_t i = 0; i < max_sub_layer_minus1; i++ )
    {
        sub_layer_profile_present_flag[i] = bs_read1( bs );
        sub_layer_level_present_flag[i] = bs_read1( bs );
    }

    if(max_sub_layer_minus1 > 0)
        bs_skip( bs, (8 - max_sub_layer_minus1) * 2 );

    for( int32_t i = 0; i < max_sub_layer_minus1; i++ )
    {
        if( sub_layer_profile_present_flag[i] )
            bs_skip( bs, 2 + 1 + 5 + 32 + 1 + 1 + 1 + 1 + 44 );
        if( sub_layer_level_present_flag[i] )
            bs_skip( bs, 8 );
    }
}

static inline uint32_t read_ue( bs_t * bs )
{
    int32_t i = 0;

    while( bs_read1( bs ) == 0 && bs->p < bs->p_end && i < 32 )
        i++;

    return (1 << i) - 1 + bs_read( bs, i );
}


static inline size_t nal_decode(uint8_t * p_src, uint8_t * p_dst, size_t i_size)
{
    size_t j = 0;
    for (size_t i = 0; i < i_size; i++) {
        if (i < i_size - 3 &&
            p_src[i] == 0 && p_src[i+1] == 0 && p_src[i+2] == 3) {
            p_dst[j++] = 0;
            p_dst[j++] = 0;
            i += 2;
            continue;
        }
        p_dst[j++] = p_src[i];
    }
    return j;
}

#endif /*MPEG_PARSER_HELPERS_H*/
