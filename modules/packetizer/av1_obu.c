/*****************************************************************************
 * av1_obu.c: AV1 OBU parser
 *****************************************************************************
 * Copyright (C) 2018 VideoLabs, VLC authors and VideoLAN
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
#include <vlc_es.h>

#include "av1.h"
#include "av1_obu.h"
#include "iso_color_tables.h"

#include <assert.h>

typedef uint8_t  obu_u1_t;
typedef uint8_t  obu_u2_t;
typedef uint8_t  obu_u3_t;
typedef uint8_t  obu_u4_t;
typedef uint8_t  obu_u5_t;
typedef uint8_t  obu_u6_t;
typedef uint8_t  obu_u7_t;
typedef uint8_t  obu_u8_t;
typedef uint16_t obu_u12_t;
typedef uint32_t obu_u32_t;
typedef uint32_t obu_uvlc_t;

#define SELECT_SCREEN_CONTENT_TOOLS 2
#define SELECT_INTEGER_MV 2


#define AV1_OPERATING_POINTS_COUNT 32

/*
 * Header
 */
struct av1_header_info_s
{
    obu_u4_t obu_type;
    obu_u3_t temporal_id;
    obu_u2_t spatial_id;
};

static bool av1_read_header(bs_t *p_bs, struct av1_header_info_s *p_hdr)
{
    if(bs_read1(p_bs))
        return false;
    p_hdr->obu_type = bs_read(p_bs, 4);
    const obu_u1_t obu_extension_flag = bs_read1(p_bs);
    const obu_u1_t obu_has_size_field = bs_read1(p_bs);
    if(bs_read1(p_bs))
        return false;
    if(obu_extension_flag)
    {
        p_hdr->temporal_id = bs_read(p_bs, 3);
        p_hdr->spatial_id = bs_read(p_bs, 2);
        bs_skip(p_bs, 3);
    }
    if(obu_has_size_field)
    {
        for (uint8_t i = 0; i < 8; i++)
        {
            uint8_t v = bs_read(p_bs, 8);
            if (!(v & 0x80))
                break;
            if(i == 7)
                return false;
        }
    }
    return !bs_error(p_bs);
}

/*
 * Sequence sub sections readers
 */

struct av1_timing_info_s
{
    obu_u32_t num_units_in_display_tick;
    obu_u32_t time_scale;
    obu_u1_t equal_picture_interval;
    obu_uvlc_t num_ticks_per_picture_minus_1;
};

static bool av1_parse_timing_info(bs_t *p_bs, struct av1_timing_info_s *p_ti)
{
    p_ti->num_units_in_display_tick = bs_read(p_bs, 32);
    p_ti->time_scale = bs_read(p_bs, 32);
    p_ti->equal_picture_interval = bs_read1(p_bs);
    if(p_ti->equal_picture_interval)
        p_ti->num_ticks_per_picture_minus_1 = bs_read_ue(p_bs);
    return true;
}

struct av1_decoder_model_info_s
{
    obu_u5_t buffer_delay_length_minus_1;
    obu_u32_t num_units_in_decoding_tick;
    obu_u5_t buffer_removal_time_length_minus_1;
    obu_u5_t frame_presentation_time_length_minus_1;
};

static bool av1_parse_decoder_model_info(bs_t *p_bs, struct av1_decoder_model_info_s *p_dm)
{
    p_dm->buffer_delay_length_minus_1 = bs_read(p_bs, 5);
    p_dm->num_units_in_decoding_tick = bs_read(p_bs, 32);
    p_dm->buffer_removal_time_length_minus_1 = bs_read(p_bs, 5);
    p_dm->frame_presentation_time_length_minus_1 = bs_read(p_bs, 5);
    return true;
}

struct av1_operating_parameters_info_s
{
    obu_u32_t decoder_buffer_delay;
    obu_u32_t encoder_buffer_delay;
    obu_u1_t low_delay_mode_flag;
};

static bool av1_parse_operating_parameters_info(bs_t *p_bs,
                                                struct av1_operating_parameters_info_s *p_op,
                                                obu_u8_t buffer_delay_length_minus_1)
{
    p_op->decoder_buffer_delay = bs_read(p_bs, 1 + buffer_delay_length_minus_1);
    p_op->encoder_buffer_delay = bs_read(p_bs, 1 + buffer_delay_length_minus_1);
    p_op->low_delay_mode_flag = bs_read1(p_bs);
    return true;
}

