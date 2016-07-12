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
#ifndef H264_NAL_H
# define H264_NAL_H

# include <vlc_common.h>

#define PROFILE_H264_BASELINE             66
#define PROFILE_H264_MAIN                 77
#define PROFILE_H264_EXTENDED             88
#define PROFILE_H264_HIGH                 100
#define PROFILE_H264_HIGH_10              110
#define PROFILE_H264_HIGH_422             122
#define PROFILE_H264_HIGH_444             144
#define PROFILE_H264_HIGH_444_PREDICTIVE  244

#define PROFILE_H264_CAVLC_INTRA          44
#define PROFILE_H264_SVC_BASELINE         83
#define PROFILE_H264_SVC_HIGH             86
#define PROFILE_H264_MVC_STEREO_HIGH      128
#define PROFILE_H264_MVC_MULTIVIEW_HIGH   118

#define PROFILE_H264_MFC_HIGH                          134
#define PROFILE_H264_MVC_MULTIVIEW_DEPTH_HIGH          138
#define PROFILE_H264_MVC_ENHANCED_MULTIVIEW_DEPTH_HIGH 139

#define H264_SPS_MAX (32)
#define H264_PPS_MAX (256)

enum h264_nal_unit_type_e
{
    H264_NAL_UNKNOWN = 0,
    H264_NAL_SLICE   = 1,
    H264_NAL_SLICE_DPA   = 2,
    H264_NAL_SLICE_DPB   = 3,
    H264_NAL_SLICE_DPC   = 4,
    H264_NAL_SLICE_IDR   = 5,    /* ref_idc != 0 */
    H264_NAL_SEI         = 6,    /* ref_idc == 0 */
    H264_NAL_SPS         = 7,
    H264_NAL_PPS         = 8,
    H264_NAL_AU_DELIMITER= 9,
    /* ref_idc == 0 for 6,9,10,11,12 */
    H264_NAL_END_OF_SEQ  = 10,
    H264_NAL_END_OF_STREAM = 11,
    H264_NAL_FILLER_DATA = 12,
    H264_NAL_SPS_EXT     = 13,
    H264_NAL_PREFIX      = 14,
    H264_NAL_SUBSET_SPS  = 15,
    H264_NAL_DEPTH_PS    = 16,
    H264_NAL_RESERVED_17 = 17,
    H264_NAL_RESERVED_18 = 18,
    H264_NAL_SLICE_WP    = 19,
    H264_NAL_SLICE_EXT   = 20,
    H264_NAL_SLICE_3D_EXT= 21,
    H264_NAL_RESERVED_22 = 22,
    H264_NAL_RESERVED_23 = 23,
};

/* Defined in H.264 annex D */
enum h264_sei_type_e
{
    H264_SEI_PIC_TIMING = 1,
    H264_SEI_USER_DATA_REGISTERED_ITU_T_T35 = 4,
    H264_SEI_RECOVERY_POINT = 6
};

typedef struct h264_sequence_parameter_set_t h264_sequence_parameter_set_t;
typedef struct h264_picture_parameter_set_t h264_picture_parameter_set_t;

h264_sequence_parameter_set_t * h264_decode_sps( const uint8_t *, size_t, bool );
h264_picture_parameter_set_t *  h264_decode_pps( const uint8_t *, size_t, bool );

void h264_release_sps( h264_sequence_parameter_set_t * );
void h264_release_pps( h264_picture_parameter_set_t * );

struct h264_sequence_parameter_set_t
{
    int i_id;
    uint8_t i_profile, i_level;
    uint8_t i_constraint_set_flags;
    /* according to avcC, 3 bits max for those */
    uint8_t i_chroma_idc;
    uint8_t i_bit_depth_luma;
    uint8_t i_bit_depth_chroma;

    uint32_t pic_width_in_mbs_minus1;
    uint32_t pic_height_in_map_units_minus1;
    struct
    {
        uint32_t left_offset;
        uint32_t right_offset;
        uint32_t top_offset;
        uint32_t bottom_offset;
    } frame_crop;
    uint8_t frame_mbs_only_flag;
    int i_log2_max_frame_num;
    int i_pic_order_cnt_type;
    int i_delta_pic_order_always_zero_flag;
    int i_log2_max_pic_order_cnt_lsb;

    struct {
        bool b_valid;
        int i_sar_num, i_sar_den;
        struct {
            bool b_full_range;
            uint8_t i_colour_primaries;
            uint8_t i_transfer_characteristics;
            uint8_t i_matrix_coefficients;
        } colour;
        bool b_timing_info_present_flag;
        uint32_t i_num_units_in_tick;
        uint32_t i_time_scale;
        bool b_fixed_frame_rate;
        bool b_pic_struct_present_flag;
        bool b_hrd_parameters_present_flag;
        uint8_t i_cpb_removal_delay_length_minus1;
        uint8_t i_dpb_output_delay_length_minus1;
    } vui;
};

struct h264_picture_parameter_set_t
{
    int i_id;
    int i_sps_id;
    int i_pic_order_present_flag;
};

/*
    AnnexB : [\x00] \x00 \x00 \x01 Prefixed NAL
    AVC Sample format : NalLengthSize encoded size prefixed NAL
    avcC: AVCDecoderConfigurationRecord combining SPS & PPS in AVC Sample Format
*/

#define H264_MIN_AVCC_SIZE 7

bool h264_isavcC( const uint8_t *, size_t );

/* Convert AVC Sample format to Annex B in-place */
void h264_AVC_to_AnnexB( uint8_t *p_buf, uint32_t i_len,
                         uint8_t i_nal_length_size );

/* Get the SPS/PPS pointers from an Annex B buffer
 * Returns 0 if a SPS and/or a PPS is found */
int h264_get_spspps( uint8_t *p_buf, size_t i_buf,
                     uint8_t **pp_sps, size_t *p_sps_size,
                     uint8_t **pp_pps, size_t *p_pps_size,
                     uint8_t **pp_ext, size_t *p_ext_size );

/* Create a AVCDecoderConfigurationRecord from SPS/PPS
 * Returns a valid block_t on success, must be freed with block_Release */
block_t *h264_AnnexB_NAL_to_avcC( uint8_t i_nal_length_size,
                                  const uint8_t *p_sps_buf,
                                  size_t i_sps_size,
                                  const uint8_t *p_pps_buf,
                                  size_t i_pps_size );

/* Convert AVCDecoderConfigurationRecord SPS/PPS to Annex B format */
uint8_t * h264_avcC_to_AnnexB_NAL( const uint8_t *p_buf, size_t i_buf,
                                   size_t *pi_result, uint8_t *pi_nal_length_size );

bool h264_get_picture_size( const h264_sequence_parameter_set_t *, unsigned *p_w, unsigned *p_h,
                            unsigned *p_vw, unsigned *p_vh );
bool h264_get_chroma_luma( const h264_sequence_parameter_set_t *, uint8_t *pi_chroma_format,
                           uint8_t *pi_depth_luma, uint8_t *pi_depth_chroma );

/* Get level and Profile from DecoderConfigurationRecord */
bool h264_get_profile_level(const es_format_t *p_fmt, uint8_t *pi_profile,
                            uint8_t *pi_level, uint8_t *p_nal_length_size);

#endif /* H264_NAL_H */
