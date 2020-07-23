/*****************************************************************************
 * Copyright © 2010-2014 VideoLAN
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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

#include <assert.h>

#include "h264_nal.h"
#include "hxxx_nal.h"
#include "hxxx_ep3b.h"
#include "iso_color_tables.h"

#include <vlc_bits.h>
#include <vlc_boxes.h>
#include <vlc_es.h>
#include <limits.h>

/* H264 Level limits from Table A-1 */
typedef struct
{
    unsigned i_max_dpb_mbs;
} h264_level_limits_t;

enum h264_level_numbers_e
{
    H264_LEVEL_NUMBER_1_B = 9, /* special level not following the 10x rule */
    H264_LEVEL_NUMBER_1   = 10,
    H264_LEVEL_NUMBER_1_1 = 11,
    H264_LEVEL_NUMBER_1_2 = 12,
    H264_LEVEL_NUMBER_1_3 = 13,
    H264_LEVEL_NUMBER_2   = 20,
    H264_LEVEL_NUMBER_2_1 = 21,
    H264_LEVEL_NUMBER_2_2 = 22,
    H264_LEVEL_NUMBER_3   = 30,
    H264_LEVEL_NUMBER_3_1 = 31,
    H264_LEVEL_NUMBER_3_2 = 32,
    H264_LEVEL_NUMBER_4   = 40,
    H264_LEVEL_NUMBER_4_1 = 41,
    H264_LEVEL_NUMBER_4_2 = 42,
    H264_LEVEL_NUMBER_5   = 50,
    H264_LEVEL_NUMBER_5_1 = 51,
    H264_LEVEL_NUMBER_5_2 = 52,
    H264_LEVEL_NUMBER_6   = 60,
    H264_LEVEL_NUMBER_6_1 = 61,
    H264_LEVEL_NUMBER_6_2 = 62,
};

const struct
{
    const uint16_t i_level;
    const h264_level_limits_t limits;
} h264_levels_limits[] = {
    { H264_LEVEL_NUMBER_1_B, { 396 } },
    { H264_LEVEL_NUMBER_1,   { 396 } },
    { H264_LEVEL_NUMBER_1_1, { 900 } },
    { H264_LEVEL_NUMBER_1_2, { 2376 } },
    { H264_LEVEL_NUMBER_1_3, { 2376 } },
    { H264_LEVEL_NUMBER_2,   { 2376 } },
    { H264_LEVEL_NUMBER_2_1, { 4752 } },
    { H264_LEVEL_NUMBER_2_2, { 8100 } },
    { H264_LEVEL_NUMBER_3,   { 8100 } },
    { H264_LEVEL_NUMBER_3_1, { 18000 } },
    { H264_LEVEL_NUMBER_3_2, { 20480 } },
    { H264_LEVEL_NUMBER_4,   { 32768 } },
    { H264_LEVEL_NUMBER_4_1, { 32768 } },
    { H264_LEVEL_NUMBER_4_2, { 34816 } },
    { H264_LEVEL_NUMBER_5,   { 110400 } },
    { H264_LEVEL_NUMBER_5_1, { 184320 } },
    { H264_LEVEL_NUMBER_5_2, { 184320 } },
    { H264_LEVEL_NUMBER_6,   { 696320 } },
    { H264_LEVEL_NUMBER_6_1, { 696320 } },
    { H264_LEVEL_NUMBER_6_2, { 696320 } },
};

/*
 * For avcC specification, see ISO/IEC 14496-15,
 * For Annex B specification, see ISO/IEC 14496-10
 */

bool h264_isavcC( const uint8_t *p_buf, size_t i_buf )
{
    return ( i_buf >= H264_MIN_AVCC_SIZE &&
             p_buf[0] != 0x00 &&
             p_buf[1] != 0x00
/*          /!\Broken quicktime streams does not respect reserved bits
            (p_buf[4] & 0xFC) == 0xFC &&
            (p_buf[4] & 0x03) != 0x02 &&
            (p_buf[5] & 0x1F) > 0x00 */
            );
}