struct av1_color_config_s
{
    obu_u1_t high_bitdepth;
    obu_u1_t twelve_bit;
    obu_u1_t mono_chrome;
    obu_u1_t color_description_present_flag;
    obu_u8_t color_primaries;
    obu_u8_t transfer_characteristics;
    obu_u8_t matrix_coefficients;
    obu_u1_t color_range;
    obu_u1_t subsampling_x;
    obu_u1_t subsampling_y;
    obu_u2_t chroma_sample_position;
    obu_u1_t separate_uv_delta_q;
};

static bool av1_parse_color_config(bs_t *p_bs,
                                   struct av1_color_config_s *p_cc,
                                   obu_u3_t seq_profile)
{
    p_cc->high_bitdepth = bs_read1(p_bs);
    if(seq_profile <= 2)
    {
        if(p_cc->high_bitdepth)
            p_cc->twelve_bit = bs_read1(p_bs);
        if(seq_profile != 1)
            p_cc->mono_chrome = bs_read1(p_bs);
    }
    const uint8_t BitDepth = p_cc->twelve_bit ? 12 : ((p_cc->high_bitdepth) ? 10 : 8);

    p_cc->color_description_present_flag = bs_read1(p_bs);
    if(p_cc->color_description_present_flag)
    {
        p_cc->color_primaries = bs_read(p_bs, 8);
        p_cc->transfer_characteristics = bs_read(p_bs, 8);
        p_cc->matrix_coefficients = bs_read(p_bs, 8);
    }
    else
    {
        p_cc->color_primaries = 2;
        p_cc->transfer_characteristics = 2;
        p_cc->matrix_coefficients = 2;
    }

    if(p_cc->mono_chrome)
    {
        p_cc->color_range = bs_read1(p_bs) ? COLOR_RANGE_FULL : COLOR_RANGE_LIMITED;
    }
    else if( p_cc->color_primaries == 1 &&
             p_cc->transfer_characteristics == 13 &&
             p_cc->matrix_coefficients == 0 )
    {
        p_cc->color_range = COLOR_RANGE_FULL;
    }
    else
    {
        p_cc->color_range = bs_read1(p_bs) ? COLOR_RANGE_FULL : COLOR_RANGE_LIMITED;
        if(seq_profile > 1)
        {
            if(BitDepth == 12)
            {
                p_cc->subsampling_x = bs_read1(p_bs);
                if(p_cc->subsampling_x)
                    p_cc->subsampling_y = bs_read1(p_bs);
            }
            else
            {
                p_cc->subsampling_x = 1;
            }
        }
        if(p_cc->subsampling_x && p_cc->subsampling_y)
            p_cc->chroma_sample_position = bs_read(p_bs, 2);
    }

    p_cc->separate_uv_delta_q = bs_read1(p_bs);

    return true;
}

/*
 * OBU readers
 */

struct av1_OBU_sequence_header_t
{
    struct av1_header_info_s obu_header;
    obu_u3_t seq_profile;
    obu_u1_t still_picture;
    obu_u1_t reduced_still_picture_header;
    obu_u1_t timing_info_present_flag;
    struct av1_timing_info_s timing_info;
    obu_u1_t decoder_model_info_present_flag;
    struct av1_decoder_model_info_s decoder_model_info;
    obu_u1_t initial_display_delay_present_flag;
    obu_u1_t operating_points_cnt_minus_1;
    struct
    {
        obu_u12_t operating_point_idc;
        obu_u5_t seq_level_idx;
        obu_u1_t seq_tier;
        obu_u1_t decoder_model_present_for_this_op;
        struct av1_operating_parameters_info_s operating_parameters_info;
        obu_u1_t initial_display_delay_present_for_this_op;
        obu_u4_t initial_display_delay_minus_1;
    } operating_points[AV1_OPERATING_POINTS_COUNT];
    obu_u32_t max_frame_width_minus_1;
    obu_u32_t max_frame_height_minus_1;
    obu_u1_t frame_id_numbers_present_flag;
    obu_u4_t delta_frame_id_length_minus_2;
    obu_u3_t additional_frame_id_length_minus_1;
    obu_u1_t use_128x128_superblock;
    obu_u1_t enable_filter_intra;
    obu_u1_t enable_intra_edge_filter;

    obu_u1_t enable_interintra_compound;
    obu_u1_t enable_masked_compound;
    obu_u1_t enable_warped_motion;
    obu_u1_t enable_dual_filter;
    obu_u1_t enable_order_hint;
    obu_u1_t enable_jnt_comp;
    obu_u1_t enable_ref_frame_mvs;
    obu_u2_t seq_force_screen_content_tools;
    obu_u2_t seq_force_integer_mv;
    obu_u3_t order_hint_bits_minus_1;

