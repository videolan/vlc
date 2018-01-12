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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "hevc_nal.h"
#include "hxxx_nal.h"

#include <vlc_common.h>
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

#define HEVC_MAX_SHORT_TERM_REF_PIC_SET 65
#define HEVC_MAX_LONG_TERM_REF_PIC_SET 33

typedef struct
{
    unsigned num_delta_pocs;
} hevc_short_term_ref_pic_set_t;

typedef struct
{
    nal_u1_t aspect_ratio_info_present_flag;
    struct
    {
        nal_u8_t aspect_ratio_idc;
        uint16_t sar_width;
        uint16_t sar_height;
    } ar;
    nal_u1_t overscan_info_present_flag;
    nal_u1_t overscan_appropriate_flag;

    nal_u1_t video_signal_type_present_flag;
    struct
    {
        nal_u3_t video_format;
        nal_u1_t video_full_range_flag;
        nal_u1_t colour_description_present_flag;
        struct
        {
            nal_u8_t colour_primaries;
            nal_u8_t transfer_characteristics;
            nal_u8_t matrix_coeffs;
        } colour;
    } vs;

    nal_u1_t chroma_loc_info_present_flag;
    struct
    {
        nal_ue_t sample_loc_type_top_field;
        nal_ue_t sample_loc_type_bottom_field;
    } chroma;

    nal_u1_t neutral_chroma_indication_flag;
    nal_u1_t field_seq_flag;
    nal_u1_t frame_field_info_present_flag;

    nal_u1_t default_display_window_flag;
    struct
    {
        nal_ue_t win_left_offset;
        nal_ue_t win_right_offset;
        nal_ue_t win_top_offset;
        nal_ue_t win_bottom_offset;
    } def_disp;

    nal_u1_t vui_timing_info_present_flag;
    struct
    {
        uint32_t vui_num_units_in_tick;
        uint32_t vui_time_scale;
        /* incomplete */
    } timing;

    /* incomplete */
} hevc_vui_parameters_t;

struct hevc_video_parameter_set_t
{
    nal_u4_t vps_video_parameter_set_id;
    nal_u1_t vps_base_layer_internal_flag;
    nal_u1_t vps_base_layer_available_flag;
    nal_u6_t vps_max_layers_minus1;
    nal_u3_t vps_max_sub_layers_minus1;
    nal_u1_t vps_temporal_id_nesting_flag;

    hevc_profile_tier_level_t profile_tier_level;

    nal_u1_t vps_sub_layer_ordering_info_present_flag;
    struct
    {
        nal_ue_t dec_pic_buffering_minus1;
        nal_ue_t num_reorder_pics;
        nal_ue_t max_latency_increase_plus1;
    } vps_max[1 + HEVC_MAX_SUBLAYERS];

    nal_u6_t vps_max_layer_id;
    nal_ue_t vps_num_layer_set_minus1;
    // layer_id_included_flag; read but discarded

    nal_u1_t vps_timing_info_present_flag;
    uint32_t vps_num_units_in_tick;
    uint32_t vps_time_scale;

    /* incomplete */
};

struct hevc_sequence_parameter_set_t
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
    nal_ue_t max_transform_hierarchy_depth_inter;
    nal_ue_t max_transform_hierarchy_depth_intra;
    nal_u1_t scaling_list_enabled;
    nal_u1_t sps_scaling_list_data_present_flag;
    // scaling_list_data; read but discarded

    nal_u1_t amp_enabled_flag;
    nal_u1_t sample_adaptive_offset_enabled_flag;

    nal_u1_t pcm_enabled_flag;
    nal_u4_t pcm_sample_bit_depth_luma_minus1;
    nal_u4_t pcm_sample_bit_depth_chroma_minus1;
    nal_ue_t log2_min_pcm_luma_coding_block_size_minus3;
    nal_ue_t log2_diff_max_min_pcm_luma_coding_block_size;
    nal_u1_t pcm_loop_filter_disabled_flag;

    nal_ue_t num_short_term_ref_pic_sets;
    // st_ref_pic_set

    nal_u1_t long_term_ref_pics_present_flag;
    nal_ue_t num_long_term_ref_pics_sps;
    //

    nal_u1_t sps_temporal_mvp_enabled_flag;
    nal_u1_t strong_intra_smoothing_enabled_flag;

    nal_u1_t vui_parameters_present_flag;
    hevc_vui_parameters_t vui;
    /* incomplete */
};

struct hevc_picture_parameter_set_t
{
    nal_ue_t pps_pic_parameter_set_id;
    nal_ue_t pps_seq_parameter_set_id;
    nal_u1_t dependent_slice_segments_enabled_flag;
    nal_u1_t output_flag_present_flag;
    nal_u3_t num_extra_slice_header_bits;
    nal_u1_t sign_data_hiding_enabled_flag;
    nal_u1_t cabac_init_present_flag;
    nal_ue_t num_ref_idx_l0_default_active_minus1;
    nal_ue_t num_ref_idx_l1_default_active_minus1;
    nal_se_t init_qp_minus26;
    nal_u1_t constrained_intra_pred_flag;
    nal_u1_t transform_skip_enabled_flag;

    nal_u1_t cu_qp_delta_enabled_flag;
    nal_ue_t diff_cu_qp_delta_depth;

    nal_se_t pps_cb_qp_offset;
    nal_se_t pps_cr_qp_offset;
    nal_u1_t pic_slice_level_chroma_qp_offsets_present_flag;
    nal_u1_t weighted_pred_flag;
    nal_u1_t weighted_bipred_flag;
    nal_u1_t transquant_bypass_enable_flag;

    nal_u1_t tiles_enabled_flag;
    nal_u1_t entropy_coding_sync_enabled_flag;
    nal_ue_t num_tile_columns_minus1;
    nal_ue_t num_tile_rows_minus1;
    nal_u1_t uniform_spacing_flag;
    // nal_ue_t *p_column_width_minus1; read but discarded
    // nal_ue_t *p_row_height_minus1; read but discarded
    nal_u1_t loop_filter_across_tiles_enabled_flag;

    nal_u1_t pps_loop_filter_across_slices_enabled_flag;

    nal_u1_t deblocking_filter_control_present_flag;
    nal_u1_t deblocking_filter_override_enabled_flag;
    nal_u1_t pps_deblocking_filter_disabled_flag;
    nal_se_t pps_beta_offset_div2;
    nal_se_t pps_tc_offset_div2;

    nal_u1_t scaling_list_data_present_flag;
    // scaling_list_data; read but discarded

    nal_u1_t lists_modification_present_flag;
    nal_ue_t log2_parallel_merge_level_minus2;
    nal_u1_t slice_header_extension_present_flag;

    nal_u1_t pps_extension_present_flag;
    nal_u1_t pps_range_extension_flag;
    nal_u1_t pps_multilayer_extension_flag;
    nal_u1_t pps_3d_extension_flag;
    nal_u5_t pps_extension_5bits;
    /* incomplete */

};