static size_t get_avcC_to_AnnexB_NAL_size( const uint8_t *p_buf, size_t i_buf )
{
    size_t i_total = 0;

    if( i_buf < H264_MIN_AVCC_SIZE )
        return 0;

    p_buf += 5;
    i_buf -= 5;

    for ( unsigned int j = 0; j < 2; j++ )
    {
        /* First time is SPS, Second is PPS */
        const unsigned int i_loop_end = p_buf[0] & (j == 0 ? 0x1f : 0xff);
        p_buf++; i_buf--;

        for ( unsigned int i = 0; i < i_loop_end; i++ )
        {
            if( i_buf < 2 )
                return 0;

            uint16_t i_nal_size = (p_buf[0] << 8) | p_buf[1];
            if(i_nal_size > i_buf - 2)
                return 0;
            i_total += i_nal_size + 4;
            p_buf += i_nal_size + 2;
            i_buf -= i_nal_size + 2;
        }

        if( j == 0 && i_buf < 1 )
            return 0;
    }
    return i_total;
}

uint8_t *h264_avcC_to_AnnexB_NAL( const uint8_t *p_buf, size_t i_buf,
                                  size_t *pi_result, uint8_t *pi_nal_length_size )
{
    *pi_result = get_avcC_to_AnnexB_NAL_size( p_buf, i_buf ); /* Does check min size */
    if( *pi_result == 0 )
        return NULL;

    /* Read infos in first 6 bytes */
    if ( pi_nal_length_size )
        *pi_nal_length_size = (p_buf[4] & 0x03) + 1;

    uint8_t *p_ret;
    uint8_t *p_out_buf = p_ret = malloc( *pi_result );
    if( !p_out_buf )
    {
        *pi_result = 0;
        return NULL;
    }

    p_buf += 5;

    for ( unsigned int j = 0; j < 2; j++ )
    {
        const unsigned int i_loop_end = p_buf[0] & (j == 0 ? 0x1f : 0xff);
        p_buf++;

        for ( unsigned int i = 0; i < i_loop_end; i++)
        {
            uint16_t i_nal_size = (p_buf[0] << 8) | p_buf[1];
            p_buf += 2;

            memcpy( p_out_buf, annexb_startcode4, 4 );
            p_out_buf += 4;

            memcpy( p_out_buf, p_buf, i_nal_size );
            p_out_buf += i_nal_size;
            p_buf += i_nal_size;
        }
    }

    return p_ret;
}

void h264_AVC_to_AnnexB( uint8_t *p_buf, uint32_t i_len,
                             uint8_t i_nal_length_size )
{
    uint32_t nal_len = 0;
    uint8_t nal_pos = 0;

    if( i_nal_length_size != 4 )
        return;

    /* This only works for a NAL length size of 4 */
    /* TODO: realloc/memmove if i_nal_length_size is 2 or 1 */
    while( i_len > 0 )
    {
        if( nal_pos < i_nal_length_size ) {
            unsigned int i;
            for( i = 0; nal_pos < i_nal_length_size && i < i_len; i++, nal_pos++ ) {
                nal_len = (nal_len << 8) | p_buf[i];
                p_buf[i] = 0;
            }
            if( nal_pos < i_nal_length_size )
                return;
            p_buf[i - 1] = 1;
            p_buf += i;
            i_len -= i;
        }
        if( nal_len > INT_MAX )
            return;
        if( nal_len > i_len )
        {
            nal_len -= i_len;
            return;
        }
        else
        {
            p_buf += nal_len;
            i_len -= nal_len;
            nal_len = 0;
            nal_pos = 0;
        }
    }
}