    obu_u1_t enable_superres;
    obu_u1_t enable_cdef;
    obu_u1_t enable_restoration;
    struct av1_color_config_s color_config;
    obu_u1_t film_grain_params_present;
};

void AV1_release_sequence_header(av1_OBU_sequence_header_t *p_seq)
{
    free(p_seq);
}

av1_OBU_sequence_header_t *
    AV1_OBU_parse_sequence_header(const uint8_t *p_data, size_t i_data)
{
    bs_t bs;
    bs_init(&bs, p_data, i_data);

    av1_OBU_sequence_header_t *p_seq = calloc(1, sizeof(*p_seq));
    if(!p_seq)
        return NULL;

    if(!av1_read_header(&bs, &p_seq->obu_header))
    {
        AV1_release_sequence_header(p_seq);
        return NULL;
    }

    p_seq->seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
    p_seq->seq_force_integer_mv = SELECT_INTEGER_MV;


    p_seq->seq_profile = bs_read(&bs, 3);
    p_seq->still_picture = bs_read1(&bs);
    const obu_u1_t reduced_still_picture_header = bs_read1(&bs);
    if(reduced_still_picture_header)
    {
        p_seq->operating_points[0].seq_level_idx = bs_read(&bs, 5);
    }
    else
    {
        p_seq->timing_info_present_flag = bs_read1(&bs);
        if(p_seq->timing_info_present_flag)
        {
            av1_parse_timing_info(&bs, &p_seq->timing_info);
            p_seq->decoder_model_info_present_flag = bs_read1(&bs);
            if(p_seq->decoder_model_info_present_flag)
                av1_parse_decoder_model_info(&bs, &p_seq->decoder_model_info);
        }

        p_seq->initial_display_delay_present_flag = bs_read1(&bs);
        p_seq->operating_points_cnt_minus_1 = bs_read(&bs, 5);
        for(obu_u5_t i=0; i<=p_seq->operating_points_cnt_minus_1; i++)
        {
            p_seq->operating_points[i].operating_point_idc = bs_read(&bs, 12);
            p_seq->operating_points[i].seq_level_idx = bs_read(&bs, 5);
            if(p_seq->operating_points[i].seq_level_idx > 7)
                p_seq->operating_points[i].seq_tier = bs_read1(&bs);
            if(p_seq->decoder_model_info_present_flag)
            {
                p_seq->operating_points[i].decoder_model_present_for_this_op = bs_read1(&bs);
                if(p_seq->operating_points[i].decoder_model_present_for_this_op)
                    av1_parse_operating_parameters_info(&bs, &p_seq->operating_points[i].operating_parameters_info,
                                                  p_seq->decoder_model_info.buffer_delay_length_minus_1);
            }
            if(p_seq->initial_display_delay_present_flag)
            {
                p_seq->operating_points[i].initial_display_delay_present_for_this_op = bs_read1(&bs);
                if(p_seq->operating_points[i].initial_display_delay_present_for_this_op)
                {
                    p_seq->operating_points[i].initial_display_delay_minus_1 = bs_read(&bs, 4);
                }
            }
        }
    }
    const obu_u4_t frame_width_bits_minus_1 = bs_read(&bs, 4);
    const obu_u4_t frame_height_bits_minus_1 = bs_read(&bs, 4);
    p_seq->max_frame_width_minus_1 = bs_read(&bs, 1 + frame_width_bits_minus_1);
    p_seq->max_frame_height_minus_1 = bs_read(&bs, 1 + frame_height_bits_minus_1);
    if(!reduced_still_picture_header)
    {
        p_seq->frame_id_numbers_present_flag = bs_read1(&bs);
        if(p_seq->frame_id_numbers_present_flag)
        {
            p_seq->delta_frame_id_length_minus_2 = bs_read(&bs, 4);
            p_seq->additional_frame_id_length_minus_1 = bs_read(&bs, 3);
        }
    }
    p_seq->use_128x128_superblock = bs_read1(&bs);
    p_seq->enable_filter_intra = bs_read1(&bs);
    p_seq->enable_intra_edge_filter = bs_read1(&bs);
    if(!reduced_still_picture_header)
    {
        p_seq->enable_interintra_compound = bs_read1(&bs);
        p_seq->enable_masked_compound = bs_read1(&bs);
        p_seq->enable_warped_motion = bs_read1(&bs);
        p_seq->enable_dual_filter = bs_read1(&bs);
        p_seq->enable_order_hint = bs_read1(&bs);
        if(p_seq->enable_order_hint)
        {
            p_seq->enable_jnt_comp = bs_read1(&bs);
            p_seq->enable_ref_frame_mvs = bs_read1(&bs);
        }
        const obu_u1_t seq_choose_screen_content_tools = bs_read1(&bs);
        if(!seq_choose_screen_content_tools)
            p_seq->seq_force_screen_content_tools = bs_read1(&bs);

        if(p_seq->seq_force_screen_content_tools)
        {
            const obu_u1_t seq_choose_integer_mv = bs_read1(&bs);
            if(!seq_choose_integer_mv)
                p_seq->seq_force_integer_mv = bs_read1(&bs);
        }

        if(p_seq->enable_order_hint)
            p_seq->order_hint_bits_minus_1 = bs_read(&bs, 3);
    }
    p_seq->enable_superres = bs_read1(&bs);
    p_seq->enable_cdef = bs_read1(&bs);
    p_seq->enable_restoration = bs_read1(&bs);
    av1_parse_color_config(&bs, &p_seq->color_config, p_seq->seq_profile);

    if(bs_error(&bs))
    {
        AV1_release_sequence_header(p_seq);
        return NULL;
    }

    p_seq->film_grain_params_present = bs_read1(&bs);

    return p_seq;
}

