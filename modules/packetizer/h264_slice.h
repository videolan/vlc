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
#ifndef VLC_H264_SLICE_H
#define VLC_H264_SLICE_H

enum h264_slice_type_e
{
    H264_SLICE_TYPE_P = 0,
    H264_SLICE_TYPE_B,
    H264_SLICE_TYPE_I,
    H264_SLICE_TYPE_SP,
    H264_SLICE_TYPE_SI,
    H264_SLICE_TYPE_UNKNOWN,
};

typedef struct
{
    int i_nal_type;
    int i_nal_ref_idc;

    enum h264_slice_type_e type;
    int i_pic_parameter_set_id;
    int i_frame_num;

    int i_field_pic_flag;
    int i_bottom_field_flag;

    int i_idr_pic_id;

    int i_pic_order_cnt_type;
    int i_pic_order_cnt_lsb;
    int i_delta_pic_order_cnt_bottom;

    int i_delta_pic_order_cnt0;
    int i_delta_pic_order_cnt1;

    bool has_mmco5;
} h264_slice_t;

static inline void h264_slice_init( h264_slice_t *p_slice )
{
    p_slice->i_nal_type = -1;
    p_slice->i_nal_ref_idc = -1;
    p_slice->i_idr_pic_id = -1;
    p_slice->i_frame_num = -1;
    p_slice->type = H264_SLICE_TYPE_UNKNOWN;
    p_slice->i_pic_parameter_set_id = -1;
    p_slice->i_field_pic_flag = 0;
    p_slice->i_bottom_field_flag = -1;
    p_slice->i_pic_order_cnt_type = -1;
    p_slice->i_pic_order_cnt_lsb = -1;
    p_slice->i_delta_pic_order_cnt_bottom = -1;
    p_slice->i_delta_pic_order_cnt0 = 0;
    p_slice->i_delta_pic_order_cnt1 = 0;
    p_slice->has_mmco5 = false;
}

bool h264_decode_slice( const uint8_t *p_buffer, size_t i_buffer,
                        void (* get_sps_pps)(uint8_t pps_id, void *,
                                             const h264_sequence_parameter_set_t **,
                                             const h264_picture_parameter_set_t ** ),
                        void *, h264_slice_t *p_slice );

typedef struct
{
    struct
    {
        int lsb;
        int msb;
    } prevPicOrderCnt;
    unsigned prevFrameNum;
    unsigned prevFrameNumOffset;
    int  prevRefPictureTFOC;
    bool prevRefPictureIsBottomField;
    bool prevRefPictureHasMMCO5;
} h264_poc_context_t;

static inline void h264_poc_context_init( h264_poc_context_t *p_ctx )
{
    p_ctx->prevPicOrderCnt.lsb = 0;
    p_ctx->prevPicOrderCnt.msb = 0;
    p_ctx->prevFrameNum = 0;
    p_ctx->prevFrameNumOffset = 0;
    p_ctx->prevRefPictureIsBottomField = false;
    p_ctx->prevRefPictureHasMMCO5 = false;
}

void h264_compute_poc( const h264_sequence_parameter_set_t *p_sps,
                       const h264_slice_t *p_slice, h264_poc_context_t *p_ctx,
                       int *p_PictureOrderCount, int *p_tFOC, int *p_bFOC );

uint8_t h264_get_num_ts( const h264_sequence_parameter_set_t *p_sps,
                         const h264_slice_t *p_slice, uint8_t pic_struct, int tFOC, int bFOC );

#endif
