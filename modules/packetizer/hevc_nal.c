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

#include <vlc_bits.h>

#include <limits.h>

typedef uint8_t  nal_u1_t;
typedef uint8_t  nal_u2_t;
typedef uint8_t  nal_u3_t;
typedef uint8_t  nal_u4_t;
typedef uint8_t  nal_u5_t;
typedef uint8_t  nal_u6_t;
typedef uint8_t  nal_u7_t;
typedef uint8_t  nal_u8_t;
typedef int32_t  nal_se_t;
typedef uint32_t nal_ue_t;

typedef struct
{
    nal_u2_t profile_space;
    nal_u1_t tier_flag;
    nal_u5_t profile_idc;
    uint32_t profile_compatibility_flag; /* nal_u1_t * 32 */
    nal_u1_t progressive_source_flag;
    nal_u1_t interlaced_source_flag;
    nal_u1_t non_packed_constraint_flag;
    nal_u1_t frame_only_constraint_flag;
    struct
    {
        nal_u1_t max_12bit_constraint_flag;
        nal_u1_t max_10bit_constraint_flag;
        nal_u1_t max_8bit_constraint_flag;
        nal_u1_t max_422chroma_constraint_flag;
        nal_u1_t max_420chroma_constraint_flag;
        nal_u1_t max_monochrome_constraint_flag;
        nal_u1_t intra_constraint_flag;
        nal_u1_t one_picture_only_constraint_flag;
        nal_u1_t lower_bit_rate_constraint_flag;
    } idc4to7;
    struct
    {
        nal_u1_t inbld_flag;
    } idc1to5;
} hevc_inner_profile_tier_level_t;

#define HEVC_MAX_SUBLAYERS 8
typedef struct
{
    hevc_inner_profile_tier_level_t general;
    nal_u8_t general_level_idc;
    uint8_t  sublayer_profile_present_flag;  /* nal_u1_t * 8 */
    uint8_t  sublayer_level_present_flag;    /* nal_u1_t * 8 */
    hevc_inner_profile_tier_level_t sub_layer[HEVC_MAX_SUBLAYERS];
    nal_u8_t sub_layer_level_idc[HEVC_MAX_SUBLAYERS];
} hevc_profile_tier_level_t;

typedef struct
{
    nal_u4_t sps_video_parameter_set_id;
    nal_u3_t sps_max_sub_layers_minus1;
    nal_u1_t sps_temporal_id_nesting_flag;

    hevc_profile_tier_level_t profile_tier_level;

    nal_ue_t sps_seq_parameter_set_id;
    nal_ue_t chroma_format_idc;
    nal_u1_t separate_colour_plane_flag;

    nal_ue_t pic_width_in_luma_samples;
    nal_ue_t pic_height_in_luma_samples;

    nal_u1_t conformance_window_flag;
    struct
    {
    nal_ue_t left_offset;
    nal_ue_t right_offset;
    nal_ue_t top_offset;
    nal_ue_t bottom_offset;
    } conf_win;

    nal_ue_t bit_depth_luma_minus8;
    nal_ue_t bit_depth_chroma_minus8;
    nal_ue_t log2_max_pic_order_cnt_lsb_minus4;

    nal_u1_t sps_sub_layer_ordering_info_present_flag;
    struct
    {
    nal_ue_t dec_pic_buffering_minus1;
    nal_ue_t num_reorder_pics;
    nal_ue_t latency_increase_plus1;
    } sps_max[1 + HEVC_MAX_SUBLAYERS];

    nal_ue_t log2_min_luma_coding_block_size_minus3;
    nal_ue_t log2_diff_max_min_luma_coding_block_size;
    nal_ue_t log2_min_luma_transform_block_size_minus2;
    nal_ue_t log2_diff_max_min_luma_transform_block_size;

    /* incomplete */
} hevc_sequence_parameter_set_t;

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
    {
        *pi_result = 0;
        return NULL;
    }

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

static bool hevc_parse_inner_profile_tier_level_rbsp( bs_t *p_bs,
                                                      hevc_inner_profile_tier_level_t *p_in )
{
    if( bs_remain( p_bs ) < 88 )
        return false;

    p_in->profile_space = bs_read( p_bs, 2 );
    p_in->tier_flag = bs_read1( p_bs );
    p_in->profile_idc = bs_read( p_bs, 5 );
    p_in->profile_compatibility_flag = bs_read( p_bs, 32 );
    p_in->progressive_source_flag = bs_read1( p_bs );
    p_in->interlaced_source_flag = bs_read1( p_bs );
    p_in->non_packed_constraint_flag = bs_read1( p_bs );
    p_in->frame_only_constraint_flag = bs_read1( p_bs );

    if( ( p_in->profile_idc >= 4 && p_in->profile_idc <= 7 ) ||
        ( p_in->profile_compatibility_flag & 0x0F000000 ) )
    {
        p_in->idc4to7.max_12bit_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_10bit_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_8bit_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_422chroma_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_420chroma_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_monochrome_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.intra_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.one_picture_only_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.lower_bit_rate_constraint_flag = bs_read1( p_bs );
        (void) bs_read( p_bs, 2 );
    }
    else
    {
        (void) bs_read( p_bs, 11 );
    }
    (void) bs_read( p_bs, 32 );

    if( ( p_in->profile_idc >= 1 && p_in->profile_idc <= 5 ) ||
        ( p_in->profile_compatibility_flag & 0x7C000000 ) )
        p_in->idc1to5.inbld_flag = bs_read1( p_bs );
    else
        (void) bs_read1( p_bs );

    return true;
}

static bool hevc_parse_profile_tier_level_rbsp( bs_t *p_bs, bool profile_present,
                                                uint8_t max_num_sub_layers_minus1,
                                                hevc_profile_tier_level_t *p_ptl )
{
    if( profile_present && !hevc_parse_inner_profile_tier_level_rbsp( p_bs, &p_ptl->general ) )
        return false;

    if( bs_remain( p_bs ) < 8)
        return false;

    p_ptl->general_level_idc = bs_read( p_bs, 8 );

    if( max_num_sub_layers_minus1 > 0 )
    {
        if( bs_remain( p_bs ) < 16 )
            return false;

        for( uint8_t i=0; i< 8; i++ )
        {
            if( i < max_num_sub_layers_minus1 )
            {
                if( bs_read1( p_bs ) )
                    p_ptl->sublayer_profile_present_flag |= (0x80 >> i);
                if( bs_read1( p_bs ) )
                    p_ptl->sublayer_level_present_flag |= (0x80 >> i);
            }
            else
                bs_read( p_bs, 2 );
        }

        for( uint8_t i=0; i < max_num_sub_layers_minus1; i++ )
        {
            if( ( p_ptl->sublayer_profile_present_flag & (0x80 >> i) ) &&
                ! hevc_parse_inner_profile_tier_level_rbsp( p_bs, &p_ptl->sub_layer[i] ) )
                return false;

            if( p_ptl->sublayer_profile_present_flag & (0x80 >> i) )
            {
                if( bs_remain( p_bs ) < 8 )
                    return false;
                p_ptl->sub_layer_level_idc[i] = bs_read( p_bs, 8 );
            }
        }
    }

    return true;
}