/*
 * Frame sub readers
 */

struct av1_uncompressed_header_s
{
    obu_u1_t show_existing_frame;
    obu_u2_t frame_type;
    obu_u1_t show_frame;
    obu_u32_t frame_presentation_time;
};

static bool av1_parse_uncompressed_header(bs_t *p_bs, struct av1_uncompressed_header_s *p_uh,
                                          const av1_OBU_sequence_header_t *p_seq)
{
    if(p_seq->reduced_still_picture_header)
    {
        p_uh->frame_type = AV1_FRAME_TYPE_KEY;
        p_uh->show_frame = 1;
    }
    else
    {
        p_uh->show_existing_frame = bs_read1(p_bs);
        if(p_uh->show_existing_frame)
        {
            const obu_u3_t frame_to_show_map_idx = bs_read(p_bs, 3);
            VLC_UNUSED(frame_to_show_map_idx);
            if(p_seq->decoder_model_info_present_flag && !p_seq->timing_info.equal_picture_interval)
            {
                /* temporal_point_info() */
                p_uh->frame_presentation_time =
                        bs_read(p_bs, 1 + p_seq->decoder_model_info.frame_presentation_time_length_minus_1);
            }
            if(p_seq->frame_id_numbers_present_flag)
            {
                const uint8_t idLen = p_seq->additional_frame_id_length_minus_1 +
                                      p_seq->delta_frame_id_length_minus_2 + 3;
                const obu_u32_t display_frame_id = bs_read(p_bs, idLen);
                VLC_UNUSED(display_frame_id);
            }
            if(p_seq->film_grain_params_present)
            {
                /* load_grain */
            }
        }
        p_uh->frame_type = bs_read(p_bs, 2);
        p_uh->show_frame = bs_read1(p_bs);
    }

    return true;
}

/*
 * Frame OBU
 */

struct av1_OBU_frame_header_t
{
    struct av1_header_info_s obu_header;
    struct av1_uncompressed_header_s header;
};

void AV1_release_frame_header(av1_OBU_frame_header_t *p_fh)
{
    free(p_fh);
}

av1_OBU_frame_header_t *
    AV1_OBU_parse_frame_header(const uint8_t *p_data, size_t i_data,
                               const av1_OBU_sequence_header_t *p_seq)
{
    bs_t bs;
    bs_init(&bs, p_data, i_data);

    av1_OBU_frame_header_t *p_fh = calloc(1, sizeof(*p_fh));
    if(!p_fh)
        return NULL;

    if(!av1_read_header(&bs, &p_fh->obu_header) ||
       !av1_parse_uncompressed_header(&bs, &p_fh->header, p_seq))
    {
        AV1_release_frame_header(p_fh);
        return NULL;
    }

    return p_fh;
}

enum av1_frame_type_e AV1_get_frame_type(const av1_OBU_frame_header_t *p_fh)
{
    return p_fh->header.frame_type;
}

bool AV1_get_frame_visibility(const av1_OBU_frame_header_t *p_fh)
{
    return p_fh->header.show_frame;
}

