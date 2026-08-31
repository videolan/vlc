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
#ifndef H266_NAL_H
# define H266_NAL_H

# include <vlc_es.h>

#ifdef __cplusplus
extern "C" {
#endif

enum h266_general_profile_idc_e
{
    H266_PROFILE_IDC_MAIN_10                     = 1,
    H266_PROFILE_IDC_MULTILAYER_MAIN_10          = 17,
    H266_PROFILE_IDC_MAIN_10_444                 = 33,
    H266_PROFILE_IDC_MULTILAYER_MAIN_10_444      = 49,
    H266_PROFILE_IDC_MAIN_STILL_PICTURE          = 64,
    H266_PROFILE_IDC_MAIN_STILL_PICTURE_444      = 97,
};

#define H266_VPS_ID_MAX 15
#define H266_SPS_ID_MAX 15
#define H266_PPS_ID_MAX 63
#define H266_MAX_NUM_VPS H266_VPS_ID_MAX /* optional vps meant with id 0 */
#define H266_MAX_NUM_SPS (H266_SPS_ID_MAX+1)
#define H266_MAX_NUM_PPS (H266_PPS_ID_MAX+1)

#define H266_MIN_CONSTRAINT_BITS 71
#define H266_MAX_CONSTRAINT_ADDITIONAL_BITS 256
#define H266_CONSTRAINT_BYTES(bits) (((bits)+7)/8)
#define H266_MAX_CONSTRAINT_BYTES H266_CONSTRAINT_BYTES(H266_MIN_CONSTRAINT_BITS + \
                                                        H266_MAX_CONSTRAINT_ADDITIONAL_BITS)
#define H266_MAX_NUH_LAYER_ID 55 /* values 56-63 are reserved */
#define H266_MAX_NUM_PTL_SUBLAYERS 7
#define H266_MAX_NUM_SUBLAYERS (1+H266_MAX_NUM_PTL_SUBLAYERS)
#define H266_MAX_NUM_SUB_PROFILES 256

enum h266_nal_unit_type_e
{
    H266_NAL_TRAIL      = 0,
    H266_NAL_STSA       = 1,
    H266_NAL_RADL       = 2,
    H266_NAL_RASL       = 3,
    H266_NAL_RSV_VCL_4  = 4,
    H266_NAL_RSV_VCL_5  = 5,
    H266_NAL_RSV_VCL_6  = 6,
    H266_NAL_IDR_W_RADL = 7,
    H266_NAL_IDR_N_LP   = 8,
    H266_NAL_CRA        = 9,
    H266_NAL_GDR        = 10,
    H266_NAL_RSV_IRAP_11 = 11,
    H266_NAL_OPI         = 12,
    H266_NAL_DCI         = 13,
    H266_NAL_VPS         = 14,
    H266_NAL_SPS         = 15,
    H266_NAL_PPS         = 16,
    H266_NAL_PREFIX_APS  = 17,
    H266_NAL_SUFFIX_APS  = 18,
    H266_NAL_PH          = 19,
    H266_NAL_AUD         = 20,
    H266_NAL_EOS         = 21,
    H266_NAL_EOB         = 22,
    H266_NAL_PREFIX_SEI  = 23,
    H266_NAL_SUFFIX_SEI  = 24,
    H266_NAL_FD          = 25,
    H266_NAL_RSV_NVCL_26 = 26,
    H266_NAL_RSV_NVCL_27 = 27,
    H266_NAL_UNSPEC_28   = 28,
    H266_NAL_UNSPEC_29   = 29,
    H266_NAL_UNSPEC_30   = 30,
    H266_NAL_UNSPEC_31   = 31,
};

static inline enum h266_nal_unit_type_e
    h266_getNALType(const uint8_t *p_buf)
{
    return (enum h266_nal_unit_type_e)(p_buf[1] >> 3);
}

static inline uint8_t h266_getNALLayer(const uint8_t *p_buf)
{
    return p_buf[0] & 0x3f;
}

