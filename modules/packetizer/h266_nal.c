/*****************************************************************************
 * Copyright © 2019 VideoLabs, VideoLAN and VLC Authors
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

#include "h266_nal.h"
#include "h26x_nal_common.h"
#include "hxxx_nal.h"
#include "hxxx_ep3b.h"
#include "iso_color_tables.h"

#include <vlc_common.h>
#include <vlc_bits.h>

#include <math.h>
#include <assert.h>

//#define H266_POC_DEBUG

#define H266_MAX_NUM_LAYER_OLSS 257
#define H266_MAX_NUM_DPB_PARAMS H266_MAX_NUM_LAYER_OLSS
#define H266_VPS_MAX_NUM_PTLS_MINUS1 (H266_MAX_NUM_LAYER_OLSS-1) /* < TotalNumOlss */
#define H266_DCI_MAX_NUM_PTLS_MINUS1 14
#define H266_MAX_SLICE_PER_AU 1000 /* Level 6.3 */

static bool h266_parse_general_constraint_info(bs_t *p_bs, h266_profile_tier_level_t *ptl)
{
    ptl->constraints_info.gci_present = bs_read1(p_bs);
    if(ptl->constraints_info.gci_present)
    {
        for(int i=0; i<8; i++)
            ptl->constraints_info.constraint_bytes[i] = bs_read(p_bs, 8);
        ptl->constraints_info.constraint_bytes[8] = bs_read(p_bs, 7);
        nal_u8_t gci_num_additional_bits = bs_read(p_bs, 8);
        if(gci_num_additional_bits > 5)
        {
            bs_read(p_bs, 6);
            if(gci_num_additional_bits < 6)
                return false;
            gci_num_additional_bits -= 6;
        }
        bs_skip(p_bs, gci_num_additional_bits);
    }

    bs_align(p_bs);

    return !bs_error(p_bs);
}


static bool h266_parse_profile_tier_level(bs_t *p_bs,
                                          nal_u1_t profileTierPresentFlag,
                                          nal_u8_t maxNumSubLayersMinus1,
                                          h266_profile_tier_level_t *p)
{
    if(maxNumSubLayersMinus1 >= H266_MAX_NUM_PTL_SUBLAYERS)
        return false;

    if(profileTierPresentFlag)
    {
        p->general_profile_idc = bs_read(p_bs, 7);
        p->general_tier_flag = bs_read1(p_bs);
    }
    p->general_level_idc = bs_read(p_bs, 8);
    p->ptl_frame_only_constraint_flag = bs_read(p_bs, 1);
    p->ptl_multilayer_enabled_flag = bs_read(p_bs, 1);

    if(profileTierPresentFlag &&
        !h266_parse_general_constraint_info(p_bs, p))
        return false;


    for(uint8_t i=0; i<maxNumSubLayersMinus1; i++)
        p->ptl_sublayer_level_present_flag[i] = bs_read1(p_bs);

    bs_align(p_bs);

    for(uint8_t i=0; i<maxNumSubLayersMinus1; i++)
    {
        if(p->ptl_sublayer_level_present_flag[i])
            p->ptl_sublayer_level_idc[i] = bs_read(p_bs, 8);
    }

    if(profileTierPresentFlag)
    {
        p->ptl_num_sub_profiles = bs_read(p_bs, 8);
        for(unsigned i=0; i<p->ptl_num_sub_profiles; i++)
            p->ptl_general_sub_profile_idc[i] = bs_read(p_bs, 32);
    }

    return !bs_error(p_bs);
}

typedef struct
{
    uint32_t num_units_in_tick;
    uint32_t time_scale;
    nal_u1_t general_nal_hrd_params_present_flag;
    nal_u1_t general_vcl_hrd_params_present_flag;
    nal_u1_t general_du_hrd_params_present_flag;
    nal_ue_t hrd_cpb_cnt_minus1;
} h266_general_hrd_parameters_t;

typedef struct
{
    nal_ue_t dpb_max_dec_pic_buffering_minus1;
    nal_ue_t dpb_max_num_reorder_pics;
    nal_ue_t dpb_max_latency_increase_plus1;
} h266_dpb_parameters_t;

typedef struct
{
    nal_u1_t progressive_source_flag;
    nal_u1_t interlaced_source_flag;

    nal_u1_t aspect_ratio_info_present_flag;
    nal_u1_t aspect_ratio_constant_flag;
    h26x_aspect_ratio_t ar;
    nal_u1_t overscan_info_present_flag;
    nal_u1_t overscan_appropriate_flag;

    nal_u1_t colour_description_present_flag;
    h26x_colour_description_t colour;

    nal_u1_t chroma_loc_info_present_flag;
    struct
    {
        nal_ue_t sample_loc_type_frame;
        nal_ue_t sample_loc_type_top_field;
        nal_ue_t sample_loc_type_bottom_field;
    } chroma;

} h266_vui_parameters_t;

struct h266_video_parameter_set_t
{
    nal_u4_t vps_video_parameter_set_id;
    nal_u6_t vps_max_layers_minus1;
    nal_u3_t vps_max_sub_layers_minus1;
    nal_u1_t vps_default_ptl_dpb_hrd_max_tid_flag;
    nal_u1_t vps_all_independent_layers_flag;
    nal_u8_t vps_num_output_layer_sets_minus2;
    nal_u1_t vps_each_layer_is_an_ols_flag;

    nal_u8_t vps_num_ptls_minus1;
    struct
    {
        nal_u1_t present_flag;
        nal_u3_t max_temporal_id;
    } vps_pt[H266_VPS_MAX_NUM_PTLS_MINUS1+1];

    nal_ue_t vps_num_dpb_params_minus1;
    h266_dpb_parameters_t dpb_parameters[H266_MAX_NUM_DPB_PARAMS][H266_MAX_NUM_PTL_SUBLAYERS];

    h266_profile_tier_level_t profile_tier_level[H266_VPS_MAX_NUM_PTLS_MINUS1+1];

    h266_general_hrd_parameters_t general_hrd_parameters;
};

struct h266_sequence_parameter_set_t
{
    nal_u4_t sps_decoding_parameter_set_id;
    nal_u4_t sps_video_parameter_set_id;
    nal_u3_t sps_max_sublayers_minus1;
    h266_profile_tier_level_t profile_tier_level[H266_MAX_NUM_PTL_SUBLAYERS];

    nal_u4_t sps_num_extra_ph_bits;

    nal_u2_t chroma_format_idc;
    nal_u1_t sps_ptl_dpb_hrd_params_present_flag;
    nal_ue_t pic_width_max_in_luma_samples;
    nal_ue_t pic_height_max_in_luma_samples;

    nal_u1_t sps_conformance_window_flag;
    h26x_conf_window_t conf_win;

    nal_ue_t sps_bitdepth_minus8;
    nal_u4_t sps_log2_max_pic_order_cnt_lsb_minus4;
    nal_u1_t sps_poc_msb_cycle_flag;
    nal_ue_t sps_poc_msb_cycle_len_minus1;

    h266_dpb_parameters_t dpb_parameters[H266_MAX_NUM_PTL_SUBLAYERS];

    h266_general_hrd_parameters_t general_hrd_parameters;
    nal_u1_t hrd_parameters_present_flag;

    nal_u1_t sps_weighted_pred_flag;
    nal_u1_t sps_weighted_bipred_flag;

    nal_u1_t sps_long_term_ref_pics_flag;
    nal_u1_t sps_inter_layer_prediction_enabled_flag;

    nal_u1_t vui_parameters_present_flag;
    h266_vui_parameters_t vui;
};

struct h266_picture_parameter_set_t
{
    nal_u6_t pps_pic_parameter_set_id;
    nal_u4_t pps_seq_parameter_set_id;
    nal_ue_t pps_pic_width_in_luma_samples;
    nal_ue_t pps_pic_heigth_in_luma_samples;
    nal_u1_t pps_conformance_window_flag;
    h26x_conf_window_t conf_win;
};

struct h266_decoding_capability_information_t
{
    nal_u4_t dci_num_ptls_minus1;
    h266_profile_tier_level_t profile_tier_level[H266_DCI_MAX_NUM_PTLS_MINUS1+1];
};

struct h266_picture_header_t
{
    enum h266_nal_unit_type_e nal_type;
    nal_u3_t nuh_temporal_id_plus1;
    nal_u1_t ph_gdr_or_irap_pic_flag;
    nal_u1_t ph_gdr_pic_flag;
    nal_u1_t ph_inter_slice_allowed_flag;
    nal_u1_t ph_intra_slice_allowed_flag;
    nal_u1_t ph_non_ref_pic_flag;
    nal_ue_t ph_pic_parameter_set_id;
    uint32_t ph_pic_order_cnt_lsb;
    nal_ue_t ph_recovery_poc_cnt;
    nal_u1_t ph_poc_msb_cycle_present_flag;
    nal_ue_t ph_poc_msb_cycle_val;
};

