/*****************************************************************************
 * Copyright © 2010-2014 VideoLAN
 *
 * Authors: Thomas Guillem <thomas.guillem@gmail.com>
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

#include "hevc_nal.h"
#include "hxxx_nal.h"

#include <limits.h>

/* Computes size and does check the whole struct integrity */
static size_t get_hvcC_to_AnnexB_NAL_size( const uint8_t *p_buf, size_t i_buf )
{
    size_t i_total = 0;

    if( i_buf < HEVC_MIN_HVCC_SIZE )
        return 0;

    const uint8_t i_nal_length_size = (p_buf[21] & 0x03) + 1;
    if(i_nal_length_size == 3)
        return 0;

    const uint8_t i_num_array = p_buf[22];
    p_buf += 23; i_buf -= 23;

    for( uint8_t i = 0; i < i_num_array; i++ )
    {
        if(i_buf < 3)
            return 0;

        const uint16_t i_num_nalu = p_buf[1] << 8 | p_buf[2];
        p_buf += 3; i_buf -= 3;

        for( uint16_t j = 0; j < i_num_nalu; j++ )
        {
            if(i_buf < 2)
                return 0;

            const uint16_t i_nalu_length = p_buf[0] << 8 | p_buf[1];
            if(i_buf < i_nalu_length)
                return 0;

            i_total += i_nalu_length + i_nal_length_size;
            p_buf += i_nalu_length + 2;
            i_buf -= i_nalu_length + 2;
        }
    }

    return i_total;
}

uint8_t * hevc_hvcC_to_AnnexB_NAL( const uint8_t *p_buf, size_t i_buf,
                                   size_t *pi_result, uint8_t *pi_nal_length_size )
{
    *pi_result = get_hvcC_to_AnnexB_NAL_size( p_buf, i_buf ); /* Does all checks */
    if( *pi_result == 0 )
        return NULL;

    if( pi_nal_length_size )
        *pi_nal_length_size = (p_buf[21] & 0x03) + 1;

    uint8_t *p_ret;
    uint8_t *p_out_buf = p_ret = malloc( *pi_result );
    if( !p_out_buf )
        return NULL;

    const uint8_t i_num_array = p_buf[22];
    p_buf += 23;

    for( uint8_t i = 0; i < i_num_array; i++ )
    {
        const uint16_t i_num_nalu = p_buf[1] << 8 | p_buf[2];
        p_buf += 3;

        for( uint16_t j = 0; j < i_num_nalu; j++ )
        {
            const uint16_t i_nalu_length = p_buf[0] << 8 | p_buf[1];

            memcpy( p_out_buf, annexb_startcode4, 4 );
            memcpy( &p_out_buf[4], &p_buf[2], i_nalu_length );

            p_out_buf += 4 + i_nalu_length;
            p_buf += 2 + i_nalu_length;
        }
    }

    return p_ret;
}