typedef struct h266_video_parameter_set_t h266_video_parameter_set_t;
typedef struct h266_sequence_parameter_set_t h266_sequence_parameter_set_t;
typedef struct h266_picture_parameter_set_t h266_picture_parameter_set_t;
typedef struct h266_decoding_capability_information_t h266_decoding_capability_information_t;
typedef struct h266_picture_header_t h266_picture_header_t;

typedef void(*pf_get_matchedh266xps)(uint8_t i_pps_id, void *priv,
                                     h266_picture_parameter_set_t **,
                                     h266_sequence_parameter_set_t **,
                                     h266_video_parameter_set_t **);

h266_video_parameter_set_t *    h266_decode_vps(const uint8_t *, size_t, bool);
h266_sequence_parameter_set_t * h266_decode_sps(const uint8_t *, size_t, bool);
h266_picture_parameter_set_t *  h266_decode_pps(const uint8_t *, size_t, bool);
h266_decoding_capability_information_t *h266_decode_dci(const uint8_t *, size_t, bool);
h266_picture_header_t * h266_decode_picture_header(const uint8_t *, size_t, bool,
                                                   pf_get_matchedh266xps, void *);
h266_picture_header_t * h266_decode_slice_header(const uint8_t *, size_t, bool,
                                                 pf_get_matchedh266xps, void *);

void h266_rbsp_release_vps(h266_video_parameter_set_t *);
void h266_rbsp_release_sps(h266_sequence_parameter_set_t *);
void h266_rbsp_release_pps(h266_picture_parameter_set_t *);
void h266_rbsp_release_dci(h266_decoding_capability_information_t *);
void h266_rbsp_release_picture_header(h266_picture_header_t *);

uint8_t h266_get_sps_vps_id(const h266_sequence_parameter_set_t *);
uint8_t h266_get_pps_sps_id(const h266_picture_parameter_set_t *);
uint8_t h266_get_picture_header_pps_id(const h266_picture_header_t *);
uint32_t h266_get_picture_header_poc_lsb(const h266_picture_header_t *);
bool h266_get_xps_id(const uint8_t *p_buf, size_t i_buf, uint8_t *pi_id);

bool h266_get_profile_tier_level(const h266_decoding_capability_information_t *,
                                  const h266_sequence_parameter_set_t *,
                                  uint8_t *pi_profile, uint8_t *pi_level);

bool h266_get_picture_size(const h266_sequence_parameter_set_t *p_sps,
                           unsigned *p_ox, unsigned *p_oy,
                           unsigned *p_w, unsigned *p_h,
                           unsigned *p_vw, unsigned *p_vh);

bool h266_get_frame_rate(const h266_sequence_parameter_set_t *p_sps,
                         const h266_video_parameter_set_t *p_vps,
                         unsigned *pi_num, unsigned *pi_den);

bool h266_get_aspect_ratio(const h266_sequence_parameter_set_t *,
                           unsigned *pi_num, unsigned *pi_den);

bool h266_get_chroma_luma(const h266_sequence_parameter_set_t *p_sps,
                          uint8_t *pi_chroma_format,
                          uint8_t *pi_depth_luma, uint8_t *pi_depth_chroma);

bool h266_get_colorimetry(const h266_sequence_parameter_set_t *p_sps,
                          video_color_primaries_t *p_primaries,
                          video_transfer_func_t *p_transfer,
                          video_color_space_t *p_colorspace,
                          video_color_range_t *p_full_range);


enum h266_slice_type_e
{
    H266_SLICE_TYPE_UNK = -1,
    H266_SLICE_TYPE_B,
    H266_SLICE_TYPE_P,
    H266_SLICE_TYPE_I,
};

bool h266_get_slice_type(const h266_picture_header_t *, enum h266_slice_type_e *);

/*
 * POC computing
 */
typedef struct
{
    struct
    {
        int lsb;
        int msb;
    } prevTid0PicOrderCnt;

    bool HandleCraAsClvsStartFlag;
    bool first_picture; /* Must be set on start or on NAL_EOS */
} h266_poc_ctx_t;