static bool h266_parse_general_hrd_parameters(bs_t *p_bs,
                                              h266_general_hrd_parameters_t *p_hrd)
{
    p_hrd->num_units_in_tick = bs_read(p_bs, 32);
    p_hrd->time_scale = bs_read(p_bs, 32);
    p_hrd->general_nal_hrd_params_present_flag = bs_read1(p_bs);
    p_hrd->general_vcl_hrd_params_present_flag = bs_read1(p_bs);
    if(p_hrd->general_nal_hrd_params_present_flag ||
        p_hrd->general_vcl_hrd_params_present_flag)
    {
        bs_skip(p_bs, 1);
        p_hrd->general_du_hrd_params_present_flag = bs_read1(p_bs);
        if(p_hrd->general_du_hrd_params_present_flag)
            bs_skip(p_bs, 8);
        bs_skip(p_bs, 4);
        bs_skip(p_bs, 4);
        if(p_hrd->general_nal_hrd_params_present_flag)
            bs_skip(p_bs, 4);
        p_hrd->hrd_cpb_cnt_minus1 = bs_read_ue(p_bs);
        if(p_hrd->hrd_cpb_cnt_minus1 > 31)
            return false;
    }

    return !bs_error(p_bs);
}

static bool h266_parse_sublayer_hrd_parameters(bs_t *p_bs,
                                               const h266_general_hrd_parameters_t *p_hrd)
{
    for(nal_ue_t j=0; j<= p_hrd->hrd_cpb_cnt_minus1; j++)
    {
        bs_read_ue(p_bs);
        bs_read_ue(p_bs);
        if(p_hrd->general_du_hrd_params_present_flag)
        {
            bs_read_ue(p_bs);
            bs_read_ue(p_bs);
        }
        bs_skip(p_bs, 1);
    }
    return !bs_error(p_bs);
}

static bool h266_parse_ols_timing_hrd_parameters(bs_t *p_bs,
                                                 unsigned firstSubLayer,
                                                 unsigned MaxSubLayersVal,
                                                 const h266_general_hrd_parameters_t *p_hrd)
{
    for(unsigned i=firstSubLayer; i<= MaxSubLayersVal; i++)
    {
        nal_u1_t fixed_pic_rate_within_cvs_flag = 0;
        if(!bs_read1(p_bs)) // fixed_pic_rate_general_flag
            fixed_pic_rate_within_cvs_flag = bs_read1(p_bs);
        if(fixed_pic_rate_within_cvs_flag)
            bs_read_ue(p_bs);
        else if((p_hrd->general_nal_hrd_params_present_flag ||
                  p_hrd->general_vcl_hrd_params_present_flag) && p_hrd->hrd_cpb_cnt_minus1 == 0)
            bs_skip(p_bs, 1);
        if(p_hrd->general_nal_hrd_params_present_flag)
            h266_parse_sublayer_hrd_parameters(p_bs, p_hrd);
        if(p_hrd->general_vcl_hrd_params_present_flag)
            h266_parse_sublayer_hrd_parameters(p_bs, p_hrd);
    }
    return !bs_error(p_bs);
}


static bool h266_parse_dpb_parameters(bs_t *p_bs,
                                      nal_u3_t sps_max_sublayers_minus1,
                                      nal_u1_t sublayer_info_flag,
                                      h266_dpb_parameters_t p[H266_MAX_NUM_PTL_SUBLAYERS])
{
    for(unsigned i=sublayer_info_flag ? 0 : sps_max_sublayers_minus1;
         i<= sps_max_sublayers_minus1; i++)
    {
        p[i].dpb_max_dec_pic_buffering_minus1 = bs_read_ue(p_bs);
        p[i].dpb_max_num_reorder_pics = bs_read_ue(p_bs);
        p[i].dpb_max_latency_increase_plus1 = bs_read_ue(p_bs);
    }
    return !bs_error(p_bs);
}

static bool h266_parse_vui_parameters(bs_t *p_bs, nal_ue_t payloadSize,
                                      h266_vui_parameters_t *p_vui)
{
    VLC_UNUSED(payloadSize);
    p_vui->progressive_source_flag = bs_read1(p_bs);
    p_vui->interlaced_source_flag = bs_read1(p_bs);
    bs_skip(p_bs, 2);
    p_vui->aspect_ratio_info_present_flag = bs_read1(p_bs);
    if(p_vui->aspect_ratio_info_present_flag)
    {
        p_vui->aspect_ratio_constant_flag = bs_read1(p_bs);
        p_vui->ar.aspect_ratio_idc = bs_read(p_bs, 8);
        if(p_vui->ar.aspect_ratio_idc == 255)
        {
            p_vui->ar.sar_width = bs_read(p_bs, 16);
            p_vui->ar.sar_height = bs_read(p_bs, 16);
        }
    }
    p_vui->overscan_info_present_flag = bs_read1(p_bs);
    if(p_vui->overscan_info_present_flag)
        p_vui->overscan_appropriate_flag = bs_read1(p_bs);
    p_vui->colour_description_present_flag = bs_read1(p_bs);
    if(p_vui->colour_description_present_flag)
    {
        p_vui->colour.colour_primaries = bs_read(p_bs, 8);
        p_vui->colour.transfer_characteristics = bs_read(p_bs, 8);
        p_vui->colour.matrix_coeffs = bs_read(p_bs, 8);
        p_vui->colour.full_range_flag = bs_read1(p_bs);
    }
    else
    {
        p_vui->colour.colour_primaries = ISO_23001_8_CP_UNSPECIFIED;
        p_vui->colour.transfer_characteristics = ISO_23001_8_TC_UNSPECIFIED;
        p_vui->colour.matrix_coeffs = ISO_23001_8_MC_UNSPECIFIED;
    }
    p_vui->chroma_loc_info_present_flag = bs_read1(p_bs);
    if(p_vui->chroma_loc_info_present_flag)
    {
        if(p_vui->progressive_source_flag && !p_vui->interlaced_source_flag)
            p_vui->chroma.sample_loc_type_frame = bs_read_ue(p_bs);
        else
        {
            p_vui->chroma.sample_loc_type_top_field = bs_read_ue(p_bs);
            p_vui->chroma.sample_loc_type_bottom_field = bs_read_ue(p_bs);
        }
    }
    return !bs_error(p_bs);
}

static bool h266_parse_video_parameter_set_rbsp(bs_t *p_bs,
                                                h266_video_parameter_set_t *p_vps)
{
    p_vps->vps_video_parameter_set_id = bs_read(p_bs, 4);
    if(p_vps->vps_video_parameter_set_id == 0)
        return false;
    p_vps->vps_max_layers_minus1 = bs_read(p_bs, 6);
    p_vps->vps_max_sub_layers_minus1 = bs_read(p_bs, 3);
    if(p_vps->vps_max_sub_layers_minus1 >= H266_MAX_NUM_PTL_SUBLAYERS)
        return false;
    if(p_vps->vps_max_layers_minus1)
    {
        if(p_vps->vps_max_sub_layers_minus1)
            p_vps->vps_default_ptl_dpb_hrd_max_tid_flag = bs_read1(p_bs);
        p_vps->vps_all_independent_layers_flag = bs_read1(p_bs);
    }
    for(nal_u6_t i=0; i<=p_vps->vps_max_layers_minus1; i++)
    {
        bs_skip(p_bs, 6);
        if(i > 0 && !p_vps->vps_all_independent_layers_flag)
        {
            if(!bs_read1(p_bs))
            {
                nal_u1_t vps_max_tid_ref_present_flag = bs_read1(p_bs);
                for(nal_u6_t j=0; j<i; j++)
                {
                    if(bs_read1(p_bs) && vps_max_tid_ref_present_flag)
                        bs_skip(p_bs, 3);
                }
            }
        }
    }

    nal_u2_t vps_ols_mode_idc = 0;
    if(p_vps->vps_max_layers_minus1)
    {
        if(p_vps->vps_all_independent_layers_flag)
            p_vps->vps_each_layer_is_an_ols_flag = bs_read1(p_bs);
        if(!p_vps->vps_each_layer_is_an_ols_flag)
        {
            if(!p_vps->vps_all_independent_layers_flag)
            {
                vps_ols_mode_idc = bs_read(p_bs, 2);
                if(vps_ols_mode_idc == 2)
                {
                    p_vps->vps_num_output_layer_sets_minus2 = bs_read(p_bs, 8);
                 for(unsigned i=1; i<= p_vps->vps_num_output_layer_sets_minus2 +1U; i++)
                     bs_skip(p_bs, 1 + p_vps->vps_max_layers_minus1);
                }
            }
        }
        p_vps->vps_num_ptls_minus1 = bs_read(p_bs, 8);
    }

    for(unsigned i=0; i<= p_vps->vps_num_ptls_minus1; i++)
    {
        if(i > 0)
            p_vps->vps_pt[i].present_flag = bs_read1(p_bs);
        else
            p_vps->vps_pt[i].present_flag = 1;
        if(!p_vps->vps_default_ptl_dpb_hrd_max_tid_flag)
            p_vps->vps_pt[i].max_temporal_id = bs_read(p_bs, 3);
    }

    bs_align(p_bs);

    for(unsigned i=0; i<= p_vps->vps_num_ptls_minus1; i++)
    {
        if(!h266_parse_profile_tier_level(p_bs,
                                            p_vps->vps_pt[i].present_flag,
                                            p_vps->vps_pt[i].max_temporal_id,
                                            &p_vps->profile_tier_level[i]))
            return false;
    }


    nal_u2_t ols_mode_idc = p_vps->vps_each_layer_is_an_ols_flag ? 4 : vps_ols_mode_idc;
    nal_ue_t TotalNumOlss;

    if (ols_mode_idc == 2)
        TotalNumOlss = p_vps->vps_num_output_layer_sets_minus2 + 2;
    else
        TotalNumOlss = p_vps->vps_max_layers_minus1 + 1;

    for(nal_ue_t i=0; i< TotalNumOlss; i++)
    {
        if(p_vps->vps_num_ptls_minus1 &&
            p_vps->vps_num_ptls_minus1 + 1U != TotalNumOlss)
            bs_skip(p_bs, 8); // vps_ols_ptl_idx[
    }

    if(!p_vps->vps_each_layer_is_an_ols_flag)
    {
        p_vps->vps_num_dpb_params_minus1 = bs_read_ue(p_bs);
        if(p_vps->vps_num_dpb_params_minus1 + 1 >= H266_MAX_NUM_DPB_PARAMS)
            return false;
        nal_u1_t vps_sublayer_dpb_params_present_flag = 0;
        if(p_vps->vps_num_dpb_params_minus1)
            vps_sublayer_dpb_params_present_flag = bs_read1(p_bs);
        unsigned VpsNumDpbParams = p_vps->vps_each_layer_is_an_ols_flag
                                 ? 0 : p_vps->vps_num_dpb_params_minus1 + 1;
        for(unsigned i=0; i< VpsNumDpbParams; i++)
        {
            nal_u3_t vps_dpb_max_tid = p_vps->vps_max_sub_layers_minus1;
            if(!p_vps->vps_default_ptl_dpb_hrd_max_tid_flag)
                vps_dpb_max_tid = bs_read(p_bs, 3);
            if(vps_dpb_max_tid > p_vps->vps_max_sub_layers_minus1)
                return false;
            if(!h266_parse_dpb_parameters(p_bs,
                                            vps_dpb_max_tid,
                                            vps_sublayer_dpb_params_present_flag,
                                            p_vps->dpb_parameters[i]))
                return false;
        }
    }

    return !bs_error(p_bs);
}