struct hevc_slice_segment_header_t
{
    nal_u6_t nal_type;
    nal_u6_t nuh_layer_id;
    nal_u3_t temporal_id_plus1;
    nal_u1_t first_slice_segment_in_pic_flag;
    nal_u1_t no_output_of_prior_pics_flag;
    nal_ue_t slice_pic_parameter_set_id;
    nal_u1_t dependent_slice_segment_flag;
    // slice_segment_address; read but discarded
    nal_ue_t slice_type;
    nal_u1_t pic_output_flag;

    uint32_t pic_order_cnt_lsb;
    /* incomplete */

};

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
            if(i_buf < (size_t)i_nalu_length + 2)
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
        *pi_nal_length_size = hevc_getNALLengthSize( p_buf );

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

static bool hevc_parse_scaling_list_rbsp( bs_t *p_bs )
{
    if( bs_remain( p_bs ) < 16 )
        return false;

    for( int i=0; i<4; i++ )
    {
        for( int j=0; j<6; j += (i == 3) ? 3 : 1 )
        {
            if( bs_read1( p_bs ) == 0 )
                bs_read_ue( p_bs );
            else
            {
                unsigned nextCoef = 8;
                unsigned coefNum = __MIN( 64, (1 << (4 + (i << 1))));
                if( i > 1 )
                {
                    nextCoef = bs_read_se( p_bs ) + 8;
                }
                for( unsigned k=0; k<coefNum; k++ )
                {
                    nextCoef = ( nextCoef + bs_read_se( p_bs ) + 256 ) % 256;
                }
            }
        }
    }

    return true;
}

static bool hevc_parse_vui_parameters_rbsp( bs_t *p_bs, hevc_vui_parameters_t *p_vui )
{
    if( bs_remain( p_bs ) < 10 )
        return false;

    p_vui->aspect_ratio_info_present_flag = bs_read1( p_bs );
    if( p_vui->aspect_ratio_info_present_flag )
    {
        p_vui->ar.aspect_ratio_idc = bs_read( p_bs, 8 );
        if( p_vui->ar.aspect_ratio_idc == 0xFF ) //HEVC_SAR__IDC_EXTENDED_SAR )
        {
            p_vui->ar.sar_width = bs_read( p_bs, 16 );
            p_vui->ar.sar_height = bs_read( p_bs, 16 );
        }
    }

    p_vui->overscan_info_present_flag = bs_read1( p_bs );
    if( p_vui->overscan_info_present_flag )
        p_vui->overscan_appropriate_flag = bs_read1( p_bs );

    p_vui->video_signal_type_present_flag = bs_read1( p_bs );
    if( p_vui->video_signal_type_present_flag )
    {
        p_vui->vs.video_format = bs_read( p_bs, 3 );
        p_vui->vs.video_full_range_flag = bs_read1( p_bs );
        p_vui->vs.colour_description_present_flag = bs_read1( p_bs );
        if( p_vui->vs.colour_description_present_flag )
        {
            p_vui->vs.colour.colour_primaries = bs_read( p_bs, 8 );
            p_vui->vs.colour.transfer_characteristics = bs_read( p_bs, 8 );
            p_vui->vs.colour.matrix_coeffs = bs_read( p_bs, 8 );
        }
        else
        {
            p_vui->vs.colour.colour_primaries = HXXX_PRIMARIES_UNSPECIFIED;
            p_vui->vs.colour.transfer_characteristics = HXXX_TRANSFER_UNSPECIFIED;
            p_vui->vs.colour.matrix_coeffs = HXXX_MATRIX_UNSPECIFIED;
        }
    }

    p_vui->chroma_loc_info_present_flag = bs_read1( p_bs );
    if( p_vui->chroma_loc_info_present_flag )
    {
        p_vui->chroma.sample_loc_type_top_field = bs_read_ue( p_bs );
        p_vui->chroma.sample_loc_type_bottom_field = bs_read_ue( p_bs );
    }

    p_vui->neutral_chroma_indication_flag = bs_read1( p_bs );
    p_vui->field_seq_flag = bs_read1( p_bs );
    p_vui->frame_field_info_present_flag = bs_read1( p_bs );

    p_vui->default_display_window_flag = bs_read1( p_bs );
    if( p_vui->default_display_window_flag )
    {
        p_vui->def_disp.win_left_offset = bs_read_ue( p_bs );
        p_vui->def_disp.win_right_offset = bs_read_ue( p_bs );
        p_vui->def_disp.win_top_offset = bs_read_ue( p_bs );
        p_vui->def_disp.win_bottom_offset = bs_read_ue( p_bs );
    }

    p_vui->vui_timing_info_present_flag = bs_read1( p_bs );
    if( p_vui->vui_timing_info_present_flag )
    {
        p_vui->timing.vui_num_units_in_tick =  bs_read( p_bs, 32 );
        p_vui->timing.vui_time_scale =  bs_read( p_bs, 32 );

        if( bs_remain( p_bs ) < 3 )
            return false;
    }
    /* incomplete */

    if( bs_remain( p_bs ) < 1 ) /* late fail */
        return false;

    return true;
}

