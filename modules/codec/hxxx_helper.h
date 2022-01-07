/*****************************************************************************
 * hxxx_helper.h: AnnexB / avcC helper for dumb decoders
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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

#include <vlc_block.h>
#include <vlc_fourcc.h>

#include "../packetizer/h264_nal.h"
#include "../packetizer/hevc_nal.h"

struct hxxx_helper_nal
{
    block_t *b;
    union {
        void                            *xps;
        h264_sequence_parameter_set_t   *h264_sps;
        h264_picture_parameter_set_t    *h264_pps;
        h264_sequence_parameter_set_extension_t *h264_spsext;
        hevc_sequence_parameter_set_t   *hevc_sps;
        hevc_picture_parameter_set_t    *hevc_pps;
        hevc_video_parameter_set_t      *hevc_vps;
    };
};

#define HXXX_HELPER_SEI_COUNT 16

struct hxxx_helper
{
    vlc_object_t *p_obj; /* for logs */
    vlc_fourcc_t i_codec;
    uint8_t i_input_nal_length_size;
    uint8_t i_output_nal_length_size;
    uint8_t i_config_version_prev;
    uint8_t i_config_version;
    union {
        struct {
            struct hxxx_helper_nal sps_list[H264_SPS_ID_MAX + 1];
            struct hxxx_helper_nal pps_list[H264_PPS_ID_MAX + 1];
            struct hxxx_helper_nal spsext_list[H264_SPS_ID_MAX + 1];
            uint8_t i_current_sps;
            uint8_t i_sps_count;
            uint8_t i_pps_count;
            uint8_t i_spsext_count;
        } h264;
        struct {
            struct hxxx_helper_nal sps_list[HEVC_SPS_ID_MAX + 1];
            struct hxxx_helper_nal pps_list[HEVC_PPS_ID_MAX + 1];
            struct hxxx_helper_nal vps_list[HEVC_VPS_ID_MAX + 1];
            struct hxxx_helper_nal sei_list[HXXX_HELPER_SEI_COUNT];
            uint8_t i_current_sps;
            uint8_t i_current_vps;
            uint8_t i_sps_count;
            uint8_t i_pps_count;
            uint8_t i_vps_count;
            uint8_t i_sei_count;
            uint8_t i_previous_nal_type;
        } hevc;
    };
};

/* Init:
 * i_input_length_size set xvcC nal length or 0 for AnnexB (overridden by extradata)
 * i_output_length_size set xvcC nal length or 0 for AnnexB */
void hxxx_helper_init(struct hxxx_helper *hh, vlc_object_t *p_obj,
                      vlc_fourcc_t i_codec, uint8_t i_input_length_size,
                      uint8_t i_output_length_size);

void hxxx_helper_clean(struct hxxx_helper *hh);

/* Process raw NAL buffer:
 * No framing prefix */
int hxxx_helper_process_nal(struct hxxx_helper *hh, const uint8_t *, size_t);

/* Process the buffer:*/
int hxxx_helper_process_buffer(struct hxxx_helper *hh, const uint8_t *, size_t);

/* Process the block:
 * Does AnnexB <-> xvcC conversion if needed. */
block_t * hxxx_helper_process_block(struct hxxx_helper *hh, block_t *p_block);

/* Set Extra:
 * Also changes/set the input to match xvcC nal_size of Annexb format */
int hxxx_helper_set_extra(struct hxxx_helper *hh, const void *p_extra,
                          size_t i_extra);

/* returns if a configuration parameter has changed since last block/buffer
 * processing */
bool hxxx_helper_has_new_config(const struct hxxx_helper *hh);
bool hxxx_helper_has_config(const struct hxxx_helper *hh);

block_t * hxxx_helper_get_extradata_chain(const struct hxxx_helper *hh);
block_t * hxxx_helper_get_extradata_block(const struct hxxx_helper *hh);

int hxxx_helper_get_current_picture_size(const struct hxxx_helper *hh,
                                         unsigned *p_w, unsigned *p_h,
                                         unsigned *p_vw, unsigned *p_vh);

int hxxx_helper_get_current_sar(const struct hxxx_helper *hh, int *p_num, int *p_den);

int h264_helper_get_current_dpb_values(const struct hxxx_helper *hh,
                                       uint8_t *p_depth, unsigned *pi_delay);

int hxxx_helper_get_current_profile_level(const struct hxxx_helper *hh,
                                          uint8_t *p_profile, uint8_t *p_level);

int
hxxx_helper_get_chroma_chroma(const struct hxxx_helper *hh, uint8_t *pi_chroma_format,
                              uint8_t *pi_depth_luma, uint8_t *pi_depth_chroma);

int hxxx_helper_get_colorimetry(const struct hxxx_helper *hh,
                                video_color_primaries_t *p_primaries,
                                video_transfer_func_t *p_transfer,
                                video_color_space_t *p_colorspace,
                                video_color_range_t *p_full_range);