/*
 * Getters
 */
void AV1_get_frame_max_dimensions(const av1_OBU_sequence_header_t *p_seq, unsigned *w, unsigned *h)
{
    *h = 1 + p_seq->max_frame_height_minus_1;
    *w = 1 + p_seq->max_frame_width_minus_1;
}

void AV1_get_profile_level(const av1_OBU_sequence_header_t *p_seq,
                           int *pi_profile, int *pi_level, int *pi_tier)
{
    *pi_profile = p_seq->seq_profile;
    *pi_level = p_seq->operating_points[0].seq_level_idx;
    *pi_tier = p_seq->operating_points[0].seq_tier;
}

bool AV1_get_frame_rate(const av1_OBU_sequence_header_t *p_seq,
                        unsigned *num, unsigned *den)
{
    if(!p_seq->timing_info_present_flag ||
       !p_seq->timing_info.equal_picture_interval) /* need support for VFR */
        return false;
    *num = (1 + p_seq->timing_info.num_ticks_per_picture_minus_1) *
           p_seq->timing_info.num_units_in_display_tick;
    *den = p_seq->timing_info.time_scale;
    return true;
}

bool AV1_get_colorimetry(const av1_OBU_sequence_header_t *p_seq,
                         video_color_primaries_t *p_primaries,
                         video_transfer_func_t *p_transfer,
                         video_color_space_t *p_colorspace,
                         video_color_range_t *p_full_range)
{
    if(!p_seq->color_config.color_description_present_flag)
        return false;
    *p_primaries = iso_23001_8_cp_to_vlc_primaries(p_seq->color_config.color_primaries);
    *p_transfer = iso_23001_8_tc_to_vlc_xfer(p_seq->color_config.transfer_characteristics);
    *p_colorspace = iso_23001_8_mc_to_vlc_coeffs(p_seq->color_config.matrix_coefficients);
    *p_full_range = p_seq->color_config.color_range ? COLOR_RANGE_FULL : COLOR_RANGE_LIMITED;
    return true;
}

size_t AV1_create_DecoderConfigurationRecord(uint8_t **pp_buffer,
                                             const av1_OBU_sequence_header_t *p_seq,
                                             size_t i_obu, const uint8_t *p_obus[],
                                             const size_t pi_obus[])
{
    size_t i_buffer = 4;
    for(size_t i=0; i<i_obu; i++)
        i_buffer += pi_obus[i];

    uint8_t *p_buffer = malloc(i_buffer);
    if(!p_buffer)
        return 0;

    bs_t bs;
    bs_write_init(&bs, p_buffer, i_buffer);
    bs_write(&bs, 1, 1); /* unsigned int (1) marker = 1; */
    bs_write(&bs, 7, 1); /* unsigned int (7) version = 1; */
    bs_write(&bs, 3, p_seq->seq_profile); /* unsigned int (3) seq_profile; */
    bs_write(&bs, 5, p_seq->operating_points[0].seq_level_idx); /* unsigned int (5) seq_level_idx_0; */

    bs_write(&bs, 1, p_seq->operating_points[0].seq_tier); /* unsigned int (1) seq_tier_0; */
    bs_write(&bs, 1, p_seq->color_config.high_bitdepth); /* unsigned int (1) high_bitdepth; */
    bs_write(&bs, 1, p_seq->color_config.twelve_bit); /* unsigned int (1) twelve_bit; */
    bs_write(&bs, 1, p_seq->color_config.mono_chrome); /* unsigned int (1) monochrome; */
    bs_write(&bs, 1, p_seq->color_config.subsampling_x); /* unsigned int (1) chroma_subsampling_x; */
    bs_write(&bs, 1, p_seq->color_config.subsampling_y); /* unsigned int (1) chroma_subsampling_y; */
    bs_write(&bs, 2, p_seq->color_config.chroma_sample_position); /* unsigned int (2) chroma_sample_position; */

    bs_write(&bs, 3, 0); /* unsigned int (3) reserved = 0; */
    bs_write(&bs, 1, 0); /* unsigned int (1) initial_presentation_delay_present; (can't compute it) */
    bs_write(&bs, 4, 0); /* unsigned int (4) reserved = 0; */

    /*unsigned int (8)[] configOBUs;*/
    size_t i_offset = 4;
    for(size_t i=0; i<i_obu; i++)
        memcpy(&p_buffer[i_offset], p_obus[i], pi_obus[i]);

    *pp_buffer = p_buffer;
    return i_buffer;
}
