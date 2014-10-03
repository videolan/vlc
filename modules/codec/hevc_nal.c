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

#include <limits.h>

/* Inspired by libavcodec/hevc.c */
int convert_hevc_nal_units(decoder_t *p_dec, const uint8_t *p_buf,
                           uint32_t i_buf_size, uint8_t *p_out_buf,
                           uint32_t i_out_buf_size, uint32_t *p_sps_pps_size,
                           uint32_t *p_nal_size)
{
    int i, num_arrays;
    const uint8_t *p_end = p_buf + i_buf_size;
    uint32_t i_sps_pps_size = 0;

    if( i_buf_size <= 3 || ( !p_buf[0] && !p_buf[1] && p_buf[2] <= 1 ) )
        return VLC_EGENERIC;

    if( p_end - p_buf < 23 )
    {
        msg_Err( p_dec, "Input Metadata too small" );
        return VLC_ENOMEM;
    }

    p_buf += 21;

    if( p_nal_size )
        *p_nal_size = (*p_buf & 0x03) + 1;
    p_buf++;

    num_arrays = *p_buf++;

    for( i = 0; i < num_arrays; i++ )
    {
        int type, cnt, j;

        if( p_end - p_buf < 3 )
        {
            msg_Err( p_dec, "Input Metadata too small" );
            return VLC_ENOMEM;
        }
        type = *(p_buf++) & 0x3f;
        VLC_UNUSED(type);

        cnt = p_buf[0] << 8 | p_buf[1];
        p_buf += 2;

        for( j = 0; j < cnt; j++ )
        {
            int i_nal_size;

            if( p_end - p_buf < 2 )
            {
                msg_Err( p_dec, "Input Metadata too small" );
                return VLC_ENOMEM;
            }
            
            i_nal_size = p_buf[0] << 8 | p_buf[1];
            p_buf += 2;

            if( i_nal_size < 0 || p_end - p_buf < i_nal_size )
            {
                msg_Err( p_dec, "NAL unit size does not match Input Metadata size" );
                return VLC_ENOMEM;
            }

            if( i_sps_pps_size + 4 + i_nal_size > i_out_buf_size )
            {
                msg_Err( p_dec, "Output buffer too small" );
                return VLC_ENOMEM;
            }

            p_out_buf[i_sps_pps_size++] = 0;
            p_out_buf[i_sps_pps_size++] = 0;
            p_out_buf[i_sps_pps_size++] = 0;
            p_out_buf[i_sps_pps_size++] = 1;

            memcpy(p_out_buf + i_sps_pps_size, p_buf, i_nal_size);
            p_buf += i_nal_size;

            i_sps_pps_size += i_nal_size;
        }
    }

    *p_sps_pps_size = i_sps_pps_size;

    return VLC_SUCCESS;
}
