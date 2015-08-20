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

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <vlc_common.h>
# include <vlc_codec.h>

# include "../demux/mpeg/mpeg_parser_helpers.h"

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

#define SPS_MAX (32)
#define PPS_MAX (256)

enum nal_unit_type_e
{
    NAL_UNKNOWN = 0,
    NAL_SLICE   = 1,
    NAL_SLICE_DPA   = 2,
    NAL_SLICE_DPB   = 3,
    NAL_SLICE_DPC   = 4,
    NAL_SLICE_IDR   = 5,    /* ref_idc != 0 */
    NAL_SEI         = 6,    /* ref_idc == 0 */
    NAL_SPS         = 7,
    NAL_PPS         = 8,
    NAL_AU_DELIMITER= 9
    /* ref_idc == 0 for 6,9,10,11,12 */
};

/* Defined in H.264 annex D */
enum sei_type_e
{
    SEI_PIC_TIMING = 1,
    SEI_USER_DATA_REGISTERED_ITU_T_T35 = 4,
    SEI_RECOVERY_POINT = 6
};

struct nal_sps
{
    int i_id;
    int i_profile, i_profile_compatibility, i_level;
    int i_width, i_height;
    int i_log2_max_frame_num;
    int b_frame_mbs_only;
    int i_pic_order_cnt_type;
    int i_delta_pic_order_always_zero_flag;
    int i_log2_max_pic_order_cnt_lsb;

    struct {
        bool b_valid;
        int i_sar_num, i_sar_den;
        bool b_timing_info_present_flag;
        uint32_t i_num_units_in_tick;
        uint32_t i_time_scale;
        bool b_fixed_frame_rate;
        bool b_pic_struct_present_flag;
        bool b_cpb_dpb_delays_present_flag;
        uint8_t i_cpb_removal_delay_length_minus1;
        uint8_t i_dpb_output_delay_length_minus1;
    } vui;
};

struct nal_pps
{
    int i_id;
    int i_sps_id;
    int i_pic_order_present_flag;
};

static inline void CreateRbspFromNAL( uint8_t **pp_ret, int *pi_ret,
                                     const uint8_t *src, int i_src )
{
    uint8_t *dst = malloc( i_src );

    *pp_ret = dst;

    if( dst )
        *pi_ret = nal_to_rbsp(src, dst, i_src);
}

/* Parse the SPS/PPS Metadata and convert it to annex b format */
int convert_sps_pps( decoder_t *p_dec, const uint8_t *p_buf,
                     uint32_t i_buf_size, uint8_t *p_out_buf,
                     uint32_t i_out_buf_size, uint32_t *p_sps_pps_size,
                     uint32_t *p_nal_length_size);

/* Convert avcC format to Annex B in-place */
void convert_h264_to_annexb( uint8_t *p_buf, uint32_t i_len,
                             size_t i_nal_length_size );

/* Convert Annex B to avcC format in-place
 * Returns the same p_block or a new p_block if there is not enough room to put
 * the NAL size. In case of error, NULL is returned and p_block is released.
 * */
block_t *convert_annexb_to_h264( block_t *p_block, size_t i_nal_length_size );

/* Get the SPS/PPS pointers from an Annex B buffer
 * Returns 0 if a SPS and/or a PPS is found */
int h264_get_spspps( uint8_t *p_buf, size_t i_buf,
                     uint8_t **pp_sps, size_t *p_sps_size,
                     uint8_t **pp_pps, size_t *p_pps_size );

/* Parse a SPS into the struct nal_sps
 * Returns 0 in case of success */
int h264_parse_sps( const uint8_t *p_sps_buf, int i_sps_size,
                    struct nal_sps *p_sps );

/* Parse a PPS into the struct nal_pps
 * Returns 0 in case of success */
int h264_parse_pps( const uint8_t *p_pps_buf, int i_pps_size,
                    struct nal_pps *p_pps );

/* Create a AVCDecoderConfigurationRecord from SPS/PPS
 * Returns a valid block_t on success, must be freed with block_Release */
block_t *h264_create_avcdec_config_record( size_t i_nal_length_size,
                                           struct nal_sps *p_sps,
                                           const uint8_t *p_sps_buf,
                                           size_t i_sps_size,
                                           const uint8_t *p_pps_buf,
                                           size_t i_pps_size );

/* Get level and Profile */
bool h264_get_profile_level(const es_format_t *p_fmt, size_t *p_profile,
                            size_t *p_level, size_t *p_nal_length_size);

#endif /* H264_NAL_H */