bool h264_AnnexB_get_spspps( const uint8_t *p_buf, size_t i_buf,
                             const uint8_t **pp_sps, size_t *p_sps_size,
                             const uint8_t **pp_pps, size_t *p_pps_size,
                             const uint8_t **pp_ext, size_t *p_ext_size )
{
    if( pp_sps ) { *p_sps_size = 0; *pp_sps = NULL; }
    if( pp_pps ) { *p_pps_size = 0; *pp_pps = NULL; }
    if( pp_ext ) { *p_ext_size = 0; *pp_ext = NULL; }

    hxxx_iterator_ctx_t it;
    hxxx_iterator_init( &it, p_buf, i_buf, 0 );

    const uint8_t *p_nal; size_t i_nal;
    while( hxxx_annexb_iterate_next( &it, &p_nal, &i_nal ) )
    {
        if( i_nal < 2 )
            continue;

        const enum h264_nal_unit_type_e i_nal_type = p_nal[0] & 0x1F;

        if ( i_nal_type <= H264_NAL_SLICE_IDR && i_nal_type != H264_NAL_UNKNOWN )
            break;

#define IFSET_NAL(type, var) \
    if( i_nal_type == type && pp_##var && *pp_##var == NULL )\
        { *pp_##var = p_nal; *p_##var##_size = i_nal; }

        IFSET_NAL(H264_NAL_SPS, sps)
        else
        IFSET_NAL(H264_NAL_PPS, pps)
        else
        IFSET_NAL(H264_NAL_SPS_EXT, ext);
#undef IFSET_NAL
    }

    return (pp_sps && *p_sps_size) || (pp_pps && *p_pps_size);
}

void h264_release_sps( h264_sequence_parameter_set_t *p_sps )
{
    free( p_sps );
}

#define H264_CONSTRAINT_SET_FLAG(N) (0x80 >> N)

static bool h264_parse_sequence_parameter_set_rbsp( bs_t *p_bs,
                                                    h264_sequence_parameter_set_t *p_sps )
{
    int i_tmp;

    int i_profile_idc = bs_read( p_bs, 8 );
    p_sps->i_profile = i_profile_idc;
    p_sps->i_constraint_set_flags = bs_read( p_bs, 8 );
    p_sps->i_level = bs_read( p_bs, 8 );
    /* sps id */
    uint32_t i_sps_id = bs_read_ue( p_bs );
    if( i_sps_id > H264_SPS_ID_MAX )
        return false;
    p_sps->i_id = i_sps_id;

    if( i_profile_idc == PROFILE_H264_HIGH ||
        i_profile_idc == PROFILE_H264_HIGH_10 ||
        i_profile_idc == PROFILE_H264_HIGH_422 ||
        i_profile_idc == PROFILE_H264_HIGH_444 || /* Old one, no longer on spec */
        i_profile_idc == PROFILE_H264_HIGH_444_PREDICTIVE ||
        i_profile_idc == PROFILE_H264_CAVLC_INTRA ||
        i_profile_idc == PROFILE_H264_SVC_BASELINE ||
        i_profile_idc == PROFILE_H264_SVC_HIGH ||
        i_profile_idc == PROFILE_H264_MVC_MULTIVIEW_HIGH ||
        i_profile_idc == PROFILE_H264_MVC_STEREO_HIGH ||
        i_profile_idc == PROFILE_H264_MVC_MULTIVIEW_DEPTH_HIGH ||
        i_profile_idc == PROFILE_H264_MVC_ENHANCED_MULTIVIEW_DEPTH_HIGH ||
        i_profile_idc == PROFILE_H264_MFC_HIGH )
    {
        /* chroma_format_idc */
        p_sps->i_chroma_idc = bs_read_ue( p_bs );
        if( p_sps->i_chroma_idc == 3 )
            p_sps->b_separate_colour_planes_flag = bs_read1( p_bs );
        else
            p_sps->b_separate_colour_planes_flag = 0;
        /* bit_depth_luma_minus8 */
        p_sps->i_bit_depth_luma = bs_read_ue( p_bs ) + 8;
        /* bit_depth_chroma_minus8 */
        p_sps->i_bit_depth_chroma = bs_read_ue( p_bs ) + 8;
        /* qpprime_y_zero_transform_bypass_flag */
        bs_skip( p_bs, 1 );
        /* seq_scaling_matrix_present_flag */
        i_tmp = bs_read( p_bs, 1 );
        if( i_tmp )
        {
            for( int i = 0; i < ((3 != p_sps->i_chroma_idc) ? 8 : 12); i++ )
            {
                /* seq_scaling_list_present_flag[i] */
                i_tmp = bs_read( p_bs, 1 );
                if( !i_tmp )
                    continue;
                const int i_size_of_scaling_list = (i < 6 ) ? 16 : 64;
                /* scaling_list (...) */
                int i_lastscale = 8;
                int i_nextscale = 8;
                for( int j = 0; j < i_size_of_scaling_list; j++ )
                {
                    if( i_nextscale != 0 )
                    {
                        /* delta_scale */
                        i_tmp = bs_read_se( p_bs );
                        i_nextscale = ( i_lastscale + i_tmp + 256 ) % 256;
                        /* useDefaultScalingMatrixFlag = ... */
                    }
                    /* scalinglist[j] */
                    i_lastscale = ( i_nextscale == 0 ) ? i_lastscale : i_nextscale;
                }
            }
        }
    }
    else
    {
        p_sps->i_chroma_idc = 1; /* Not present == inferred to 4:2:0 */
        p_sps->i_bit_depth_luma = 8;
        p_sps->i_bit_depth_chroma = 8;
    }

    /* Skip i_log2_max_frame_num */
    p_sps->i_log2_max_frame_num = bs_read_ue( p_bs );
    if( p_sps->i_log2_max_frame_num > 12)
        p_sps->i_log2_max_frame_num = 12;
    /* Read poc_type */
    p_sps->i_pic_order_cnt_type = bs_read_ue( p_bs );
    if( p_sps->i_pic_order_cnt_type == 0 )
    {
        /* skip i_log2_max_poc_lsb */
        p_sps->i_log2_max_pic_order_cnt_lsb = bs_read_ue( p_bs );
        if( p_sps->i_log2_max_pic_order_cnt_lsb > 12 )
            p_sps->i_log2_max_pic_order_cnt_lsb = 12;
    }
    else if( p_sps->i_pic_order_cnt_type == 1 )
    {
        p_sps->i_delta_pic_order_always_zero_flag = bs_read( p_bs, 1 );
        p_sps->offset_for_non_ref_pic = bs_read_se( p_bs );
        p_sps->offset_for_top_to_bottom_field = bs_read_se( p_bs );
        p_sps->i_num_ref_frames_in_pic_order_cnt_cycle = bs_read_ue( p_bs );
        if( p_sps->i_num_ref_frames_in_pic_order_cnt_cycle > 255 )
            return false;
        for( int i=0; i<p_sps->i_num_ref_frames_in_pic_order_cnt_cycle; i++ )
            p_sps->offset_for_ref_frame[i] = bs_read_se( p_bs );
    }
    /* i_num_ref_frames */
    bs_read_ue( p_bs );
    /* b_gaps_in_frame_num_value_allowed */
    bs_skip( p_bs, 1 );

    /* Read size */
    p_sps->pic_width_in_mbs_minus1 = bs_read_ue( p_bs );
    p_sps->pic_height_in_map_units_minus1 = bs_read_ue( p_bs );

    /* b_frame_mbs_only */
    p_sps->frame_mbs_only_flag = bs_read( p_bs, 1 );
    if( !p_sps->frame_mbs_only_flag )
        p_sps->mb_adaptive_frame_field_flag = bs_read( p_bs, 1 );

    /* b_direct8x8_inference */
    bs_skip( p_bs, 1 );

    /* crop */
    if( bs_read1( p_bs ) ) /* frame_cropping_flag */
    {
        p_sps->frame_crop.left_offset = bs_read_ue( p_bs );
        p_sps->frame_crop.right_offset = bs_read_ue( p_bs );
        p_sps->frame_crop.top_offset = bs_read_ue( p_bs );
        p_sps->frame_crop.bottom_offset = bs_read_ue( p_bs );
    }

    /* vui */
    i_tmp = bs_read( p_bs, 1 );
    if( i_tmp )
    {
        p_sps->vui.b_valid = true;
        /* read the aspect ratio part if any */
        i_tmp = bs_read( p_bs, 1 );
        if( i_tmp )
        {
            static const struct { int w, h; } sar[17] =
            {
                { 0,   0 }, { 1,   1 }, { 12, 11 }, { 10, 11 },
                { 16, 11 }, { 40, 33 }, { 24, 11 }, { 20, 11 },
                { 32, 11 }, { 80, 33 }, { 18, 11 }, { 15, 11 },
                { 64, 33 }, { 160,99 }, {  4,  3 }, {  3,  2 },
                {  2,  1 },
            };
            int i_sar = bs_read( p_bs, 8 );
            int w, h;

            if( i_sar < 17 )
            {
                w = sar[i_sar].w;
                h = sar[i_sar].h;
            }
            else if( i_sar == 255 )
            {
                w = bs_read( p_bs, 16 );
                h = bs_read( p_bs, 16 );
            }
            else
            {
                w = 0;
                h = 0;
            }

            if( w != 0 && h != 0 )
            {
                p_sps->vui.i_sar_num = w;
                p_sps->vui.i_sar_den = h;
            }
            else
            {
                p_sps->vui.i_sar_num = 1;
                p_sps->vui.i_sar_den = 1;
            }
        }

        /* overscan */
        i_tmp = bs_read( p_bs, 1 );
        if ( i_tmp )
            bs_read( p_bs, 1 );

        /* video signal type */
        i_tmp = bs_read( p_bs, 1 );
        if( i_tmp )
        {
            bs_read( p_bs, 3 );
            p_sps->vui.colour.b_full_range = bs_read( p_bs, 1 );
            /* colour desc */
            i_tmp = bs_read( p_bs, 1 );
            if ( i_tmp )
            {
                p_sps->vui.colour.i_colour_primaries = bs_read( p_bs, 8 );
                p_sps->vui.colour.i_transfer_characteristics = bs_read( p_bs, 8 );
                p_sps->vui.colour.i_matrix_coefficients = bs_read( p_bs, 8 );
            }
            else
            {
                p_sps->vui.colour.i_colour_primaries = ISO_23001_8_CP_UNSPECIFIED;
                p_sps->vui.colour.i_transfer_characteristics = ISO_23001_8_TC_UNSPECIFIED;
                p_sps->vui.colour.i_matrix_coefficients = ISO_23001_8_MC_UNSPECIFIED;
            }
        }

        /* chroma loc info */
        i_tmp = bs_read( p_bs, 1 );
        if( i_tmp )
        {
            bs_read_ue( p_bs );
            bs_read_ue( p_bs );
        }

        /* timing info */
        p_sps->vui.b_timing_info_present_flag = bs_read( p_bs, 1 );
        if( p_sps->vui.b_timing_info_present_flag )
        {
            p_sps->vui.i_num_units_in_tick = bs_read( p_bs, 32 );
            p_sps->vui.i_time_scale = bs_read( p_bs, 32 );
            p_sps->vui.b_fixed_frame_rate = bs_read( p_bs, 1 );
        }

        /* Nal hrd & VC1 hrd parameters */
        p_sps->vui.b_hrd_parameters_present_flag = false;
        for ( int i=0; i<2; i++ )
        {
            i_tmp = bs_read( p_bs, 1 );
            if( i_tmp )
            {
                p_sps->vui.b_hrd_parameters_present_flag = true;
                uint32_t count = bs_read_ue( p_bs ) + 1;
                if( count > 31 )
                    return false;
                bs_read( p_bs, 4 );
                bs_read( p_bs, 4 );
                for( uint32_t j = 0; j < count; j++ )
                {
                    bs_read_ue( p_bs );
                    bs_read_ue( p_bs );
                    bs_read( p_bs, 1 );
                    if( bs_error( p_bs ) )
                        return false;
                }
                bs_read( p_bs, 5 );
                p_sps->vui.i_cpb_removal_delay_length_minus1 = bs_read( p_bs, 5 );
                p_sps->vui.i_dpb_output_delay_length_minus1 = bs_read( p_bs, 5 );
                bs_read( p_bs, 5 );
            }
        }

        if( p_sps->vui.b_hrd_parameters_present_flag )
            bs_read( p_bs, 1 ); /* low delay hrd */

        /* pic struct info */
        p_sps->vui.b_pic_struct_present_flag = bs_read( p_bs, 1 );

        p_sps->vui.b_bitstream_restriction_flag = bs_read( p_bs, 1 );
        if( p_sps->vui.b_bitstream_restriction_flag )
        {
            bs_read( p_bs, 1 ); /* motion vector pic boundaries */
            bs_read_ue( p_bs ); /* max bytes per pic */
            bs_read_ue( p_bs ); /* max bits per mb */
            bs_read_ue( p_bs ); /* log2 max mv h */
            bs_read_ue( p_bs ); /* log2 max mv v */
            p_sps->vui.i_max_num_reorder_frames = bs_read_ue( p_bs );
            bs_read_ue( p_bs ); /* max dec frame buffering */
        }
    }

    return !bs_error( p_bs );
}

void h264_release_pps( h264_picture_parameter_set_t *p_pps )
{
    free( p_pps );
}

static bool h264_parse_picture_parameter_set_rbsp( bs_t *p_bs,
                                                   h264_picture_parameter_set_t *p_pps )
{
    uint32_t i_pps_id = bs_read_ue( p_bs ); // pps id
    uint32_t i_sps_id = bs_read_ue( p_bs ); // sps id
    if( i_pps_id > H264_PPS_ID_MAX || i_sps_id > H264_SPS_ID_MAX )
        return false;
    p_pps->i_id = i_pps_id;
    p_pps->i_sps_id = i_sps_id;

    bs_skip( p_bs, 1 ); // entropy coding mode flag
    p_pps->i_pic_order_present_flag = bs_read( p_bs, 1 );

    unsigned num_slice_groups = bs_read_ue( p_bs ) + 1;
    if( num_slice_groups > 8 ) /* never has value > 7. Annex A, G & J */
        return false;
    if( num_slice_groups > 1 )
    {
        unsigned slice_group_map_type = bs_read_ue( p_bs );
        if( slice_group_map_type == 0 )
        {
            for( unsigned i = 0; i < num_slice_groups; i++ )
                bs_read_ue( p_bs ); /* run_length_minus1[group] */
        }
        else if( slice_group_map_type == 2 )
        {
            for( unsigned i = 0; i < num_slice_groups; i++ )
            {
                bs_read_ue( p_bs ); /* top_left[group] */
                bs_read_ue( p_bs ); /* bottom_right[group] */
            }
        }
        else if( slice_group_map_type > 2 && slice_group_map_type < 6 )
        {
            bs_read1( p_bs );   /* slice_group_change_direction_flag */
            bs_read_ue( p_bs ); /* slice_group_change_rate_minus1 */
        }
        else if( slice_group_map_type == 6 )
        {
            unsigned pic_size_in_maps_units = bs_read_ue( p_bs ) + 1;
            unsigned sliceGroupSize = 1;
            while(num_slice_groups > 1)
            {
                sliceGroupSize++;
                num_slice_groups = ((num_slice_groups - 1) >> 1) + 1;
            }
            for( unsigned i = 0; i < pic_size_in_maps_units; i++ )
            {
                bs_skip( p_bs, sliceGroupSize );
            }
        }
    }

    bs_read_ue( p_bs ); /* num_ref_idx_l0_default_active_minus1 */
    bs_read_ue( p_bs ); /* num_ref_idx_l1_default_active_minus1 */
    p_pps->weighted_pred_flag = bs_read( p_bs, 1 );
    p_pps->weighted_bipred_idc = bs_read( p_bs, 2 );
    bs_read_se( p_bs ); /* pic_init_qp_minus26 */
    bs_read_se( p_bs ); /* pic_init_qs_minus26 */
    bs_read_se( p_bs ); /* chroma_qp_index_offset */
    bs_read( p_bs, 1 ); /* deblocking_filter_control_present_flag */
    bs_read( p_bs, 1 ); /* constrained_intra_pred_flag */
    p_pps->i_redundant_pic_present_flag = bs_read( p_bs, 1 );

    /* TODO */

    return true;
}

#define IMPL_h264_generic_decode( name, h264type, decode, release ) \
    h264type * name( const uint8_t *p_buf, size_t i_buf, bool b_escaped ) \
    { \
        h264type *p_h264type = calloc(1, sizeof(h264type)); \
        if(likely(p_h264type)) \
        { \
            bs_t bs; \
            struct hxxx_bsfw_ep3b_ctx_s bsctx; \
            if( b_escaped ) \
            { \
                hxxx_bsfw_ep3b_ctx_init( &bsctx ); \
                bs_init_custom( &bs, p_buf, i_buf, &hxxx_bsfw_ep3b_callbacks, &bsctx );\
            } \
            else bs_init( &bs, p_buf, i_buf ); \
            bs_skip( &bs, 8 ); /* Skip nal_unit_header */ \
            if( !decode( &bs, p_h264type ) ) \
            { \
                release( p_h264type ); \
                p_h264type = NULL; \
            } \
        } \
        return p_h264type; \
    }

IMPL_h264_generic_decode( h264_decode_sps, h264_sequence_parameter_set_t,
                          h264_parse_sequence_parameter_set_rbsp, h264_release_sps )

IMPL_h264_generic_decode( h264_decode_pps, h264_picture_parameter_set_t,
                          h264_parse_picture_parameter_set_rbsp, h264_release_pps )

block_t *h264_NAL_to_avcC( uint8_t i_nal_length_size,
                           const uint8_t **pp_sps_buf,
                           const size_t *p_sps_size, uint8_t i_sps_count,
                           const uint8_t **pp_pps_buf,
                           const size_t *p_pps_size, uint8_t i_pps_count )
{
    /* The length of the NAL size is encoded using 1, 2 or 4 bytes */
    if( i_nal_length_size != 1 && i_nal_length_size != 2
     && i_nal_length_size != 4 )
        return NULL;
    if( i_sps_count == 0 || i_sps_count > H264_SPS_ID_MAX || i_pps_count == 0 )
        return NULL;

    /* Calculate the total size of all SPS and PPS NALs */
    size_t i_spspps_size = 0;
    for( size_t i = 0; i < i_sps_count; ++i )
    {
        assert( pp_sps_buf[i] && p_sps_size[i] );
        if( p_sps_size[i] < 4 || p_sps_size[i] > UINT16_MAX )
            return NULL;
        i_spspps_size += p_sps_size[i] +  2 /* 16be size place holder */;
    }
    for( size_t i = 0; i < i_pps_count; ++i )
    {
        assert( pp_pps_buf[i] && p_pps_size[i] );
        if( p_pps_size[i] > UINT16_MAX)
            return NULL;
        i_spspps_size += p_pps_size[i] +  2 /* 16be size place holder */;
    }

    bo_t bo;
    /* 1 + 3 + 1 + 1 + 1 + i_spspps_size */
    if( bo_init( &bo, 7 + i_spspps_size ) != true )
        return NULL;

    bo_add_8( &bo, 1 ); /* configuration version */
    bo_add_mem( &bo, 3, &pp_sps_buf[0][1] ); /* i_profile/profile_compatibility/level */
    bo_add_8( &bo, 0xfc | (i_nal_length_size - 1) ); /* 0b11111100 | lengthsize - 1*/

    bo_add_8( &bo, 0xe0 | i_sps_count ); /* 0b11100000 | sps_count */
    for( size_t i = 0; i < i_sps_count; ++i )
    {
        bo_add_16be( &bo, p_sps_size[i] );
        bo_add_mem( &bo, p_sps_size[i], pp_sps_buf[i] );
    }

    bo_add_8( &bo, i_pps_count ); /* pps_count */
    for( size_t i = 0; i < i_pps_count; ++i )
    {
        bo_add_16be( &bo, p_pps_size[i] );
        bo_add_mem( &bo, p_pps_size[i], pp_pps_buf[i] );
    }

    return bo.b;
}

static const h264_level_limits_t * h264_get_level_limits( const h264_sequence_parameter_set_t *p_sps )
{
    uint16_t i_level_number = p_sps->i_level;
    if( i_level_number == H264_LEVEL_NUMBER_1_1 &&
       (p_sps->i_constraint_set_flags & H264_CONSTRAINT_SET_FLAG(3)) )
    {
        i_level_number = H264_LEVEL_NUMBER_1_B;
    }

    for( size_t i=0; i< ARRAY_SIZE(h264_levels_limits); i++ )
        if( h264_levels_limits[i].i_level == i_level_number )
            return & h264_levels_limits[i].limits;

    return NULL;
}

static uint8_t h264_get_max_dpb_frames( const h264_sequence_parameter_set_t *p_sps )
{
    const h264_level_limits_t *limits = h264_get_level_limits( p_sps );
    if( limits )
    {
        unsigned i_frame_height_in_mbs = ( p_sps->pic_height_in_map_units_minus1 + 1 ) *
                                         ( 2 - p_sps->frame_mbs_only_flag );
        unsigned i_den = ( p_sps->pic_width_in_mbs_minus1 + 1 ) * i_frame_height_in_mbs;
        uint8_t i_max_dpb_frames = limits->i_max_dpb_mbs / i_den;
        if( i_max_dpb_frames < 16 )
            return i_max_dpb_frames;
    }
    return 16;
}

bool h264_get_dpb_values( const h264_sequence_parameter_set_t *p_sps,
                          uint8_t *pi_depth, unsigned *pi_delay )
{
    uint8_t i_max_num_reorder_frames = p_sps->vui.i_max_num_reorder_frames;
    if( !p_sps->vui.b_bitstream_restriction_flag )
    {
        switch( p_sps->i_profile ) /* E-2.1 */
        {
            case PROFILE_H264_BASELINE:
                i_max_num_reorder_frames = 0; /* only I & P */
                break;
            case PROFILE_H264_CAVLC_INTRA:
            case PROFILE_H264_SVC_HIGH:
            case PROFILE_H264_HIGH:
            case PROFILE_H264_HIGH_10:
            case PROFILE_H264_HIGH_422:
            case PROFILE_H264_HIGH_444_PREDICTIVE:
                if( p_sps->i_constraint_set_flags & H264_CONSTRAINT_SET_FLAG(3) )
                {
                    i_max_num_reorder_frames = 0; /* all IDR */
                    break;
                }
                /* fallthrough */
            default:
                i_max_num_reorder_frames = h264_get_max_dpb_frames( p_sps );
                break;
        }
    }

    *pi_depth = i_max_num_reorder_frames;
    *pi_delay = 0;

    return true;
}

bool h264_get_picture_size( const h264_sequence_parameter_set_t *p_sps, unsigned *p_w, unsigned *p_h,
                            unsigned *p_vw, unsigned *p_vh )
{
    unsigned CropUnitX = 1;
    unsigned CropUnitY = 2 - p_sps->frame_mbs_only_flag;
    if( p_sps->b_separate_colour_planes_flag != 1 )
    {
        if( p_sps->i_chroma_idc > 0 )
        {
            unsigned SubWidthC = 2;
            unsigned SubHeightC = 2;
            if( p_sps->i_chroma_idc > 1 )
            {
                SubHeightC = 1;
                if( p_sps->i_chroma_idc > 2 )
                    SubWidthC = 1;
            }
            CropUnitX *= SubWidthC;
            CropUnitY *= SubHeightC;
        }
    }

    *p_w = 16 * p_sps->pic_width_in_mbs_minus1 + 16;
    *p_h = 16 * p_sps->pic_height_in_map_units_minus1 + 16;
    *p_h *= ( 2 - p_sps->frame_mbs_only_flag );

    *p_vw = *p_w - ( p_sps->frame_crop.left_offset + p_sps->frame_crop.right_offset ) * CropUnitX;
    *p_vh = *p_h - ( p_sps->frame_crop.bottom_offset + p_sps->frame_crop.top_offset ) * CropUnitY;

    return true;
}

bool h264_get_chroma_luma( const h264_sequence_parameter_set_t *p_sps, uint8_t *pi_chroma_format,
                           uint8_t *pi_depth_luma, uint8_t *pi_depth_chroma )
{
    *pi_chroma_format = p_sps->i_chroma_idc;
    *pi_depth_luma = p_sps->i_bit_depth_luma;
    *pi_depth_chroma = p_sps->i_bit_depth_chroma;
    return true;
}
bool h264_get_colorimetry( const h264_sequence_parameter_set_t *p_sps,
                           video_color_primaries_t *p_primaries,
                           video_transfer_func_t *p_transfer,
                           video_color_space_t *p_colorspace,
                           video_color_range_t *p_full_range )
{
    if( !p_sps->vui.b_valid )
        return false;
    *p_primaries =
        iso_23001_8_cp_to_vlc_primaries( p_sps->vui.colour.i_colour_primaries );
    *p_transfer =
        iso_23001_8_tc_to_vlc_xfer( p_sps->vui.colour.i_transfer_characteristics );
    *p_colorspace =
        iso_23001_8_mc_to_vlc_coeffs( p_sps->vui.colour.i_matrix_coefficients );
    *p_full_range = p_sps->vui.colour.b_full_range ? COLOR_RANGE_FULL : COLOR_RANGE_LIMITED;
    return true;
}


bool h264_get_profile_level(const es_format_t *p_fmt, uint8_t *pi_profile,
                            uint8_t *pi_level, uint8_t *pi_nal_length_size)
{
    uint8_t *p = (uint8_t*)p_fmt->p_extra;
    if(p_fmt->i_extra < 8)
        return false;

    /* Check the profile / level */
    if (p[0] == 1 && p_fmt->i_extra >= 12)
    {
        if (pi_nal_length_size)
            *pi_nal_length_size = 1 + (p[4]&0x03);
        p += 8;
    }
    else if(!p[0] && !p[1]) /* FIXME: WTH is setting AnnexB data here ? */
    {
        if (!p[2] && p[3] == 1)
            p += 4;
        else if (p[2] == 1)
            p += 3;
        else
            return false;
    }
    else return false;

    if ( ((*p++)&0x1f) != 7) return false;

    if (pi_profile)
        *pi_profile = p[0];

    if (pi_level)
        *pi_level = p[2];

    return true;
}