/* Shortcut for retrieving vps/sps/pps id */
bool hevc_get_xps_id(const uint8_t *p_buf, size_t i_buf, uint8_t *pi_id)
{
    if(i_buf < 3)
        return false;
    /* No need to lookup convert from emulation for that data */
    uint8_t i_nal_type = hevc_getNALType(p_buf);
    bs_t bs;
    bs_init(&bs, &p_buf[2], i_buf - 2);
    if(i_nal_type == HEVC_NAL_PPS)
    {
        *pi_id = bs_read_ue( &bs );
        if(*pi_id > HEVC_PPS_ID_MAX)
            return false;
    }
    else
    {
        *pi_id = bs_read( &bs, 4 );
        if(i_nal_type == HEVC_NAL_SPS)
        {
            if(*pi_id > HEVC_SPS_ID_MAX)
                return false;
        }
        else if(*pi_id > HEVC_VPS_ID_MAX)
            return false;
    }
    return true;
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

static bool hevc_parse_video_parameter_set_rbsp( bs_t *p_bs,
                                                 hevc_video_parameter_set_t *p_vps )
{
    if( bs_remain( p_bs ) < 134 )
        return false;

    p_vps->vps_video_parameter_set_id = bs_read( p_bs, 4 );
    p_vps->vps_base_layer_internal_flag = bs_read1( p_bs );
    p_vps->vps_base_layer_available_flag = bs_read1( p_bs );
    p_vps->vps_max_layers_minus1 = bs_read( p_bs, 6 );
    p_vps->vps_max_sub_layers_minus1 = bs_read( p_bs, 3 );
    p_vps->vps_temporal_id_nesting_flag = bs_read1( p_bs );
    bs_skip( p_bs, 16 );

    if( !hevc_parse_profile_tier_level_rbsp( p_bs, true, p_vps->vps_max_sub_layers_minus1,
                                            &p_vps->profile_tier_level ) )
        return false;

    p_vps->vps_sub_layer_ordering_info_present_flag = bs_read1( p_bs );
    for( unsigned i= (p_vps->vps_sub_layer_ordering_info_present_flag ?
                      0 : p_vps->vps_max_sub_layers_minus1);
         i<= p_vps->vps_max_sub_layers_minus1; i++ )
    {
        p_vps->vps_max[i].dec_pic_buffering_minus1 = bs_read_ue( p_bs );
        p_vps->vps_max[i].num_reorder_pics = bs_read_ue( p_bs );
        p_vps->vps_max[i].max_latency_increase_plus1 = bs_read_ue( p_bs );
    }
    if( bs_remain( p_bs ) < 10 )
        return false;

    p_vps->vps_max_layer_id = bs_read( p_bs, 6 );
    p_vps->vps_num_layer_set_minus1 = bs_read_ue( p_bs );
    // layer_id_included_flag; read but discarded
    bs_skip( p_bs, p_vps->vps_num_layer_set_minus1 * (p_vps->vps_max_layer_id + 1) );
    if( bs_remain( p_bs ) < 2 )
        return false;

    p_vps->vps_timing_info_present_flag = bs_read1( p_bs );
    if( p_vps->vps_timing_info_present_flag )
    {
        p_vps->vps_num_units_in_tick = bs_read( p_bs, 32 );
        p_vps->vps_time_scale = bs_read( p_bs, 32 );
    }
    /* parsing incomplete */

    if( bs_remain( p_bs ) < 1 )
        return false;

    return true;
}

void hevc_rbsp_release_vps( hevc_video_parameter_set_t *p_vps )
{
    free( p_vps );
}

#define IMPL_hevc_generic_decode( name, hevctype, decode, release ) \
    hevctype * name( const uint8_t *p_buf, size_t i_buf, bool b_escaped ) \
    { \
        hevctype *p_hevctype = calloc(1, sizeof(hevctype)); \
        if(likely(p_hevctype)) \
        { \
            bs_t bs; \
            bs_init( &bs, p_buf, i_buf ); \
            unsigned i_bitflow = 0; \
            if( b_escaped ) \
            { \
                bs.p_fwpriv = &i_bitflow; \
                bs.pf_forward = hxxx_bsfw_ep3b_to_rbsp;  /* Does the emulated 3bytes conversion to rbsp */ \
            } \
            else (void) i_bitflow;\
            bs_skip( &bs, 7 ); /* nal_unit_header */ \
            uint8_t i_nuh_layer_id = bs_read( &bs, 6 ); \
            bs_skip( &bs, 3 ); /* !nal_unit_header */ \
            if( i_nuh_layer_id > 62 || !decode( &bs, p_hevctype ) ) \
            { \
                release( p_hevctype ); \
                p_hevctype = NULL; \
            } \
        } \
        return p_hevctype; \
    }

IMPL_hevc_generic_decode( hevc_decode_vps, hevc_video_parameter_set_t,
                          hevc_parse_video_parameter_set_rbsp, hevc_rbsp_release_vps )

static bool hevc_parse_st_ref_pic_set( bs_t *p_bs, unsigned stRpsIdx,
                                       unsigned num_short_term_ref_pic_sets,
                                       hevc_short_term_ref_pic_set_t *p_sets )
{
    if( stRpsIdx && bs_read1( p_bs ) ) /* Interref pic set prediction flag */
    {
        nal_ue_t delta_idx_minus_1 = 0;
        if( stRpsIdx == num_short_term_ref_pic_sets )
        {
            delta_idx_minus_1 = bs_read_ue( p_bs );
            if( delta_idx_minus_1 >= stRpsIdx )
                return false;
        }
        if(delta_idx_minus_1 == stRpsIdx)
            return false;

        nal_u1_t delta_rps_sign = bs_read1( p_bs );
        nal_ue_t abs_delta_rps_minus1 = bs_read_ue( p_bs );
        unsigned RefRpsIdx = stRpsIdx - delta_idx_minus_1 - 1;
        int deltaRps = ( 1 - ( delta_rps_sign << 1 ) ) * ( abs_delta_rps_minus1 + 1 );
        VLC_UNUSED(deltaRps);

        unsigned numDeltaPocs = p_sets[RefRpsIdx].num_delta_pocs;
        p_sets[stRpsIdx].num_delta_pocs = 0;
        for( unsigned j=0; j<= numDeltaPocs; j++ )
        {
            if( ! bs_read1( p_bs ) ) /* used_by_curr_pic_flag */
            {
                if( bs_read1( p_bs ) ) /* use_delta_flag */
                    p_sets[stRpsIdx].num_delta_pocs++;
            }
            else
                p_sets[stRpsIdx].num_delta_pocs++;
        }
    }
    else
    {
        nal_ue_t num_negative_pics = bs_read_ue( p_bs );
        nal_ue_t num_positive_pics = bs_read_ue( p_bs );
        if( bs_remain( p_bs ) < ((int64_t)num_negative_pics + num_positive_pics) * 2 )
            return false;
        for(unsigned int i=0; i<num_negative_pics; i++)
        {
            (void) bs_read_ue( p_bs ); /* delta_poc_s0_minus1 */
            (void) bs_read1( p_bs ); /* used_by_current_pic_s0_flag */
        }
        for(unsigned int i=0; i<num_positive_pics; i++)
        {
            (void) bs_read_ue( p_bs ); /* delta_poc_s1_minus1 */
            (void) bs_read1( p_bs ); /* used_by_current_pic_s1_flag */
        }
        p_sets[stRpsIdx].num_delta_pocs = num_positive_pics + num_negative_pics;
    }

    return true;
}

static bool hevc_parse_sequence_parameter_set_rbsp( bs_t *p_bs,
                                                    hevc_sequence_parameter_set_t *p_sps )
{
    p_sps->sps_video_parameter_set_id = bs_read( p_bs, 4 );
    p_sps->sps_max_sub_layers_minus1 = bs_read( p_bs, 3 );
    p_sps->sps_temporal_id_nesting_flag = bs_read1( p_bs );
    if( !hevc_parse_profile_tier_level_rbsp( p_bs, true, p_sps->sps_max_sub_layers_minus1,
                                            &p_sps->profile_tier_level ) )
        return false;

    if( bs_remain( p_bs ) < 1 )
        return false;

    p_sps->sps_seq_parameter_set_id = bs_read_ue( p_bs );
    if( p_sps->sps_seq_parameter_set_id > HEVC_SPS_ID_MAX )
        return false;

    p_sps->chroma_format_idc = bs_read_ue( p_bs );
    if( p_sps->chroma_format_idc == 3 )
        p_sps->separate_colour_plane_flag = bs_read1( p_bs );
    p_sps->pic_width_in_luma_samples = bs_read_ue( p_bs );
    p_sps->pic_height_in_luma_samples = bs_read_ue( p_bs );
    if( !p_sps->pic_width_in_luma_samples || !p_sps->pic_height_in_luma_samples )
        return false;

    p_sps->conformance_window_flag = bs_read1( p_bs );
    if( p_sps->conformance_window_flag )
    {
        p_sps->conf_win.left_offset = bs_read_ue( p_bs );
        p_sps->conf_win.right_offset = bs_read_ue( p_bs );
        p_sps->conf_win.top_offset = bs_read_ue( p_bs );
        p_sps->conf_win.bottom_offset = bs_read_ue( p_bs );
    }

    p_sps->bit_depth_luma_minus8 = bs_read_ue( p_bs );
    p_sps->bit_depth_chroma_minus8 = bs_read_ue( p_bs );
    p_sps->log2_max_pic_order_cnt_lsb_minus4 = bs_read_ue( p_bs );

    p_sps->sps_sub_layer_ordering_info_present_flag = bs_read1( p_bs );
    for( uint8_t i=(p_sps->sps_sub_layer_ordering_info_present_flag ? 0 : p_sps->sps_max_sub_layers_minus1);
         i <= p_sps->sps_max_sub_layers_minus1; i++ )
    {
        p_sps->sps_max[i].dec_pic_buffering_minus1 = bs_read_ue( p_bs );
        p_sps->sps_max[i].num_reorder_pics = bs_read_ue( p_bs );
        p_sps->sps_max[i].latency_increase_plus1 = bs_read_ue( p_bs );
    }

    if( bs_remain( p_bs ) < 4 )
        return false;

    p_sps->log2_min_luma_coding_block_size_minus3 = bs_read_ue( p_bs );
    p_sps->log2_diff_max_min_luma_coding_block_size = bs_read_ue( p_bs );
    p_sps->log2_min_luma_transform_block_size_minus2 = bs_read_ue( p_bs );
    if( bs_remain( p_bs ) < 1 ) /* last late fail check */
        return false;
    p_sps->log2_diff_max_min_luma_transform_block_size = bs_read_ue( p_bs );

    /* parsing incomplete */

    p_sps->max_transform_hierarchy_depth_inter = bs_read_ue( p_bs );
    p_sps->max_transform_hierarchy_depth_intra = bs_read_ue( p_bs );
    p_sps->scaling_list_enabled = bs_read1( p_bs );
    if( p_sps->scaling_list_enabled )
    {
        p_sps->sps_scaling_list_data_present_flag = bs_read1( p_bs );
        if( p_sps->sps_scaling_list_data_present_flag &&
            ! hevc_parse_scaling_list_rbsp( p_bs ) )
        {
            return false;
        }
    }

    p_sps->amp_enabled_flag = bs_read1( p_bs );
    p_sps->sample_adaptive_offset_enabled_flag = bs_read1( p_bs );

    p_sps->pcm_enabled_flag = bs_read1( p_bs );
    if( p_sps->pcm_enabled_flag )
    {
        p_sps->pcm_sample_bit_depth_luma_minus1 = bs_read( p_bs, 4 );
        p_sps->pcm_sample_bit_depth_chroma_minus1 = bs_read( p_bs, 4 );
        p_sps->log2_min_pcm_luma_coding_block_size_minus3 = bs_read_ue( p_bs );
        p_sps->log2_diff_max_min_pcm_luma_coding_block_size = bs_read_ue( p_bs );
        p_sps->pcm_loop_filter_disabled_flag = bs_read1( p_bs );
    }

    p_sps->num_short_term_ref_pic_sets = bs_read_ue( p_bs );
    if( p_sps->num_short_term_ref_pic_sets > HEVC_MAX_SHORT_TERM_REF_PIC_SET )
        return false;

    hevc_short_term_ref_pic_set_t sets[HEVC_MAX_SHORT_TERM_REF_PIC_SET];
    memset(&sets, 0, sizeof(hevc_short_term_ref_pic_set_t) * HEVC_MAX_SHORT_TERM_REF_PIC_SET);
    for( unsigned int i=0; i<p_sps->num_short_term_ref_pic_sets; i++ )
    {
        if( !hevc_parse_st_ref_pic_set( p_bs, i, p_sps->num_short_term_ref_pic_sets, sets ) )
            return false;
    }

    p_sps->long_term_ref_pics_present_flag = bs_read1( p_bs );
    if( p_sps->long_term_ref_pics_present_flag )
    {
        p_sps->num_long_term_ref_pics_sps = bs_read_ue( p_bs );
        if( p_sps->num_long_term_ref_pics_sps > HEVC_MAX_LONG_TERM_REF_PIC_SET )
            return false;
        for( unsigned int i=0; i< p_sps->num_long_term_ref_pics_sps; i++ )
        {
             /* lt_ref_pic_poc_lsb_sps */
            bs_skip( p_bs, p_sps->log2_max_pic_order_cnt_lsb_minus4 + 4 );
             /* used_by_curr_pic_lt_sps_flag */
            bs_skip( p_bs, 1 );
        }
    }

    p_sps->sps_temporal_mvp_enabled_flag = bs_read1( p_bs );
    p_sps->strong_intra_smoothing_enabled_flag = bs_read1( p_bs );

    if( bs_remain( p_bs ) < 1 ) /* late fail */
        return false;

    p_sps->vui_parameters_present_flag = bs_read1( p_bs );
    if( p_sps->vui_parameters_present_flag &&
        !hevc_parse_vui_parameters_rbsp( p_bs, &p_sps->vui ) )
        return false;

    /* incomplete */

    return true;
}

void hevc_rbsp_release_sps( hevc_sequence_parameter_set_t *p_sps )
{
    free( p_sps );
}

IMPL_hevc_generic_decode( hevc_decode_sps, hevc_sequence_parameter_set_t,
                          hevc_parse_sequence_parameter_set_rbsp, hevc_rbsp_release_sps )

static bool hevc_parse_pic_parameter_set_rbsp( bs_t *p_bs,
                                               hevc_picture_parameter_set_t *p_pps )
{
    if( bs_remain( p_bs ) < 1 )
        return false;
    p_pps->pps_pic_parameter_set_id = bs_read_ue( p_bs );
    if( p_pps->pps_pic_parameter_set_id > HEVC_PPS_ID_MAX || bs_remain( p_bs ) < 1 )
        return false;
    p_pps->pps_seq_parameter_set_id = bs_read_ue( p_bs );
    if( p_pps->pps_seq_parameter_set_id > HEVC_SPS_ID_MAX )
        return false;
    p_pps->dependent_slice_segments_enabled_flag = bs_read1( p_bs );
    p_pps->output_flag_present_flag = bs_read1( p_bs );
    p_pps->num_extra_slice_header_bits = bs_read( p_bs, 3 );
    p_pps->sign_data_hiding_enabled_flag = bs_read1( p_bs );
    p_pps->cabac_init_present_flag = bs_read1( p_bs );

    p_pps->num_ref_idx_l0_default_active_minus1 = bs_read_ue( p_bs );
    p_pps->num_ref_idx_l1_default_active_minus1 = bs_read_ue( p_bs );

    p_pps->init_qp_minus26 = bs_read_se( p_bs );
    p_pps->constrained_intra_pred_flag = bs_read1( p_bs );
    p_pps->transform_skip_enabled_flag = bs_read1( p_bs );
    p_pps->cu_qp_delta_enabled_flag = bs_read1( p_bs );
    if( p_pps->cu_qp_delta_enabled_flag )
        p_pps->diff_cu_qp_delta_depth = bs_read_ue( p_bs );

    if( bs_remain( p_bs ) < 1 )
        return false;

    p_pps->pps_cb_qp_offset = bs_read_se( p_bs );
    p_pps->pps_cr_qp_offset = bs_read_se( p_bs );
    p_pps->pic_slice_level_chroma_qp_offsets_present_flag = bs_read1( p_bs );
    p_pps->weighted_pred_flag = bs_read1( p_bs );
    p_pps->weighted_bipred_flag = bs_read1( p_bs );
    p_pps->transquant_bypass_enable_flag = bs_read1( p_bs );
    p_pps->tiles_enabled_flag = bs_read1( p_bs );
    p_pps->entropy_coding_sync_enabled_flag = bs_read1( p_bs );

    if( p_pps->tiles_enabled_flag )
    {
        p_pps->num_tile_columns_minus1 = bs_read_ue( p_bs ); /* TODO: validate max col/row values */
        p_pps->num_tile_rows_minus1 = bs_read_ue( p_bs );    /*       against sps PicWidthInCtbsY */
        p_pps->uniform_spacing_flag = bs_read1( p_bs );
        if( !p_pps->uniform_spacing_flag )
        {
            if( bs_remain( p_bs ) < (int64_t) p_pps->num_tile_columns_minus1 +
                                               p_pps->num_tile_rows_minus1 + 1 )
                return false;
            for( unsigned i=0; i< p_pps->num_tile_columns_minus1; i++ )
                (void) bs_read_ue( p_bs );
            for( unsigned i=0; i< p_pps->num_tile_rows_minus1; i++ )
                (void) bs_read_ue( p_bs );
        }
        p_pps->loop_filter_across_tiles_enabled_flag = bs_read1( p_bs );
    }

    p_pps->pps_loop_filter_across_slices_enabled_flag = bs_read1( p_bs );
    p_pps->deblocking_filter_control_present_flag = bs_read1( p_bs );
    if( p_pps->deblocking_filter_control_present_flag )
    {
        p_pps->deblocking_filter_override_enabled_flag = bs_read1( p_bs );
        p_pps->pps_deblocking_filter_disabled_flag = bs_read1( p_bs );
        if( !p_pps->pps_deblocking_filter_disabled_flag )
        {
            p_pps->pps_beta_offset_div2 = bs_read_se( p_bs );
            p_pps->pps_tc_offset_div2 = bs_read_se( p_bs );
        }
    }

    p_pps->scaling_list_data_present_flag = bs_read1( p_bs );
    if( p_pps->scaling_list_data_present_flag && !hevc_parse_scaling_list_rbsp( p_bs ) )
        return false;

    p_pps->lists_modification_present_flag = bs_read1( p_bs );
    p_pps->log2_parallel_merge_level_minus2 = bs_read_ue( p_bs );
    p_pps->slice_header_extension_present_flag = bs_read1( p_bs );

    if( bs_remain( p_bs ) < 1 )
        return false;

    p_pps->pps_extension_present_flag = bs_read1( p_bs );
    if( p_pps->pps_extension_present_flag )
    {
        p_pps->pps_range_extension_flag = bs_read1( p_bs );
        p_pps->pps_multilayer_extension_flag = bs_read1( p_bs );
        p_pps->pps_3d_extension_flag = bs_read1( p_bs );
        if( bs_remain( p_bs ) < 5 )
            return false;
        p_pps->pps_extension_5bits = bs_read( p_bs, 5 );
    }

    return true;
}

void hevc_rbsp_release_pps( hevc_picture_parameter_set_t *p_pps )
{
    free( p_pps );
}

IMPL_hevc_generic_decode( hevc_decode_pps, hevc_picture_parameter_set_t,
                          hevc_parse_pic_parameter_set_rbsp, hevc_rbsp_release_pps )

uint8_t hevc_get_sps_vps_id( const hevc_sequence_parameter_set_t *p_sps )
{
    return p_sps->sps_video_parameter_set_id;
}

uint8_t hevc_get_pps_sps_id( const hevc_picture_parameter_set_t *p_pps )
{
    return p_pps->pps_seq_parameter_set_id;
}

uint8_t hevc_get_slice_pps_id( const hevc_slice_segment_header_t *p_slice )
{
    return p_slice->slice_pic_parameter_set_id;
}

bool hevc_get_sps_profile_tier_level( const hevc_sequence_parameter_set_t *p_sps,
                                      uint8_t *pi_profile, uint8_t *pi_level)
{
    if(p_sps->profile_tier_level.general.profile_idc)
    {
        *pi_profile = p_sps->profile_tier_level.general.profile_idc;
        *pi_level = p_sps->profile_tier_level.general_level_idc;
        return true;
    }
    return false;
}

bool hevc_get_picture_size( const hevc_sequence_parameter_set_t *p_sps,
                            unsigned *p_w, unsigned *p_h, unsigned *p_vw, unsigned *p_vh )
{
    *p_w = *p_vw = p_sps->pic_width_in_luma_samples;
    *p_h = *p_vh = p_sps->pic_height_in_luma_samples;
    if( p_sps->conformance_window_flag )
    {
        *p_vh -= p_sps->conf_win.bottom_offset + p_sps->conf_win.top_offset;
        *p_vh -= p_sps->conf_win.left_offset +  p_sps->conf_win.right_offset;
    }
    return true;
}

uint8_t hevc_get_max_num_reorder( const hevc_video_parameter_set_t *p_vps )
{
    return p_vps->vps_max[p_vps->vps_max_sub_layers_minus1/* HighestTid */].num_reorder_pics;
}

static inline uint8_t vlc_ceil_log2( uint32_t val )
{
    uint8_t n = 31 - clz(val);
    if (((unsigned)1 << n) != val)
        n++;
    return n;
}

static bool hevc_get_picture_CtbsYsize( const hevc_sequence_parameter_set_t *p_sps, unsigned *p_w, unsigned *p_h )
{
    const unsigned int MinCbLog2SizeY = p_sps->log2_min_luma_coding_block_size_minus3 + 3;
    const unsigned int CtbLog2SizeY = MinCbLog2SizeY + p_sps->log2_diff_max_min_luma_coding_block_size;
    if( CtbLog2SizeY > 31 )
        return false;
    const unsigned int CtbSizeY = 1 << CtbLog2SizeY;
    *p_w = (p_sps->pic_width_in_luma_samples - 1) / CtbSizeY + 1;
    *p_h = (p_sps->pic_height_in_luma_samples - 1) / CtbSizeY + 1;
    return true;
}

bool hevc_get_frame_rate( const hevc_sequence_parameter_set_t *p_sps,
                          const hevc_video_parameter_set_t *p_vps,
                          unsigned *pi_num, unsigned *pi_den )
{
    if( p_sps->vui_parameters_present_flag && p_sps->vui.vui_timing_info_present_flag )
    {
        *pi_den = p_sps->vui.timing.vui_num_units_in_tick;
        *pi_num = p_sps->vui.timing.vui_time_scale;
        return (*pi_den && *pi_num);
    }
    else if( p_vps && p_vps->vps_timing_info_present_flag )
    {
        *pi_den = p_vps->vps_num_units_in_tick;
        *pi_num = p_vps->vps_time_scale;
        return (*pi_den && *pi_num);
    }
    return false;
}

bool hevc_get_aspect_ratio( const hevc_sequence_parameter_set_t *p_sps,
                            unsigned *num, unsigned *den )
{
    if( p_sps->vui_parameters_present_flag )
    {
        if( p_sps->vui.ar.aspect_ratio_idc != 255 )
        {
            static const uint8_t ar_table[16][2] =
            {
                {    1,      1 },
                {   12,     11 },
                {   10,     11 },
                {   16,     11 },
                {   40,     33 },
                {   24,     11 },
                {   20,     11 },
                {   32,     11 },
                {   80,     33 },
                {   18,     11 },
                {   15,     11 },
                {   64,     33 },
                {  160,     99 },
                {    4,      3 },
                {    3,      2 },
                {    2,      1 },
            };
            if( p_sps->vui.ar.aspect_ratio_idc > 0 &&
                p_sps->vui.ar.aspect_ratio_idc < 17 )
            {
                *num = ar_table[p_sps->vui.ar.aspect_ratio_idc - 1][0];
                *den = ar_table[p_sps->vui.ar.aspect_ratio_idc - 1][1];
                return true;
            }
        }
        else
        {
            *num = p_sps->vui.ar.sar_width;
            *den = p_sps->vui.ar.sar_height;
            return true;
        }
    }
    return false;
}

bool hevc_get_colorimetry( const hevc_sequence_parameter_set_t *p_sps,
                           video_color_primaries_t *p_primaries,
                           video_transfer_func_t *p_transfer,
                           video_color_space_t *p_colorspace,
                           bool *p_full_range )
{
    if( !p_sps->vui_parameters_present_flag )
        return false;
    *p_primaries =
        hxxx_colour_primaries_to_vlc( p_sps->vui.vs.colour.colour_primaries );
    *p_transfer =
        hxxx_transfer_characteristics_to_vlc( p_sps->vui.vs.colour.transfer_characteristics );
    *p_colorspace =
        hxxx_matrix_coeffs_to_vlc( p_sps->vui.vs.colour.matrix_coeffs );
    *p_full_range = p_sps->vui.vs.video_full_range_flag;
    return true;
}

static bool hevc_parse_slice_segment_header_rbsp( bs_t *p_bs,
                                                  pf_get_matchedxps get_matchedxps,
                                                  void *priv,
                                                  hevc_slice_segment_header_t *p_sl )
{
    hevc_sequence_parameter_set_t *p_sps;
    hevc_picture_parameter_set_t *p_pps;
    hevc_video_parameter_set_t *p_vps;

    if( bs_remain( p_bs ) < 3 )
        return false;

    p_sl->first_slice_segment_in_pic_flag = bs_read1( p_bs );
    if( p_sl->nal_type >= HEVC_NAL_BLA_W_LP && p_sl->nal_type <= HEVC_NAL_IRAP_VCL23 )
        p_sl->no_output_of_prior_pics_flag = bs_read1( p_bs );
    p_sl->slice_pic_parameter_set_id = bs_read_ue( p_bs );
    if( p_sl->slice_pic_parameter_set_id > HEVC_PPS_ID_MAX || bs_remain( p_bs ) < 1 )
        return false;

    get_matchedxps( p_sl->slice_pic_parameter_set_id, priv, &p_pps, &p_sps, &p_vps );
    if(!p_sps || !p_pps)
        return false;

    if( !p_sl->first_slice_segment_in_pic_flag )
    {
        if( p_pps->dependent_slice_segments_enabled_flag )
            p_sl->dependent_slice_segment_flag = bs_read1( p_bs );

        unsigned w, h;
        if( !hevc_get_picture_CtbsYsize( p_sps, &w, &h ) )
            return false;

        (void) bs_read( p_bs, vlc_ceil_log2( w * h ) ); /* slice_segment_address */
    }

    if( !p_sl->dependent_slice_segment_flag )
    {
        unsigned i=0;
        if( p_pps->num_extra_slice_header_bits > i )
        {
            i++;
            bs_skip( p_bs, 1 ); /* discardable_flag */
        }

        if( p_pps->num_extra_slice_header_bits > i )
        {
            i++;
            bs_skip( p_bs, 1 ); /* cross_layer_bla_flag */
        }

        if( i < p_pps->num_extra_slice_header_bits )
           bs_skip( p_bs, p_pps->num_extra_slice_header_bits - i );

        p_sl->slice_type = bs_read_ue( p_bs );
        if( p_sl->slice_type > HEVC_SLICE_TYPE_I )
            return false;

        if( p_pps->output_flag_present_flag )
            p_sl->pic_output_flag = bs_read1( p_bs );
    }

    if( p_sps->separate_colour_plane_flag )
        bs_skip( p_bs, 2 ); /* colour_plane_id */

    if( p_sl->nal_type != HEVC_NAL_IDR_W_RADL && p_sl->nal_type != HEVC_NAL_IDR_N_LP )
        p_sl->pic_order_cnt_lsb = bs_read( p_bs, p_sps->log2_max_pic_order_cnt_lsb_minus4 + 4 );
    else
        p_sl->pic_order_cnt_lsb = 0;

    if( bs_remain( p_bs ) < 1 )
        return false;

    return true;
}

void hevc_rbsp_release_slice_header( hevc_slice_segment_header_t *p_sh )
{
    free( p_sh );
}

hevc_slice_segment_header_t * hevc_decode_slice_header( const uint8_t *p_buf, size_t i_buf, bool b_escaped,
                                                        pf_get_matchedxps get_matchedxps, void *priv )
{
    hevc_slice_segment_header_t *p_sh = calloc(1, sizeof(hevc_slice_segment_header_t));
    if(likely(p_sh))
    {
        bs_t bs;
        bs_init( &bs, p_buf, i_buf );
        unsigned i_bitflow = 0;
        if( b_escaped )
        {
            bs.p_fwpriv = &i_bitflow;
            bs.pf_forward = hxxx_bsfw_ep3b_to_rbsp;  /* Does the emulated 3bytes conversion to rbsp */
        }
        else (void) i_bitflow;
        bs_skip( &bs, 1 );
        p_sh->nal_type = bs_read( &bs, 6 );
        p_sh->nuh_layer_id = bs_read( &bs, 6 );
        p_sh->temporal_id_plus1 = bs_read( &bs, 3 );
        if( p_sh->nuh_layer_id > 62 || p_sh->temporal_id_plus1 == 0 ||
           !hevc_parse_slice_segment_header_rbsp( &bs, get_matchedxps, priv, p_sh ) )
        {
            hevc_rbsp_release_slice_header( p_sh );
            p_sh = NULL;
        }
    }
    return p_sh;
}

bool hevc_get_slice_type( const hevc_slice_segment_header_t *p_sli, enum hevc_slice_type_e *pi_type )
{
    if( !p_sli->dependent_slice_segment_flag )
    {
        *pi_type = p_sli->slice_type;
        return true;
    }
    return false;
}

bool hevc_get_profile_level(const es_format_t *p_fmt, uint8_t *pi_profile,
                            uint8_t *pi_level, uint8_t *pi_nal_length_size)
{
    const uint8_t *p = (const uint8_t*)p_fmt->p_extra;
    if(p_fmt->i_extra < 23 || p[0] != 1)
        return false;

    /* HEVCDecoderConfigurationRecord */
    if(pi_profile)
        *pi_profile = p[1] & 0x1F;

    if(pi_level)
        *pi_level = p[12];

    if (pi_nal_length_size)
        *pi_nal_length_size = 1 + (p[21]&0x03);

    return true;
}

/*
 * HEVCDecoderConfigurationRecord operations
 */

static void hevc_dcr_params_from_vps( const uint8_t * p_buffer, size_t i_buffer,
                                      struct hevc_dcr_values *p_values )
{
    if( i_buffer < 19 )
        return;

    bs_t bs;
    bs_init( &bs, p_buffer, i_buffer );
    unsigned i_bitflow = 0;
    bs.p_fwpriv = &i_bitflow;
    bs.pf_forward = hxxx_bsfw_ep3b_to_rbsp;  /* Does the emulated 3bytes conversion to rbsp */

    /* first two bytes are the NAL header, 3rd and 4th are:
        vps_video_parameter_set_id(4)
        vps_reserved_3_2bis(2)
        vps_max_layers_minus1(6)
        vps_max_sub_layers_minus1(3)
        vps_temporal_id_nesting_flags
    */
    bs_skip( &bs, 16 + 4 + 2 + 6 );
    p_values->i_numTemporalLayer = bs_read( &bs, 3 ) + 1;
    p_values->b_temporalIdNested = bs_read1( &bs );

    /* 5th & 6th are reserved 0xffff */
    bs_skip( &bs, 16 );
    /* copy the first 12 bytes of profile tier */
    for( unsigned i=0; i<12; i++ )
        p_values->general_configuration[i] = bs_read( &bs, 8 );
}

#define HEVC_DCR_ADD_NALS(type, count, buffers, sizes) \
for (uint8_t i = 0; i < count; i++) \
{ \
    if( i ==0 ) \
    { \
        *p++ = (type | (b_completeness ? 0x80 : 0)); \
        SetWBE( p, count ); p += 2; \
    } \
    SetWBE( p, sizes[i]); p += 2; \
    memcpy( p, buffers[i], sizes[i] ); p += sizes[i];\
}

#define HEVC_DCR_ADD_SIZES(count, sizes) \
if(count > 0) \
{\
    i_total_size += 3;\
    for(uint8_t i=0; i<count; i++)\
        i_total_size += 2 + sizes[i];\
}

/* Generate HEVCDecoderConfiguration iso/iec 14496-15 3rd edition */
uint8_t * hevc_create_dcr( const struct hevc_dcr_params *p_params,
                           uint8_t i_nal_length_size,
                           bool b_completeness, size_t *pi_size )
{
    *pi_size = 0;

    if( i_nal_length_size != 1 && i_nal_length_size != 2 && i_nal_length_size != 4 )
        return NULL;

    struct hevc_dcr_values values =
    {
        .general_configuration = {0},
        .i_numTemporalLayer = 0,
        .i_chroma_idc = 1,
        .i_bit_depth_luma_minus8 = 0,
        .i_bit_depth_chroma_minus8 = 0,
        .b_temporalIdNested = false,
    };

    if( p_params->p_values != NULL )
    {
        values = *p_params->p_values;
    }
    else
    {
        if( p_params->i_vps_count == 0 || p_params->i_sps_count == 0 )
           return NULL; /* required to extract info */

        hevc_dcr_params_from_vps( p_params->p_vps[0], p_params->rgi_vps[0], &values );

        hevc_sequence_parameter_set_t *p_sps =
                hevc_decode_sps( p_params->p_sps[0], p_params->rgi_sps[0], true );
        if( p_sps )
        {
            values.i_chroma_idc = p_sps->chroma_format_idc;
            values.i_bit_depth_chroma_minus8 = p_sps->bit_depth_chroma_minus8;
            values.i_bit_depth_luma_minus8 = p_sps->bit_depth_luma_minus8;
            hevc_rbsp_release_sps( p_sps );
        }
    }

    size_t i_total_size = 1+12+2+4+2+2;
    HEVC_DCR_ADD_SIZES(p_params->i_vps_count, p_params->rgi_vps);
    HEVC_DCR_ADD_SIZES(p_params->i_sps_count, p_params->rgi_sps);
    HEVC_DCR_ADD_SIZES(p_params->i_pps_count, p_params->rgi_pps);
    HEVC_DCR_ADD_SIZES(p_params->i_seipref_count, p_params->rgi_seipref);
    HEVC_DCR_ADD_SIZES(p_params->i_seisuff_count, p_params->rgi_seisuff);

    uint8_t *p_data = malloc( i_total_size );
    if( p_data == NULL )
        return NULL;

    *pi_size = i_total_size;
    uint8_t *p = p_data;

    /* version */
    *p++ = 0x01;
    memcpy( p, values.general_configuration, 12 ); p += 12;
    /* Don't set min spatial segmentation */
    SetWBE( p, 0xF000 ); p += 2;
    /* Don't set parallelism type since segmentation isn't set */
    *p++ = 0xFC;
    *p++ = (0xFC | (values.i_chroma_idc & 0x03));
    *p++ = (0xF8 | (values.i_bit_depth_luma_minus8 & 0x07));
    *p++ = (0xF8 | (values.i_bit_depth_chroma_minus8 & 0x07));

    /* Don't set framerate */
    SetWBE( p, 0x0000); p += 2;
    /* Force NAL size of 4 bytes that replace the startcode */
    *p++ = ( ((values.i_numTemporalLayer & 0x07) << 3) |
              (values.b_temporalIdNested << 2) |
              (i_nal_length_size - 1) );
    /* total number of arrays */
    *p++ = !!p_params->i_vps_count + !!p_params->i_sps_count +
           !!p_params->i_pps_count + !!p_params->i_seipref_count +
           !!p_params->i_seisuff_count;

    /* Write NAL arrays */
    HEVC_DCR_ADD_NALS(HEVC_NAL_VPS, p_params->i_vps_count,
                      p_params->p_vps, p_params->rgi_vps);
    HEVC_DCR_ADD_NALS(HEVC_NAL_SPS, p_params->i_sps_count,
                      p_params->p_sps, p_params->rgi_sps);
    HEVC_DCR_ADD_NALS(HEVC_NAL_PPS, p_params->i_pps_count,
                      p_params->p_pps, p_params->rgi_pps);
    HEVC_DCR_ADD_NALS(HEVC_NAL_PREF_SEI, p_params->i_seipref_count,
                      p_params->p_seipref, p_params->rgi_seipref);
    HEVC_DCR_ADD_NALS(HEVC_NAL_SUFF_SEI, p_params->i_seisuff_count,
                      p_params->p_seisuff, p_params->rgi_seisuff);

    return p_data;
}

#undef HEVC_DCR_ADD_NALS
#undef HEVC_DCR_ADD_SIZES


/*
 * 8.3.1 Decoding process for POC
 */
int hevc_compute_picture_order_count( const hevc_sequence_parameter_set_t *p_sps,
                                       const hevc_slice_segment_header_t *p_slice,
                                       hevc_poc_ctx_t *p_ctx )
{
    struct
    {
        int lsb;
        int msb;
    } prevPicOrderCnt;
    int pocMSB;
    bool NoRaslOutputFlag;
    bool IsIRAP = ( p_slice->nal_type >= HEVC_NAL_BLA_W_LP &&
                    p_slice->nal_type <= HEVC_NAL_IRAP_VCL23 );

    if( IsIRAP )
    {
        /* if( IRAP ) NoRaslOutputFlag = first || IDR || BLA || after(EOSNAL) */
        NoRaslOutputFlag =(p_ctx->first_picture ||
                           p_slice->nal_type == HEVC_NAL_IDR_N_LP ||
                           p_slice->nal_type == HEVC_NAL_IDR_W_RADL ||
                           p_slice->nal_type == HEVC_NAL_BLA_W_LP ||
                           p_slice->nal_type == HEVC_NAL_BLA_W_RADL ||
                           p_slice->nal_type == HEVC_NAL_BLA_N_LP ||
                           p_ctx->HandleCraAsBlaFlag );
    }
    else
    {
        NoRaslOutputFlag = false;
    }

    if( p_slice->nal_type == HEVC_NAL_IDR_N_LP ||
        p_slice->nal_type == HEVC_NAL_IDR_W_RADL )
    {
        prevPicOrderCnt.msb = 0;
        prevPicOrderCnt.lsb = 0;
    }
    /* Not an IRAP with NoRaslOutputFlag == 1 */
    else if( !IsIRAP || !NoRaslOutputFlag )
    {
        prevPicOrderCnt.msb = p_ctx->prevTid0PicOrderCnt.msb;
        prevPicOrderCnt.lsb = p_ctx->prevTid0PicOrderCnt.lsb;
    }

    if( IsIRAP && NoRaslOutputFlag )
    {
        pocMSB = 0;
    }
    else
    {
        const unsigned maxPocLSB = 1U << (p_sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
        pocMSB = prevPicOrderCnt.msb;
        int64_t orderDiff = (int64_t)p_slice->pic_order_cnt_lsb - prevPicOrderCnt.lsb;
        if( orderDiff < 0 && -orderDiff >= maxPocLSB / 2 )
            pocMSB += maxPocLSB;
        else if( orderDiff > maxPocLSB / 2 )
            pocMSB -= maxPocLSB;
    }

    /* Set prevTid0Pic for next pic */
    if( p_slice->temporal_id_plus1 == 1 &&
       !( ( p_slice->nal_type <= HEVC_NAL_RSV_VCL_N14 && p_slice->nal_type % 2 == 0 /* SLNR */ ) ||
          ( p_slice->nal_type >= HEVC_NAL_RADL_N && p_slice->nal_type <= HEVC_NAL_RASL_R ) /* RADL or RASL */ ) )
    {
        p_ctx->prevTid0PicOrderCnt.msb = pocMSB;
        p_ctx->prevTid0PicOrderCnt.lsb = p_slice->pic_order_cnt_lsb;
    }

    p_ctx->first_picture = false;

    return pocMSB + p_slice->pic_order_cnt_lsb;
}

struct hevc_sei_pic_timing_t
{
    nal_u4_t pic_struct;
    nal_u2_t source_scan_type;
};

void hevc_release_sei_pic_timing( hevc_sei_pic_timing_t *p_timing )
{
    free( p_timing );
}

hevc_sei_pic_timing_t * hevc_decode_sei_pic_timing( bs_t *p_bs,
                                                    const hevc_sequence_parameter_set_t *p_sps )
{
    hevc_sei_pic_timing_t *p_timing = malloc(sizeof(*p_timing));
    if( p_timing )
    {
        if( p_sps->vui.frame_field_info_present_flag )
        {
            p_timing->pic_struct = bs_read( p_bs, 4 );
            p_timing->source_scan_type = bs_read( p_bs, 2 );
        }
        else
        {
            p_timing->pic_struct = 0;
            p_timing->source_scan_type = 1;
        }
    }
    return p_timing;
}

bool hevc_frame_is_progressive( const hevc_sequence_parameter_set_t *p_sps,
                                const hevc_sei_pic_timing_t *p_timing )
{
    if( p_sps->vui_parameters_present_flag &&
        p_sps->vui.field_seq_flag )
        return false;

    if( p_sps->profile_tier_level.general.interlaced_source_flag &&
       !p_sps->profile_tier_level.general.progressive_source_flag )
        return false;

    if( p_timing && p_sps->vui.frame_field_info_present_flag )
    {
        if( p_timing->source_scan_type < 2 )
            return p_timing->source_scan_type != 0;
    }

    return true;
}

uint8_t hevc_get_num_clock_ts( const hevc_sequence_parameter_set_t *p_sps,
                               const hevc_sei_pic_timing_t *p_timing )
{
    if( p_sps->vui.frame_field_info_present_flag && p_timing && p_timing->pic_struct < 13 )
    {
        /* !WARN modified with units_field_based_flag (D.3.25) for values 0, 7 and 8 */
        const uint8_t rgi_numclock[13] = { 2, 1, 1, 2, 2, 3, 3, 4, 6, 1, 1, 1, 1 };
        return rgi_numclock[p_timing->pic_struct];
    }

    if( p_sps->vui_parameters_present_flag )
    {
        if( p_sps->vui.field_seq_flag )
            return 1; /* D.3.27 */
    }
    else if( p_sps->profile_tier_level.general.interlaced_source_flag &&
            !p_sps->profile_tier_level.general.progressive_source_flag )
    {
        return 1;
    }

    return 2;
}
