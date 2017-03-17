/*****************************************************************************
 * h264_slice.c: h264 slice parser
 *****************************************************************************
 * Copyright (C) 2001-17 VLC authors and VideoLAN
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
#include <vlc_bits.h>
#include <vlc_block.h>

#include "h264_nal.h"
#include "h264_slice.h"
#include "hxxx_nal.h"

bool h264_decode_slice( const uint8_t *p_buffer, size_t i_buffer,
                        void (* get_sps_pps)(uint8_t, void *,
                                             const h264_sequence_parameter_set_t **,
                                             const h264_picture_parameter_set_t ** ),
                        void *priv, h264_slice_t *p_slice )
{
    int i_slice_type;
    h264_slice_init( p_slice );
    bs_t s;
    unsigned i_bitflow = 0;
    bs_init( &s, p_buffer, i_buffer );
    s.p_fwpriv = &i_bitflow;
    s.pf_forward = hxxx_bsfw_ep3b_to_rbsp;  /* Does the emulated 3bytes conversion to rbsp */

    /* nal unit header */
    bs_skip( &s, 1 );
    const uint8_t i_nal_ref_idc = bs_read( &s, 2 );
    const uint8_t i_nal_type = bs_read( &s, 5 );

    /* first_mb_in_slice */
    /* int i_first_mb = */ bs_read_ue( &s );

    /* slice_type */
    switch( (i_slice_type = bs_read_ue( &s )) )
    {
    case 0: case 5:
        p_slice->i_frame_type = BLOCK_FLAG_TYPE_P;
        break;
    case 1: case 6:
        p_slice->i_frame_type = BLOCK_FLAG_TYPE_B;
        break;
    case 2: case 7:
        p_slice->i_frame_type = BLOCK_FLAG_TYPE_I;
        break;
    case 3: case 8: /* SP */
        p_slice->i_frame_type = BLOCK_FLAG_TYPE_P;
        break;
    case 4: case 9:
        p_slice->i_frame_type = BLOCK_FLAG_TYPE_I;
        break;
    default:
        p_slice->i_frame_type = 0;
        break;
    }

    /* */
    p_slice->i_nal_type = i_nal_type;
    p_slice->i_nal_ref_idc = i_nal_ref_idc;

    p_slice->i_pic_parameter_set_id = bs_read_ue( &s );
    if( p_slice->i_pic_parameter_set_id > H264_PPS_ID_MAX )
        return false;

    const h264_sequence_parameter_set_t *p_sps;
    const h264_picture_parameter_set_t *p_pps;

    /* Bind matched/referred PPS and SPS */
    get_sps_pps( p_slice->i_pic_parameter_set_id, priv, &p_sps, &p_pps );
    if( !p_sps || !p_pps )
        return false;

    p_slice->i_frame_num = bs_read( &s, p_sps->i_log2_max_frame_num + 4 );

    if( !p_sps->frame_mbs_only_flag )
    {
        /* field_pic_flag */
        p_slice->i_field_pic_flag = bs_read( &s, 1 );
        if( p_slice->i_field_pic_flag )
            p_slice->i_bottom_field_flag = bs_read( &s, 1 );
    }

    if( p_slice->i_nal_type == H264_NAL_SLICE_IDR )
        p_slice->i_idr_pic_id = bs_read_ue( &s );

    p_slice->i_pic_order_cnt_type = p_sps->i_pic_order_cnt_type;
    if( p_sps->i_pic_order_cnt_type == 0 )
    {
        p_slice->i_pic_order_cnt_lsb = bs_read( &s, p_sps->i_log2_max_pic_order_cnt_lsb + 4 );
        if( p_pps->i_pic_order_present_flag && !p_slice->i_field_pic_flag )
            p_slice->i_delta_pic_order_cnt_bottom = bs_read_se( &s );
    }
    else if( (p_sps->i_pic_order_cnt_type == 1) &&
             (!p_sps->i_delta_pic_order_always_zero_flag) )
    {
        p_slice->i_delta_pic_order_cnt0 = bs_read_se( &s );
        if( p_pps->i_pic_order_present_flag && !p_slice->i_field_pic_flag )
            p_slice->i_delta_pic_order_cnt1 = bs_read_se( &s );
    }

    return true;
}