void h266_rbsp_release_vps(h266_video_parameter_set_t *p_vps)
{
    free(p_vps);
}

static bool h266_parse_pic_reflist(bs_t *p_bs, nal_ue_t sps_num_ref_pic_lists,
                                   unsigned listIdx, unsigned rplsIdx,
                                   h266_sequence_parameter_set_t *p_sps)
{
    VLC_UNUSED(listIdx);
    nal_ue_t numentries = bs_read_ue(p_bs);
    if(numentries > 16 + 13) // A.4.2 wrost case MaxDpbSize = 2 * maxDpbPicBuf(8)
        return false;
    nal_u1_t ltrp_in_header_flag = 0;
    if(p_sps->sps_long_term_ref_pics_flag &&
        rplsIdx < sps_num_ref_pic_lists && numentries)
        ltrp_in_header_flag = bs_read1(p_bs);

    for(nal_ue_t i=0; i<numentries; i++)
    {
        nal_u1_t inter_layer_ref_pic_flag = 0;
        if(p_sps->sps_inter_layer_prediction_enabled_flag)
            inter_layer_ref_pic_flag = bs_read1(p_bs);
        if(!inter_layer_ref_pic_flag)
        {
            nal_u1_t st_ref_pic_flag = 1;
            if(p_sps->sps_long_term_ref_pics_flag)
                st_ref_pic_flag = bs_read1(p_bs);
            if(st_ref_pic_flag)
            {
                nal_ue_t abs_delta_poc_st = bs_read_ue(p_bs);
                if(abs_delta_poc_st > (1<<15)-1)
                    return false;
                nal_ue_t AbsDeltaPocSt = abs_delta_poc_st;
                if(i == 0 || !(p_sps->sps_weighted_pred_flag || p_sps->sps_weighted_bipred_flag))
                    AbsDeltaPocSt++;
                if(AbsDeltaPocSt > 0)
                    bs_skip(p_bs, 1);
            }
            else if (!ltrp_in_header_flag)
            {
                bs_skip(p_bs, p_sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
            }
        }
        else
        {
            bs_read_ue(p_bs); // ilrp_idx
        }

    }

    return !bs_error(p_bs);
}

static bool h266_parse_sequence_parameter_set_rbsp(bs_t *p_bs,
                                                   h266_sequence_parameter_set_t *p_sps)
{
    p_sps->sps_decoding_parameter_set_id = bs_read(p_bs, 4);
    p_sps->sps_video_parameter_set_id = bs_read(p_bs, 4);
    p_sps->sps_max_sublayers_minus1 = bs_read(p_bs, 3);
    if(p_sps->sps_max_sublayers_minus1 >= H266_MAX_NUM_PTL_SUBLAYERS)
        return false;
    p_sps->chroma_format_idc = bs_read(p_bs, 2);
    nal_u2_t sps_log2_ctu_size_minus5 = bs_read(p_bs, 2);
    p_sps->sps_ptl_dpb_hrd_params_present_flag = bs_read(p_bs, 1);
    if(p_sps->sps_ptl_dpb_hrd_params_present_flag &&
        !h266_parse_profile_tier_level(p_bs,
                                        1,
                                        p_sps->sps_max_sublayers_minus1,
                                        p_sps->profile_tier_level))
        return false;

    const nal_u8_t CtbLog2SizeY = sps_log2_ctu_size_minus5 + 5;
    const unsigned CtbSizeY = 1 << CtbLog2SizeY;

    bs_skip(p_bs, 1); // sps_gdr_enabled_flag
    if(bs_read1(p_bs)) // sps_ref_pic_resampling_enabled_flag
        bs_skip(p_bs, 1); // sps_res_change_in_clvs_allowed_flag

    p_sps->pic_width_max_in_luma_samples = bs_read_ue(p_bs);
    p_sps->pic_height_max_in_luma_samples = bs_read_ue(p_bs);
    p_sps->sps_conformance_window_flag = bs_read1(p_bs);
    if(p_sps->sps_conformance_window_flag)
    {
        p_sps->conf_win.left_offset = bs_read_ue(p_bs);
        p_sps->conf_win.right_offset = bs_read_ue(p_bs);
        p_sps->conf_win.top_offset = bs_read_ue(p_bs);
        p_sps->conf_win.bottom_offset = bs_read_ue(p_bs);
    }

    if(bs_read1(p_bs)) // sps_subpic_info_present_flag
    {
        nal_ue_t sps_num_subpics_minus1 = bs_read_ue(p_bs);
        if(sps_num_subpics_minus1 >= H266_MAX_SLICE_PER_AU)
            return false;
        nal_u1_t sps_subpic_same_size_flag = 0;
        nal_u1_t sps_independent_subpics_flag = 0;
        if(sps_num_subpics_minus1)
        {
            sps_independent_subpics_flag = bs_read1(p_bs);
            sps_subpic_same_size_flag = bs_read1(p_bs);
        }

        for(nal_ue_t i=0; sps_num_subpics_minus1 > 0 && i <= sps_num_subpics_minus1; i++)
        {
            if(!sps_subpic_same_size_flag || i == 0)
            {
                unsigned tmpWidthVal = (p_sps->pic_width_max_in_luma_samples + CtbSizeY - 1) / CtbSizeY;
                unsigned tmpHeightVal = (p_sps->pic_height_max_in_luma_samples + CtbSizeY - 1) / CtbSizeY;
                uint8_t readsizeW = ceil(vlc_log2(tmpWidthVal));
                uint8_t readsizeH = ceil(vlc_log2(tmpHeightVal));
                if(i > 0 && p_sps->pic_width_max_in_luma_samples > CtbSizeY)
                    bs_read(p_bs, readsizeW);
                if(i > 0 && p_sps->pic_height_max_in_luma_samples > CtbSizeY)
                    bs_read(p_bs, readsizeH);
                if(i < sps_num_subpics_minus1 &&
                    p_sps->pic_width_max_in_luma_samples > CtbSizeY)
                    bs_read(p_bs, readsizeW);
                if(i < sps_num_subpics_minus1 &&
                    p_sps->pic_height_max_in_luma_samples > CtbSizeY)
                    bs_read(p_bs, readsizeH);
            }
            if(!sps_independent_subpics_flag)
            {
                bs_skip(p_bs, 1); // sps_subpic_treated_as_pic_flag[
                bs_skip(p_bs, 1); // sps_loop_filter_across_subpic_enabled_flag[
            }
        }

        nal_ue_t sps_subpic_id_len_minus1 = bs_read_ue(p_bs);
        if(sps_subpic_id_len_minus1 > 15)
            return false;
        if(bs_read(p_bs, 1))
        {
            if(bs_read(p_bs, 1)) // sps_subpic_id_mapping_present_flag
            {
                for(nal_ue_t i=0; i<= sps_num_subpics_minus1; i++)
                    bs_skip(p_bs, sps_subpic_id_len_minus1 + 1);
            }
        }
    }

    p_sps->sps_bitdepth_minus8 = bs_read_ue(p_bs);
    if(p_sps->sps_bitdepth_minus8 > 8)
        return false;

    bs_skip(p_bs, 2);
    p_sps->sps_log2_max_pic_order_cnt_lsb_minus4 = bs_read(p_bs, 4);
    if(p_sps->sps_log2_max_pic_order_cnt_lsb_minus4 > 12)
        return false;

    p_sps->sps_poc_msb_cycle_flag = bs_read(p_bs, 1);
    if(p_sps->sps_poc_msb_cycle_flag)
    {
        p_sps->sps_poc_msb_cycle_len_minus1 = bs_read_ue(p_bs);
        if(p_sps->sps_poc_msb_cycle_len_minus1 > (32U - 5) - p_sps->sps_log2_max_pic_order_cnt_lsb_minus4)
            return false;
    }

    for(uint8_t sps_num_extra_ph_bytes = bs_read(p_bs, 2) * 8; sps_num_extra_ph_bytes; sps_num_extra_ph_bytes--)
        p_sps->sps_num_extra_ph_bits += bs_read(p_bs, 1); // sps_extra_ph_bit_present_flag
    bs_skip(p_bs, bs_read(p_bs, 2) * 8); // sps_num_extra_sh_bytes

    if(p_sps->sps_ptl_dpb_hrd_params_present_flag)
    {
        nal_u1_t sps_sublayer_dpb_params_flag = 0;
        if(p_sps->sps_max_sublayers_minus1)
            sps_sublayer_dpb_params_flag = bs_read(p_bs, 1);
        if(!h266_parse_dpb_parameters(p_bs,
                                      p_sps->sps_max_sublayers_minus1,
                                      sps_sublayer_dpb_params_flag,
                                      p_sps->dpb_parameters))
            return false;
    }

    bs_read_ue(p_bs);
    bs_skip(p_bs, 1);
    bs_read_ue(p_bs);
    if(bs_read_ue(p_bs)) // sps_max_mtt_hierarchy_depth_intra_slice_luma
    {
        bs_read_ue(p_bs);
        bs_read_ue(p_bs);
    }

    nal_u1_t sps_qtbtt_dual_tree_intra_flag = 0;
    if(p_sps->chroma_format_idc)
        sps_qtbtt_dual_tree_intra_flag = bs_read1(p_bs);
    if(sps_qtbtt_dual_tree_intra_flag)
    {
        bs_read_ue(p_bs);
        if(bs_read_ue(p_bs)) // sps_max_mtt_hierarchy_depth_intra_slice_chroma
        {
            bs_read_ue(p_bs);
            bs_read_ue(p_bs);
        }
    }

    bs_read_ue(p_bs);
    if(bs_read_ue(p_bs)) // sps_max_mtt_hierarchy_depth_inter_slice
    {
        bs_read_ue(p_bs);
        bs_read_ue(p_bs);
    }

    nal_u1_t sps_max_luma_transform_size_64_flag = 0;
    if(CtbSizeY > 32)
        sps_max_luma_transform_size_64_flag = bs_read(p_bs, 1);

    nal_u1_t sps_transform_skip_enabled_flag = bs_read1(p_bs);
    if(sps_transform_skip_enabled_flag)
    {
        bs_read_ue(p_bs);
        bs_skip(p_bs, 1);
    }

    if(bs_read1(p_bs)) // sps_mts_enabled_flag
    {
        bs_skip(p_bs, 1);
        bs_skip(p_bs, 1);
    }

    nal_u1_t sps_lfnst_enabled_flag = bs_read(p_bs, 1);

    if(p_sps->chroma_format_idc)
    {
        nal_u1_t sps_joint_cbcr_enabled_flag = bs_read1(p_bs);
        nal_u1_t sps_same_qp_table_for_chroma_flag = bs_read1(p_bs);
        nal_u8_t numQpTables = sps_same_qp_table_for_chroma_flag ? 1 :
            (sps_joint_cbcr_enabled_flag ? 3 : 2);
        unsigned QpBdOffset = 6 * p_sps->sps_bitdepth_minus8;
        for(nal_u8_t i=0; i<numQpTables; i++)
        {
            nal_se_t sps_qp_table_start_minus26 = bs_read_se(p_bs);
            if(sps_qp_table_start_minus26 < (nal_se_t)(-26 - QpBdOffset) || sps_qp_table_start_minus26 > 36)
                return false;
            nal_ue_t sps_num_points_in_qp_table_minus1 = bs_read_ue(p_bs);
            if(sps_num_points_in_qp_table_minus1 > (nal_ue_t)(36 - sps_qp_table_start_minus26))
                return false;
            for(nal_ue_t j=0; j<= sps_num_points_in_qp_table_minus1; j++)
            {
                bs_read_ue(p_bs);
                bs_read_ue(p_bs);
            }
        }
    }

    bs_skip(p_bs, 1); // sps_sao_enabled_flag
    if(bs_read1(p_bs) && p_sps->chroma_format_idc) // sps_alf_enabled_flag
        bs_skip(p_bs, 1);

    bs_skip(p_bs, 1); // sps_lmcs_enabled_flag
    p_sps->sps_weighted_pred_flag = bs_read1(p_bs);
    p_sps->sps_weighted_bipred_flag = bs_read1(p_bs);

    p_sps->sps_long_term_ref_pics_flag = bs_read1(p_bs);

    if(p_sps->sps_video_parameter_set_id)
        p_sps->sps_inter_layer_prediction_enabled_flag = bs_read1(p_bs);

   bs_skip(p_bs, 1);
    nal_u1_t sps_rpl1_same_as_rpl0_flag = bs_read1(p_bs);
    for(nal_u8_t i=0; i< 2 - sps_rpl1_same_as_rpl0_flag; i++)
    {
        nal_ue_t sps_num_ref_pic_lists = bs_read_ue(p_bs);
        if(sps_num_ref_pic_lists > 64)
            return false;
        for(nal_ue_t j=0; j<sps_num_ref_pic_lists; j++)
            if(!h266_parse_pic_reflist(p_bs, sps_num_ref_pic_lists, i, j, p_sps))
                return false;
    }

    bs_skip(p_bs, 1);
    if(bs_read1(p_bs)) // sps_temporal_mvp_enabled_flag
        bs_skip(p_bs, 1);

    nal_u1_t sps_amvr_enabled_flag = bs_read(p_bs, 1);
    if(bs_read1(p_bs)) // sps_bdof_enabled_flag
        bs_skip(p_bs, 1);

    bs_skip(p_bs, 1);
    if(bs_read1(p_bs)) // sps_dmvr_enabled_flag
        bs_skip(p_bs, 1);

    if(bs_read1(p_bs)) // sps_mmvd_enabled_flag
        bs_skip(p_bs, 1);

     nal_ue_t sps_six_minus_max_num_merge_cand = bs_read_ue(p_bs);
    if(sps_six_minus_max_num_merge_cand > 6)
         return false;
     bs_skip(p_bs, 1);
     if(bs_read1(p_bs)) // sps_affine_enabled_flag
     {
         bs_read_ue(p_bs);
         bs_skip(p_bs, 1);
         if(sps_amvr_enabled_flag)
             bs_skip(p_bs, 1);
         if(bs_read1(p_bs)) // sps_affine_prof_enabled_flag
            bs_skip(p_bs, 1);
     }

     bs_skip(p_bs, 2);
     const unsigned MaxNumMergeCand = 6 - sps_six_minus_max_num_merge_cand;
     if(MaxNumMergeCand >= 2)
     {
         if(bs_read(p_bs, 1) && MaxNumMergeCand >= 3) // sps_gpm_enabled_flag
            bs_read_ue(p_bs);
     }

     nal_u1_t sps_act_enabled_flag = 0;
     bs_read_ue(p_bs);
     bs_skip(p_bs, 3);
     if(p_sps->chroma_format_idc)
         bs_skip(p_bs, 1);
     if(p_sps->chroma_format_idc == 1)
         bs_skip(p_bs, 2);
     nal_u1_t sps_palette_enabled_flag = bs_read(p_bs, 1);
     if(p_sps->chroma_format_idc == 3 && !!sps_max_luma_transform_size_64_flag)
         sps_act_enabled_flag = bs_read(p_bs, 1);
     if(sps_transform_skip_enabled_flag || sps_palette_enabled_flag)
         bs_read_ue(p_bs);
    if(bs_read1(p_bs)) // sps_ibc_enabled_flag
        bs_read_ue(p_bs);
    if(bs_read1(p_bs)) // sps_ladf_enabled_flag
    {
        nal_u2_t sps_num_ladf_intervals_minus2 = bs_read(p_bs, 2);
        bs_read_se(p_bs);
        for(nal_u8_t i=0; i<sps_num_ladf_intervals_minus2 + 1; i++)
        {
            bs_read_se(p_bs);
            bs_read_ue(p_bs);
        }
    }

    if(bs_read1(p_bs)) // sps_explicit_scaling_list_enabled_flag
    {
        if(sps_lfnst_enabled_flag)
            bs_skip(p_bs, 1);
        if(sps_act_enabled_flag)
            if(bs_read1(p_bs)) // sps_scaling_matrix_for_alternative_colour_space_disabled_flag
                bs_skip(p_bs, 1);
    }

    bs_skip(p_bs, 2);
    if(bs_read1(p_bs)) // sps_virtual_boundaries_enabled_flag
    {
        if(bs_read1(p_bs)) // sps_virtual_boundaries_present_flag
        {
            nal_ue_t sps_num_ver_virtual_boundaries = bs_read_ue(p_bs);
            if(sps_num_ver_virtual_boundaries > 3)
                return false;
            for(nal_ue_t i=0; i<sps_num_ver_virtual_boundaries; i++)
                bs_read_ue(p_bs);
            nal_ue_t sps_num_hor_virtual_boundaries = bs_read_ue(p_bs);
            if(sps_num_hor_virtual_boundaries > 3)
                return false;
            for(nal_ue_t i=0; i<sps_num_hor_virtual_boundaries; i++)
                bs_read_ue(p_bs);
        }
    }

    /* TIMINGS ! FINALLY !!! */
    if(p_sps->sps_ptl_dpb_hrd_params_present_flag)
    {
        if(bs_read1(p_bs)) // sps_timing_hrd_params_present_flag
        {
            if(!h266_parse_general_hrd_parameters(p_bs, &p_sps->general_hrd_parameters))
                return false;
            uint8_t firstSubLayer = p_sps->sps_max_sublayers_minus1;
            if(p_sps->sps_max_sublayers_minus1 && bs_read1(p_bs)) // sps_sublayer_cpb_params_present_flag
                firstSubLayer = 0;
            if(!h266_parse_ols_timing_hrd_parameters(p_bs, firstSubLayer,
                                                       p_sps->sps_max_sublayers_minus1,
                                                       &p_sps->general_hrd_parameters))
                return false;
        }
    }

    bs_skip(p_bs, 1);

    /* and VUI */
    p_sps->vui_parameters_present_flag = bs_read1(p_bs);
    if(p_sps->vui_parameters_present_flag)
    {
        nal_ue_t sps_vui_payload_size_minus1 = bs_read_ue(p_bs);
        bs_align(p_bs);
        h266_parse_vui_parameters(p_bs, sps_vui_payload_size_minus1 + 1,
                                   &p_sps->vui);
    }

    return !bs_error(p_bs);
}

void h266_rbsp_release_sps(h266_sequence_parameter_set_t *p_sps)
{
    free(p_sps);
}

static bool h266_parse_picture_parameter_set_rbsp(bs_t *p_bs,
                                                  h266_picture_parameter_set_t *p_pps)
{
    p_pps->pps_pic_parameter_set_id = bs_read(p_bs, 6);
    p_pps->pps_seq_parameter_set_id = bs_read(p_bs, 4);
    bs_skip(p_bs, 1);
    p_pps->pps_pic_width_in_luma_samples = bs_read_ue(p_bs);
    p_pps->pps_pic_heigth_in_luma_samples = bs_read_ue(p_bs);
    p_pps->pps_conformance_window_flag = bs_read1(p_bs);
    if(p_pps->pps_conformance_window_flag)
    {
        p_pps->conf_win.left_offset = bs_read_ue(p_bs);
        p_pps->conf_win.right_offset = bs_read_ue(p_bs);
        p_pps->conf_win.top_offset = bs_read_ue(p_bs);
        p_pps->conf_win.bottom_offset = bs_read_ue(p_bs);
    }
    return !bs_error(p_bs);
}

void h266_rbsp_release_pps(h266_picture_parameter_set_t *p_pps)
{
    free(p_pps);
}

static bool h266_parse_decoding_capability_information_rbsp(bs_t *p_bs,
                          h266_decoding_capability_information_t *p_dci)
{
    if(bs_read(p_bs, 4) != 0)
        return false;
    p_dci->dci_num_ptls_minus1 = bs_read(p_bs, 4);
    if(p_dci->dci_num_ptls_minus1 > H266_DCI_MAX_NUM_PTLS_MINUS1)
        return false;
    for(nal_u4_t i=0; i<=p_dci->dci_num_ptls_minus1; i++)
    {
        if(!h266_parse_profile_tier_level(p_bs, 1, 0,
                                           &p_dci->profile_tier_level[i]))
            return false;
    }
    return !bs_error(p_bs);
}

void h266_rbsp_release_dci(h266_decoding_capability_information_t *p_dci)
{
    free(p_dci);
}

#define IMPL_h266_generic_decode(name, type, decode, release) \
    type * name(const uint8_t *p_buf, size_t i_buf, bool b_escaped) \
    { \
        type *p_type = calloc(1, sizeof(type)); \
        if(likely(p_type)) \
        { \
            bs_t bs; \
            struct hxxx_bsfw_ep3b_ctx_s bsctx; \
            if(b_escaped) \
            { \
                hxxx_bsfw_ep3b_ctx_init(&bsctx); \
                bs_init_custom(&bs, p_buf, i_buf, &hxxx_bsfw_ep3b_callbacks, &bsctx);\
            } \
            else bs_init(&bs, p_buf, i_buf); \
            const uint8_t zeroes = bs_read(&bs, 2); /* nal_unit_header */ \
            const uint8_t i_nuh_layer_id = bs_read(&bs, 6); \
            bs_skip(&bs, 8); /* !nal_unit_header */ \
            if(zeroes || i_nuh_layer_id > 55 || !decode(&bs, p_type)) \
            { \
                release(p_type); \
                p_type = NULL; \
            } \
        } \
        return p_type; \
    }

IMPL_h266_generic_decode(h266_decode_vps, h266_video_parameter_set_t,
                          h266_parse_video_parameter_set_rbsp, h266_rbsp_release_vps)
IMPL_h266_generic_decode(h266_decode_sps, h266_sequence_parameter_set_t,
                          h266_parse_sequence_parameter_set_rbsp, h266_rbsp_release_sps)
IMPL_h266_generic_decode(h266_decode_pps, h266_picture_parameter_set_t,
                          h266_parse_picture_parameter_set_rbsp, h266_rbsp_release_pps)
IMPL_h266_generic_decode(h266_decode_dci, h266_decoding_capability_information_t,
                          h266_parse_decoding_capability_information_rbsp, h266_rbsp_release_dci)

static bool h266_parse_picture_header_rbsp(bs_t *p_bs,
                                           pf_get_matchedh266xps get_matchedxps, void *priv,
                                           h266_picture_header_t *ph)
{
    ph->ph_gdr_or_irap_pic_flag = bs_read1(p_bs);
    ph->ph_non_ref_pic_flag = bs_read1(p_bs);
    if(ph->ph_gdr_or_irap_pic_flag)
        ph->ph_gdr_pic_flag = bs_read1(p_bs);
    ph->ph_inter_slice_allowed_flag = bs_read1(p_bs);
    if(ph->ph_inter_slice_allowed_flag)
        ph->ph_intra_slice_allowed_flag = bs_read1(p_bs);
    else
        ph->ph_intra_slice_allowed_flag = true;
    ph->ph_pic_parameter_set_id = bs_read_ue(p_bs);
    if(ph->ph_pic_parameter_set_id > H266_PPS_ID_MAX)
        return false;
    h266_picture_parameter_set_t *p_pps;
    h266_sequence_parameter_set_t *p_sps;
    h266_video_parameter_set_t *p_vps;
    get_matchedxps(ph->ph_pic_parameter_set_id, priv, &p_pps, &p_sps, &p_vps);
    if(!p_sps)
        return false;
    ph->ph_pic_order_cnt_lsb = bs_read(p_bs, p_sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
    if(ph->ph_gdr_pic_flag)
        ph->ph_recovery_poc_cnt = bs_read_ue(p_bs);
    bs_read(p_bs, p_sps->sps_num_extra_ph_bits);
    if(p_sps->sps_poc_msb_cycle_flag)
    {
        ph->ph_poc_msb_cycle_present_flag = bs_read(p_bs, 1);
        if(ph->ph_poc_msb_cycle_present_flag)
            ph->ph_poc_msb_cycle_val = bs_read(p_bs, p_sps->sps_poc_msb_cycle_len_minus1 + 1);
    }
    return !bs_error(p_bs);
}

void h266_rbsp_release_picture_header(h266_picture_header_t *ph)
{
    free(ph);
}

static h266_picture_header_t * h266_decode_pictureorslice_header(const uint8_t *p_buf, size_t i_buf, bool b_escaped,
                                                                 pf_get_matchedh266xps get_matchedxps, void *priv,
                                bool (*pf_parse_rbsp)(bs_t *, pf_get_matchedh266xps, void *, h266_picture_header_t *))
{
    h266_picture_header_t *p_ph = calloc(1, sizeof(*p_ph));
    if(likely(p_ph))
    {
        bs_t bs;
        struct hxxx_bsfw_ep3b_ctx_s bsctx;
        if(b_escaped)
        {
            hxxx_bsfw_ep3b_ctx_init(&bsctx);
            bs_init_custom(&bs, p_buf, i_buf, &hxxx_bsfw_ep3b_callbacks, &bsctx);
        }
        else bs_init(&bs, p_buf, i_buf);
        bs_skip(&bs, 8);
        p_ph->nal_type = bs_read(&bs, 5);
        p_ph->nuh_temporal_id_plus1 = bs_read(&bs, 3);
        if(!pf_parse_rbsp(&bs, get_matchedxps, priv, p_ph))
        {
            h266_rbsp_release_picture_header(p_ph);
            p_ph = NULL;
        }
    }
    return p_ph;
}

h266_picture_header_t * h266_decode_picture_header(const uint8_t *p_buf, size_t i_buf, bool b_escaped,
                                                   pf_get_matchedh266xps get_matchedxps, void *priv)
{
    return h266_decode_pictureorslice_header(p_buf, i_buf, b_escaped, get_matchedxps, priv,
                                             h266_parse_picture_header_rbsp);
}

static bool h266_parse_slice_header_rbsp(bs_t *p_bs,
                                         pf_get_matchedh266xps get_matchedxps, void *priv,
                                         h266_picture_header_t *ph)
{
    if(!bs_read1(p_bs)) // sh_picture_header_in_slice_header_flag
        return false;
    return h266_parse_picture_header_rbsp(p_bs, get_matchedxps, priv, ph);
}

h266_picture_header_t * h266_decode_slice_header(const uint8_t *p_buf, size_t i_buf, bool b_escaped,
                                                 pf_get_matchedh266xps get_matchedxps, void *priv)
{
    return h266_decode_pictureorslice_header(p_buf, i_buf, b_escaped, get_matchedxps, priv,
                                             h266_parse_slice_header_rbsp);
}

uint8_t h266_get_sps_vps_id(const h266_sequence_parameter_set_t *p_sps)
{
    return p_sps->sps_video_parameter_set_id;
}

uint8_t h266_get_pps_sps_id(const h266_picture_parameter_set_t *p_pps)
{
    return p_pps->pps_seq_parameter_set_id;
}

uint8_t h266_get_picture_header_pps_id(const h266_picture_header_t *ph)
{
    return ph->ph_pic_parameter_set_id;
}

uint32_t h266_get_picture_header_poc_lsb(const h266_picture_header_t *ph)
{
    return ph->ph_pic_order_cnt_lsb;
}

/* Shortcut for retrieving vps/sps/pps id */
bool h266_get_xps_id(const uint8_t *p_buf, size_t i_buf, uint8_t *pi_id)
{
    if(i_buf < 3)
        return false;
    /* No need to lookup convert from emulation for that data */
    uint8_t i_nal_type = h266_getNALType(p_buf);
    bs_t bs;
    bs_init(&bs, &p_buf[2], i_buf - 2);
    if(i_nal_type == H266_NAL_PPS)
    {
        *pi_id = bs_read(&bs, 6);
        if(*pi_id > H266_PPS_ID_MAX)
            return false;
    }
    else
    {
        *pi_id = bs_read(&bs, 4);
        if(i_nal_type == H266_NAL_SPS)
        {
            if(*pi_id > H266_SPS_ID_MAX)
                return false;
        }
        else if(*pi_id > H266_VPS_ID_MAX || *pi_id == 0)
            return false;
    }
    return true;
}

bool h266_get_profile_tier_level(const h266_decoding_capability_information_t *p_dci,
                                 const h266_sequence_parameter_set_t *p_sps,
                                 uint8_t *pi_profile, uint8_t *pi_level)
{
    const h266_profile_tier_level_t *ptl;
    if(p_dci)
    {
        ptl = &p_dci->profile_tier_level[0];
    }
    else if(p_sps && p_sps->sps_ptl_dpb_hrd_params_present_flag)
    {
        ptl = &p_sps->profile_tier_level[0];
    }
    else return false;
    *pi_profile = ptl->general_profile_idc;
    *pi_level = ptl->general_level_idc;
    return true;
}

bool h266_get_picture_size(const h266_sequence_parameter_set_t *p_sps,
                           unsigned *p_ox, unsigned *p_oy,
                           unsigned *p_w, unsigned *p_h,
                           unsigned *p_vw, unsigned *p_vh)
{
    return h26x_get_picture_size(p_sps->chroma_format_idc,
                                 p_sps->pic_width_max_in_luma_samples,
                                 p_sps->pic_height_max_in_luma_samples,
                                 &p_sps->conf_win,
                                 p_ox, p_oy, p_w, p_h, p_vw, p_vh);
}

bool h266_get_frame_rate(const h266_sequence_parameter_set_t *p_sps,
                         const h266_video_parameter_set_t *p_vps,
                         unsigned *pi_num, unsigned *pi_den)
{
    VLC_UNUSED(p_vps);
    if(p_sps->general_hrd_parameters.num_units_in_tick &&
        p_sps->general_hrd_parameters.time_scale)
    {
        *pi_den = p_sps->general_hrd_parameters.num_units_in_tick;
        *pi_num = p_sps->general_hrd_parameters.time_scale;
        return (*pi_den && *pi_num);
    }
    return false;
}

bool h266_get_aspect_ratio(const h266_sequence_parameter_set_t *p_sps,
                           unsigned *num, unsigned *den)
{
    if(!p_sps->vui.aspect_ratio_info_present_flag)
        return false;
    return h26x_get_aspect_ratio(&p_sps->vui.ar, num, den);
}


bool h266_get_chroma_luma(const h266_sequence_parameter_set_t *p_sps,
                          uint8_t *pi_chroma_format,
                          uint8_t *pi_depth_luma, uint8_t *pi_depth_chroma)
{
    *pi_chroma_format = p_sps->chroma_format_idc;
    *pi_depth_luma = p_sps->sps_bitdepth_minus8 + 8;
    *pi_depth_chroma = p_sps->sps_bitdepth_minus8 + 8;
    return true;
}

bool h266_get_colorimetry(const h266_sequence_parameter_set_t *p_sps,
                          video_color_primaries_t *p_primaries,
                          video_transfer_func_t *p_transfer,
                          video_color_space_t *p_colorspace,
                          video_color_range_t *p_full_range)
{
    if(!p_sps->vui_parameters_present_flag)
        return false;
    return h26x_get_colorimetry(&p_sps->vui.colour,
                                p_primaries, p_transfer, p_colorspace, p_full_range);
}

bool h266_get_slice_type(const h266_picture_header_t *ph, enum h266_slice_type_e *type)
{
    if(!ph->ph_inter_slice_allowed_flag)
        *type = H266_SLICE_TYPE_I;
    else
        *type = H266_SLICE_TYPE_UNK;
    return true;
}

/*
 * 8.3.1 Decoding process for POC
 */
int h266_compute_picture_order_count(const h266_sequence_parameter_set_t *p_sps,
                                     const h266_picture_header_t *p_ph,
                                     h266_poc_ctx_t *p_ctx)
{
    struct
    {
        int lsb;
        int msb;
    } prevPicOrderCnt;
    int PicOrderCntMsb;

    // FIXME HandleCraAsClvsStartFlag
    const bool NoOutputBeforeRecoveryFlag = p_ctx->first_picture ||
                                            p_ph->nal_type == H266_NAL_IDR_W_RADL ||
                                            p_ph->nal_type == H266_NAL_IDR_N_LP ||
                                            p_ctx->HandleCraAsClvsStartFlag;
    /* coded layer video sequence start */
    const bool IsCLVSS = p_ph->nal_type >= H266_NAL_IDR_W_RADL &&
                         p_ph->nal_type <= H266_NAL_RSV_IRAP_11 &&
                         NoOutputBeforeRecoveryFlag;

    const unsigned MaxPicOrderCntLsb = 1U << (p_sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);

#ifdef H266_POC_DEBUG
    fprintf(stderr, "POC lsb=%"PRIu32" pocmsb %d CLVSS=%d nooutput=%d prevmsb=%d prevlsb=%d\n",
            p_ph->ph_pic_order_cnt_lsb, p_ph->ph_poc_msb_cycle_present_flag,
            IsCLVSS, NoOutputBeforeRecoveryFlag,
            p_ctx->prevTid0PicOrderCnt.msb, p_ctx->prevTid0PicOrderCnt.lsb);
#endif

    if(!p_ph->ph_poc_msb_cycle_present_flag && !IsCLVSS)
    {
        prevPicOrderCnt.lsb = p_ctx->prevTid0PicOrderCnt.lsb;
        prevPicOrderCnt.msb = p_ctx->prevTid0PicOrderCnt.msb;
    }

    if(p_ph->ph_poc_msb_cycle_present_flag)
    {
        PicOrderCntMsb = p_ph->ph_poc_msb_cycle_val * MaxPicOrderCntLsb;
    }
    else if(IsCLVSS)
    {
        PicOrderCntMsb = 0;
    }
    else
    {
        PicOrderCntMsb = prevPicOrderCnt.msb;
        int64_t orderDiff = (int64_t)p_ph->ph_pic_order_cnt_lsb - prevPicOrderCnt.lsb;
        if(orderDiff < 0 && -orderDiff >= MaxPicOrderCntLsb / 2)
            PicOrderCntMsb += MaxPicOrderCntLsb;
        else if(orderDiff > MaxPicOrderCntLsb / 2)
            PicOrderCntMsb -= MaxPicOrderCntLsb;
    }

    if(p_ph->nuh_temporal_id_plus1 == 1 && !p_ph->ph_non_ref_pic_flag &&
       p_ph->nal_type != H266_NAL_RADL && p_ph->nal_type != H266_NAL_RASL)
    {
        p_ctx->prevTid0PicOrderCnt.msb = PicOrderCntMsb;
        p_ctx->prevTid0PicOrderCnt.lsb = p_ph->ph_pic_order_cnt_lsb;
    }

    p_ctx->first_picture = false;

#ifdef H266_POC_DEBUG
    fprintf(stderr, " POC=%"PRIu32"\n", PicOrderCntMsb + p_ph->ph_pic_order_cnt_lsb);
#endif

    return PicOrderCntMsb + p_ph->ph_pic_order_cnt_lsb;
}

/* VVCDecoderConfigurationRecord */

#define H266_DCR_ADD_NALS(type, count, buffers, sizes) \
for (uint8_t i = 0; i < count; i++) \
    { \
        if(i ==0) \
        { \
                *p++ = (type | (b_completeness ? 0x80 : 0)); \
                SetWBE(p, count); p += 2; \
        } \
            SetWBE(p, sizes[i]); p += 2; \
            memcpy(p, buffers[i], sizes[i]); p += sizes[i];\
    }

#define H266_DCR_ADD_SIZES(count, sizes) \
if(count > 0) \
    {\
            i_total_size += 3;\
            for(uint8_t i=0; i<count; i++)\
            i_total_size += 2 + sizes[i];\
    }

uint8_t * h266_create_DecoderConfigurationRecord(const struct h266_dcr_params *p_params,
                                                 bool b_completeness, size_t *pi_size)
{
    *pi_size = 0;

    struct h266_dcr_values values =
    {
        .nal_length_size = 4,
        .ptl_present_flag = 0,
        .constant_frame_rate = 1,
        .avg_frame_rate = 0,
    };

    if(p_params->p_values != NULL)
    {
        values = *p_params->p_values;
    }
    else /* extract from SPS */
    {
        if(p_params->i_sps_count == 0)
            return NULL; /* required to extract info */

        h266_sequence_parameter_set_t *p_sps =
            h266_decode_sps(p_params->p_sps[0], p_params->rgi_sps[0], true);
        if(!p_sps)
            return NULL;

        values.ptl_present_flag = p_sps->sps_ptl_dpb_hrd_params_present_flag;
        values.ols_idx = 0; //spec ? FIXME
        values.num_sublayers = p_sps->sps_max_sublayers_minus1 + 1;
        values.constant_frame_rate = 1;
        values.chroma_format_idc = p_sps->chroma_format_idc;
        values.bit_depth_minus8 = p_sps->sps_bitdepth_minus8;
        values.ptl = p_sps->profile_tier_level[0];

        unsigned sz[6] = {0};
        h266_get_picture_size(p_sps, &sz[0], &sz[1], &sz[2], &sz[3], &sz[4], &sz[5]);
        values.max_picture_width = sz[4];
        values.max_picture_height = sz[5];
        values.avg_frame_rate = 0;

        h266_rbsp_release_sps(p_sps);
    }

    if(values.nal_length_size != 1 &&
       values.nal_length_size != 2 &&
       values.nal_length_size != 4)
        return NULL;

    const uint8_t num_bytes_constraint_info = h266_gci_num_constraint_bytes(&values.ptl);
    size_t i_total_size = 1;

    size_t i_ptl_size;
    if(values.ptl_present_flag)
    {
        i_ptl_size = 6 + __MAX(1, num_bytes_constraint_info);
        if(values.num_sublayers > 1)
        {
            i_ptl_size += 1;
            for(uint8_t i=0; i<values.num_sublayers - 1; i++)
                i_ptl_size += !!values.ptl.ptl_sublayer_level_present_flag[i];
        }
        i_ptl_size += 1 + values.ptl.ptl_num_sub_profiles * 4;
        i_ptl_size += 6;
        i_total_size += i_ptl_size;
    }
    else i_ptl_size = 0;

    i_total_size += 1;

    if(p_params->i_dci)
        i_total_size += 1 + sizeof(uint16_t) + p_params->i_dci;
    if(p_params->i_opi)
        i_total_size += 1 + sizeof(uint16_t) + p_params->i_opi;
    H266_DCR_ADD_SIZES(p_params->i_vps_count, p_params->rgi_vps);
    H266_DCR_ADD_SIZES(p_params->i_sps_count, p_params->rgi_sps);
    H266_DCR_ADD_SIZES(p_params->i_pps_count, p_params->rgi_pps);
    H266_DCR_ADD_SIZES(p_params->i_seipref_count, p_params->rgi_seipref);
    H266_DCR_ADD_SIZES(p_params->i_seisuff_count, p_params->rgi_seisuff);

    uint8_t *p_data = calloc(1, i_total_size);
    if(p_data == NULL)
        return NULL;

    *pi_size = i_total_size;
    uint8_t *p = p_data;

    p[0] = 0xF8 | (values.nal_length_size - 1) << 1 | values.ptl_present_flag;
    if(values.ptl_present_flag)
    {
        bs_t bs;
        bs_write_init(&bs, &p_data[1], i_ptl_size);
        bs_write(&bs, 9, values.ols_idx);
        bs_write(&bs, 3, values.num_sublayers);
        bs_write(&bs, 2, values.constant_frame_rate);
        bs_write(&bs, 2, values.chroma_format_idc);
        bs_write(&bs, 3, values.bit_depth_minus8);
        bs_write(&bs, 5, 0xff); // reserved
        /* PTL */
        bs_write(&bs, 2, 0x00); // reserved
        bs_write(&bs, 6, __MAX(1, num_bytes_constraint_info));
        bs_write(&bs, 7, values.ptl.general_profile_idc);
        bs_write(&bs, 1, values.ptl.general_tier_flag);
        bs_write(&bs, 8, values.ptl.general_level_idc);

        bs_write(&bs, 1, values.ptl.ptl_frame_only_constraint_flag);
        bs_write(&bs, 1, values.ptl.ptl_multilayer_enabled_flag);
        if(num_bytes_constraint_info)
        {
            for (int i = 0; i < num_bytes_constraint_info; i++)
                bs_write(&bs,
                         (i + 1 < num_bytes_constraint_info) ? 8 : 6,
                         values.ptl.constraints_info.constraint_bytes[i]);
        }
        else bs_write(&bs, 6, 0);
        assert(bs_aligned(&bs));

        if(values.num_sublayers > 1)
        {
            bs_write(&bs, 8 - (values.num_sublayers - 1), 0x00);
            for (int i = 0; i < values.num_sublayers - 1; i++)
                bs_write(&bs, 1, values.ptl.ptl_sublayer_level_present_flag[i]);
        }

        for (int i = 0; i < values.num_sublayers - 1; i++)
            if(values.ptl.ptl_sublayer_level_present_flag[i])
                bs_write(&bs, 8, values.ptl.ptl_sublayer_level_idc[i]);
        assert(bs_aligned(&bs));

        bs_write(&bs, 8, values.ptl.ptl_num_sub_profiles);

        /* unsigned int(32) general_sub_profile_idc[j]; */
        for (int i = 0; i < values.ptl.ptl_num_sub_profiles; i++)
            bs_write(&bs, 32, values.ptl.ptl_general_sub_profile_idc[i]);

        bs_write(&bs, 16, values.max_picture_width);
        bs_write(&bs, 16, values.max_picture_height);
        bs_write(&bs, 16, values.avg_frame_rate);
    }

    p += 1 + i_ptl_size;

    /* num arrays */
    *p++ = !!p_params->i_opi + !!p_params->i_dci +
           !!p_params->i_vps_count + !!p_params->i_sps_count + !!p_params->i_pps_count +
           !!p_params->i_seipref_count + !!p_params->i_seisuff_count;

    /* Write NAL arrays */
    if(p_params->i_opi) // Size for that single unit
    {
        p[0] = (H266_NAL_OPI | (b_completeness ? 0x80 : 0));
        SetWBE(&p[1], p_params->i_opi);
        memcpy(&p[3], p_params->p_opi, p_params->i_opi);
        p += 3 + p_params->i_opi;
    }
    if(p_params->i_dci) // Size for that single unit
    {
        p[0] = (H266_NAL_DCI | (b_completeness ? 0x80 : 0));
        SetWBE(&p[1], p_params->i_dci);
        memcpy(&p[3], p_params->p_dci, p_params->i_dci);
        p += 3 + p_params->i_dci;
    }
    H266_DCR_ADD_NALS(H266_NAL_VPS, p_params->i_vps_count,
                      p_params->p_vps, p_params->rgi_vps);
    H266_DCR_ADD_NALS(H266_NAL_SPS, p_params->i_sps_count,
                      p_params->p_sps, p_params->rgi_sps);
    H266_DCR_ADD_NALS(H266_NAL_PPS, p_params->i_pps_count,
                      p_params->p_pps, p_params->rgi_pps);
    H266_DCR_ADD_NALS(H266_NAL_PREFIX_SEI, p_params->i_seipref_count,
                      p_params->p_seipref, p_params->rgi_seipref);
    H266_DCR_ADD_NALS(H266_NAL_SUFFIX_SEI, p_params->i_seisuff_count,
                      p_params->p_seisuff, p_params->rgi_seisuff);

    return p_data;
}

void h266_add_NALtoParams(const uint8_t *p_nal, size_t i_nal,
                          struct h266_dcr_params *p_params)
{
    if(i_nal < 2 || i_nal > UINT16_MAX)
        return;

    switch (h266_getNALType(p_nal))
    {
    case H266_NAL_OPI:
        p_params->p_opi = p_nal;
        p_params->i_opi = i_nal;
        break;
    case H266_NAL_DCI:
        p_params->p_dci = p_nal;
        p_params->i_dci = i_nal;
        break;
    case H266_NAL_VPS:
        if(p_params->i_vps_count != H266_MAX_NUM_VPS)
        {
            p_params->p_vps[p_params->i_vps_count] = p_nal;
            p_params->rgi_vps[p_params->i_vps_count++] = i_nal;
        }
        break;
    case H266_NAL_SPS:
        if(p_params->i_sps_count != H266_MAX_NUM_SPS)
        {
            p_params->p_sps[p_params->i_sps_count] = p_nal;
            p_params->rgi_sps[p_params->i_sps_count++] = i_nal;
        }
        break;
    case H266_NAL_PPS:
        if(p_params->i_pps_count != H266_MAX_NUM_PPS)
        {
            p_params->p_pps[p_params->i_pps_count] = p_nal;
            p_params->rgi_pps[p_params->i_pps_count++] = i_nal;
        }
        break;
    case H266_NAL_PREFIX_SEI:
        if(p_params->i_seipref_count != H266_MAX_NUM_SEI)
        {
            p_params->p_seipref[p_params->i_seipref_count] = p_nal;
            p_params->rgi_seipref[p_params->i_seipref_count++] = i_nal;
        }
        break;
    case H266_NAL_SUFFIX_SEI:
        if(p_params->i_seisuff_count != H266_MAX_NUM_SEI)
        {
            p_params->p_seisuff[p_params->i_seisuff_count] = p_nal;
            p_params->rgi_seisuff[p_params->i_seisuff_count++] = i_nal;
        }
        break;

    default:
        break;
    }
}

bool h266_parse_DecoderConfigurationRecord(const uint8_t *p_buf, size_t i_buf,
                                           struct h266_dcr_params *p_params)
{
    struct h266_dcr_values *p_values = p_params->p_values;
    if(!p_values)
        return false;
    { // force bs unscoping
        bs_t bs;
        bs_init(&bs, p_buf, i_buf);
        bs_skip(&bs, 5);
        p_values->nal_length_size = bs_read(&bs, 2) + 1;
        if(p_values->nal_length_size == 3)
            return false;

        p_values->ptl_present_flag = bs_read(&bs, 1);
        if(p_values->ptl_present_flag)
        {
            h266_profile_tier_level_t *ptl = &p_values->ptl;
            p_values->ols_idx = bs_read(&bs, 9);
            p_values->num_sublayers = bs_read(&bs, 3);
            p_values->constant_frame_rate = bs_read(&bs, 2);
            p_values->chroma_format_idc = bs_read(&bs, 2);
            p_values->bit_depth_minus8 = bs_read(&bs, 3);
            bs_skip(&bs, 5); // reserved

            bs_skip(&bs, 2); // reserved
            uint8_t num_bytes_constraint_info = bs_read(&bs, 6);
            if(num_bytes_constraint_info > H266_MAX_CONSTRAINT_BYTES)
                return false;
            /* Spec ? Is there any zero bits compression ? (<H266_MIN_CONSTRAINT_BITS) */
            ptl->constraints_info.gci_present = num_bytes_constraint_info > 1;
            if(num_bytes_constraint_info * 8U > H266_MIN_CONSTRAINT_BITS)
                ptl->constraints_info.gci_num_additional_bits = num_bytes_constraint_info * 8U - H266_MIN_CONSTRAINT_BITS;
            else
                ptl->constraints_info.gci_num_additional_bits = 0;

            ptl->general_profile_idc = bs_read(&bs, 7);
            ptl->general_tier_flag = bs_read(&bs, 1);
            ptl->general_level_idc = bs_read(&bs, 8);

            ptl->ptl_frame_only_constraint_flag = bs_read(&bs, 1);
            ptl->ptl_multilayer_enabled_flag = bs_read(&bs, 1);
            if(num_bytes_constraint_info)
            {
                for(uint8_t i=0; i<num_bytes_constraint_info - 1; i++)
                    ptl->constraints_info.constraint_bytes[i] = bs_read(&bs, 8);
                ptl->constraints_info.constraint_bytes[num_bytes_constraint_info - 1] = bs_read(&bs, 6);
            }
            else bs_skip(&bs, 6);

            assert(bs_aligned(&bs));
            if(p_values->num_sublayers > 1)
            {
                bs_skip(&bs, 8 - (p_values->num_sublayers-1));
                for(uint8_t i=0; i<p_values->num_sublayers - 1; i++)
                    ptl->ptl_sublayer_level_present_flag[i] = bs_read(&bs, 1);
                for(uint8_t i=0; i<p_values->num_sublayers - 1; i++)
                    if(ptl->ptl_sublayer_level_present_flag[i])
                        ptl->ptl_sublayer_level_idc[i] = bs_read(&bs, 8);
                assert(bs_aligned(&bs));
            }
            ptl->ptl_num_sub_profiles = bs_read(&bs, 8);
            for(uint8_t i=0; i<ptl->ptl_num_sub_profiles; i++)
                ptl->ptl_general_sub_profile_idc[i] = bs_read(&bs, 32);
            p_values->max_picture_width = bs_read(&bs, 16);
            p_values->max_picture_height = bs_read(&bs, 16);
            p_values->avg_frame_rate = bs_read(&bs, 16);
        }
        if(!bs_aligned(&bs) || bs_eof(&bs))
            return false;
        p_buf += bs_pos(&bs) / 8;
        i_buf -= bs_pos(&bs) / 8;
    }

    const uint8_t i_num_array = p_buf[0];
    p_buf++; i_buf--;
    for(uint8_t i = 0; i < i_num_array; i++)
    {
        if(i_buf < 3)
            return false;

        enum h266_nal_unit_type_e type = *p_buf & 0x1f;
        uint16_t num_nal = 1;
        p_buf += 1; i_buf -= 1;
        if(type != H266_NAL_OPI && type != H266_NAL_DCI)
        {
            if(i_buf < 3)
                return false;
            num_nal = GetWBE(p_buf);
            p_buf += 2; i_buf -= 2;
        }

        for(uint16_t j=0; j<num_nal; j++)
        {
            if(i_buf < 2)
                return false;

            const uint16_t i_nalu_length = GetWBE(p_buf);
            if(i_buf < (size_t)i_nalu_length + 2)
                return false;

            h266_add_NALtoParams(&p_buf[2], i_nalu_length, p_params);

            p_buf += i_nalu_length + 2;
            i_buf -= i_nalu_length + 2;
        }
    }

    return true;
}

uint8_t * h266_create_AnnexbExtradataFromParams(const struct h266_dcr_params *p_params, size_t *pi_size)
{
    size_t i_total_size = 0;

    struct
    {
        const uint8_t * const *pp_xps;
        const uint16_t *p_sizes;
        uint8_t i_count;
    } const xps[] = {
        { &p_params->p_opi, &p_params->i_opi, !!p_params->i_opi },
        { &p_params->p_dci, &p_params->i_dci, !!p_params->i_dci },
        { p_params->p_vps, p_params->rgi_vps, p_params->i_vps_count },
        { p_params->p_sps, p_params->rgi_sps, p_params->i_sps_count },
        { p_params->p_pps, p_params->rgi_pps, p_params->i_pps_count },
        { p_params->p_seipref, p_params->rgi_seipref, p_params->i_seipref_count },
        { p_params->p_seisuff, p_params->rgi_seisuff, p_params->i_seisuff_count },
    };

    for(unsigned i=0; i<ARRAY_SIZE(xps); i++)
        for(uint8_t j=0; j<xps[i].i_count; j++)
            i_total_size += 4 + xps[i].p_sizes[j];

    uint8_t *p = malloc(i_total_size), *w = p;
    if(!p)
        return NULL;

    for(unsigned i=0; i<ARRAY_SIZE(xps); i++)
    {
        for(uint8_t j=0; j<xps[i].i_count; j++)
        {
            SetDWBE(w, 1);
            memcpy(&w[4], xps[i].pp_xps[j], xps[i].p_sizes[j]);
            w += 4 + xps[i].p_sizes[j];
        }
    }
    *pi_size = i_total_size;
    return p;
}