static inline void h266_poc_ctx_init(h266_poc_ctx_t *p_ctx)
{
    p_ctx->prevTid0PicOrderCnt.lsb = 0;
    p_ctx->prevTid0PicOrderCnt.msb = 0;
    p_ctx->first_picture = true;
    p_ctx->HandleCraAsClvsStartFlag = false;
}

int h266_compute_picture_order_count(const h266_sequence_parameter_set_t *p_sps,
                                     const h266_picture_header_t *slice,
                                     h266_poc_ctx_t *ctx);

/* VVCDecoderConfigurationRecord */

static inline bool h266_isvvcC(const uint8_t *p_buf, size_t i_buf)
{
    return ( i_buf >= 1 && (p_buf[0] & 0xF8) == 0xF8 );
}

typedef struct
{
    uint8_t general_profile_idc;
    uint8_t general_tier_flag;
    uint8_t general_level_idc;
    uint8_t ptl_frame_only_constraint_flag;
    uint8_t ptl_multilayer_enabled_flag;
    uint8_t ptl_sublayer_level_present_flag[H266_MAX_NUM_PTL_SUBLAYERS];
    uint8_t ptl_sublayer_level_idc[H266_MAX_NUM_PTL_SUBLAYERS];
    uint8_t ptl_num_sub_profiles;
    uint32_t ptl_general_sub_profile_idc[H266_MAX_NUM_SUB_PROFILES];
    struct
    {
        uint8_t gci_present;
        uint8_t gci_num_additional_bits;
        uint8_t constraint_bytes[H266_MAX_CONSTRAINT_BYTES];
    } constraints_info;
} h266_profile_tier_level_t;

static inline uint8_t h266_gci_num_constraint_bytes(const h266_profile_tier_level_t *ptl)
{
    if(!ptl->constraints_info.gci_present)
        return 0;
    return H266_CONSTRAINT_BYTES(H266_MIN_CONSTRAINT_BITS +
                                 ptl->constraints_info.gci_num_additional_bits);
}

struct h266_dcr_values
{
    uint8_t nal_length_size;
    uint8_t ptl_present_flag;
    uint16_t ols_idx;
    uint8_t num_sublayers;
    uint8_t constant_frame_rate;
    uint8_t chroma_format_idc;
    uint8_t bit_depth_minus8;
    h266_profile_tier_level_t ptl;
    uint16_t max_picture_width;
    uint16_t max_picture_height;
    uint16_t avg_frame_rate;
};

#define H266_MAX_NUM_SEI 16
struct h266_dcr_params
{
    const uint8_t *p_vps[H266_MAX_NUM_VPS],
        *p_sps[H266_MAX_NUM_SPS],
        *p_pps[H266_MAX_NUM_PPS],
        *p_seipref[H266_MAX_NUM_SEI],
        *p_seisuff[H266_MAX_NUM_SEI],
        *p_dci, *p_opi;
    uint16_t rgi_vps[H266_MAX_NUM_VPS],
        rgi_sps[H266_MAX_NUM_SPS],
        rgi_pps[H266_MAX_NUM_PPS],
        rgi_seipref[H266_MAX_NUM_SEI],
        rgi_seisuff[H266_MAX_NUM_SEI],
        i_dci, i_opi;
    uint8_t i_vps_count, i_sps_count, i_pps_count;
    uint8_t i_seipref_count, i_seisuff_count;
    struct h266_dcr_values *p_values;
};

void h266_add_NALtoParams(const uint8_t *p_nal, size_t i_nal,
                          struct h266_dcr_params *p_params);

uint8_t * h266_create_DecoderConfigurationRecord(const struct h266_dcr_params *p_params,
                                                 bool b_completeness, size_t *pi_size);
bool h266_parse_DecoderConfigurationRecord(const uint8_t *p_buf, size_t i_buf,
                                           struct h266_dcr_params *);
uint8_t * h266_create_AnnexbExtradataFromParams(const struct h266_dcr_params *p_params, size_t *pi_size);

#ifdef __cplusplus
}
#endif

#endif /* H266_NAL_H */
