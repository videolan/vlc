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

typedef struct h264_slice_s h264_slice_t;

enum h264_slice_type_e h264_get_slice_type( const h264_slice_t *p_slice );

enum h264_slice_struct_e
{
    H264_SLICE_FRAME,
    H264_SLICE_FIELD,
    H264_SLICE_MBAFF,
};

enum h264_slice_struct_e h264_get_slice_struct( const h264_slice_t *p_slice );
bool h264_is_field_pic( const h264_slice_t *p_slice );

int h264_get_slice_pps_id( const h264_slice_t *p_slice );
unsigned h264_get_frame_num( const h264_slice_t *p_slice );
unsigned h264_get_nal_ref_idc( const h264_slice_t *p_slice );
bool h264_has_mmco5( const h264_slice_t *p_slice );
void h264_slice_release( h264_slice_t *p_slice );
void h264_slice_copy_idr_id( const h264_slice_t *src, h264_slice_t *dst );


h264_slice_t * h264_decode_slice( const uint8_t *p_buffer, size_t i_buffer,
                        void (* get_sps_pps)(uint8_t pps_id, void *,
                                             const h264_sequence_parameter_set_t **,
                                             const h264_picture_parameter_set_t ** ),
                        void * );

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

bool h264_slice_top_field( const h264_slice_t *p_slice );

bool h264_IsFirstVCLNALUnit( const h264_slice_t *p_prev, const h264_slice_t *p_cur );

bool h264_CanSwapPTSWithDTS( const h264_slice_t *p_slice, const h264_sequence_parameter_set_t * );

#endif
