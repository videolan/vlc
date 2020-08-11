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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_bits.h>

#include "h264_nal.h"
#include "h264_slice.h"
#include "hxxx_nal.h"
#include "hxxx_ep3b.h"

bool h264_decode_slice( const uint8_t *p_buffer, size_t i_buffer,
                        void (* get_sps_pps)(uint8_t, void *,
                                             const h264_sequence_parameter_set_t **,
                                             const h264_picture_parameter_set_t ** ),
                        void *priv, h264_slice_t *p_slice )
{
    int i_slice_type;
    h264_slice_init( p_slice );
    bs_t s;
    struct hxxx_bsfw_ep3b_ctx_s bsctx;
    hxxx_bsfw_ep3b_ctx_init( &bsctx );
    bs_init_custom( &s, p_buffer, i_buffer, &hxxx_bsfw_ep3b_callbacks, &bsctx );

    /* nal unit header */
    bs_skip( &s, 1 );
    const uint8_t i_nal_ref_idc = bs_read( &s, 2 );
    const uint8_t i_nal_type = bs_read( &s, 5 );

    /* first_mb_in_slice */
    /* int i_first_mb = */ bs_read_ue( &s );

    /* slice_type */
    i_slice_type = bs_read_ue( &s );
    p_slice->type = i_slice_type % 5;

    /* */
    p_slice->i_nal_type = i_nal_type;
    p_slice->i_nal_ref_idc = i_nal_ref_idc;

    p_slice->i_pic_parameter_set_id = bs_read_ue( &s );
    if( p_slice->i_pic_parameter_set_id > H264_PPS_ID_MAX )
        return false;

    const h264_sequence_parameter_set_t *p_sps;
    const h264_picture_parameter_set_t *p_pps;

    /* Bind matched/referred PPS and SPS */
    get_sps_pps( p_slice->i_pic_parameter_set_id, priv, &p_sps, &p_pps );
    if( !p_sps || !p_pps )
        return false;

    p_slice->i_frame_num = bs_read( &s, p_sps->i_log2_max_frame_num + 4 );

    if( !p_sps->frame_mbs_only_flag )
    {
        /* field_pic_flag */
        p_slice->i_field_pic_flag = bs_read( &s, 1 );
        if( p_slice->i_field_pic_flag )
            p_slice->i_bottom_field_flag = bs_read( &s, 1 );
    }

    if( p_slice->i_nal_type == H264_NAL_SLICE_IDR )
        p_slice->i_idr_pic_id = bs_read_ue( &s );

    p_slice->i_pic_order_cnt_type = p_sps->i_pic_order_cnt_type;
    if( p_sps->i_pic_order_cnt_type == 0 )
    {
        p_slice->i_pic_order_cnt_lsb = bs_read( &s, p_sps->i_log2_max_pic_order_cnt_lsb + 4 );
        if( p_pps->i_pic_order_present_flag && !p_slice->i_field_pic_flag )
            p_slice->i_delta_pic_order_cnt_bottom = bs_read_se( &s );
    }
    else if( (p_sps->i_pic_order_cnt_type == 1) &&
             (!p_sps->i_delta_pic_order_always_zero_flag) )
    {
        p_slice->i_delta_pic_order_cnt0 = bs_read_se( &s );
        if( p_pps->i_pic_order_present_flag && !p_slice->i_field_pic_flag )
            p_slice->i_delta_pic_order_cnt1 = bs_read_se( &s );
    }

    if( p_pps->i_redundant_pic_present_flag )
        bs_read_ue( &s ); /* redudant_pic_count */

    unsigned num_ref_idx_l01_active_minus1[2] = {0 , 0};

    if( i_slice_type == 1 || i_slice_type == 6 ) /* B slices */
        bs_read1( &s ); /* direct_spatial_mv_pred_flag */
    if( i_slice_type == 0 || i_slice_type == 5 ||
        i_slice_type == 3 || i_slice_type == 8 ||
        i_slice_type == 1 || i_slice_type == 6 ) /* P SP B slices */
    {
        if( bs_read1( &s ) ) /* num_ref_idx_active_override_flag */
        {
            num_ref_idx_l01_active_minus1[0] = bs_read_ue( &s );
            if( i_slice_type == 1 || i_slice_type == 6 ) /* B slices */
                num_ref_idx_l01_active_minus1[1] = bs_read_ue( &s );
        }
    }

    /* BELOW, Further processing up to assert MMCO 5 presence for POC */
    if( p_slice->i_nal_type == 5 || p_slice->i_nal_ref_idc == 0 )
    {
        /* Early END, don't waste parsing below */
        p_slice->has_mmco5 = false;
        return true;
    }

    /* ref_pic_list_[mvc_]modification() */
    const bool b_mvc = (p_slice->i_nal_type == 20 || p_slice->i_nal_type == 21 );
    unsigned i = 0;
    if( i_slice_type % 5 != 2 && i_slice_type % 5 != 4 )
        i++;
    if( i_slice_type % 5 == 1 )
        i++;

    for( ; i>0; i-- )
    {
        if( bs_read1( &s ) ) /* ref_pic_list_modification_flag_l{0,1} */
        {
            uint32_t mod;
            do
            {
                mod = bs_read_ue( &s );
                if( mod < 3 || ( b_mvc && (mod == 4 || mod == 5) ) )
                    bs_read_ue( &s ); /* abs_diff_pic_num_minus1, long_term_pic_num, abs_diff_view_idx_min1 */
            }
            while( mod != 3 && !bs_eof( &s ) );
        }
    }

    if( bs_error( &s ) )
        return false;

    /* pred_weight_table() */
    if( ( p_pps->weighted_pred_flag && ( i_slice_type == 0 || i_slice_type == 5 || /* P, SP */
                                         i_slice_type == 3 || i_slice_type == 8 ) ) ||
        ( p_pps->weighted_bipred_idc == 1 && ( i_slice_type == 1 || i_slice_type == 6 ) /* B */ ) )
    {
        bs_read_ue( &s ); /* luma_log2_weight_denom */
        if( !p_sps->b_separate_colour_planes_flag ) /* ChromaArrayType != 0 */
            bs_read_ue( &s ); /* chroma_log2_weight_denom */

        const unsigned i_num_layers = ( i_slice_type % 5 == 1 ) ? 2 : 1;
        for( unsigned j=0; j < i_num_layers; j++ )
        {
            for( unsigned k=0; k<=num_ref_idx_l01_active_minus1[j]; k++ )
            {
                if( bs_read1( &s ) ) /* luma_weight_l{0,1}_flag */
                {
                    bs_read_se( &s );
                    bs_read_se( &s );
                }
                if( !p_sps->b_separate_colour_planes_flag ) /* ChromaArrayType != 0 */
                {
                    if( bs_read1( &s ) ) /* chroma_weight_l{0,1}_flag */
                    {
                        bs_read_se( &s );
                        bs_read_se( &s );
                        bs_read_se( &s );
                        bs_read_se( &s );
                    }
                }
            }
        }
    }

    /* dec_ref_pic_marking() */
    if( p_slice->i_nal_type != 5 ) /* IdrFlag */
    {
        if( bs_read1( &s ) ) /* adaptive_ref_pic_marking_mode_flag */
        {
            uint32_t mmco;
            do
            {
                mmco = bs_read_ue( &s );
                if( mmco == 1 || mmco == 3 )
                    bs_read_ue( &s ); /* diff_pics_minus1 */
                if( mmco == 2 )
                    bs_read_ue( &s ); /* long_term_pic_num */
                if( mmco == 3 || mmco == 6 )
                    bs_read_ue( &s ); /* long_term_frame_idx */
                if( mmco == 4 )
                    bs_read_ue( &s ); /* max_long_term_frame_idx_plus1 */
                if( mmco == 5 )
                {
                    p_slice->has_mmco5 = true;
                    break; /* Early END */
                }
            }
            while( mmco > 0 );
        }
    }

    /* If you need to store anything else than MMCO presence above, care of "Early END" cases */

    return !bs_error( &s );
}


void h264_compute_poc( const h264_sequence_parameter_set_t *p_sps,
                       const h264_slice_t *p_slice, h264_poc_context_t *p_ctx,
                       int *p_PictureOrderCount, int *p_tFOC, int *p_bFOC )
{
    *p_tFOC = *p_bFOC = 0;

    if( p_sps->i_pic_order_cnt_type == 0 )
    {
        unsigned maxPocLSB = 1U << (p_sps->i_log2_max_pic_order_cnt_lsb  + 4);

        /* POC reference */
        if( p_slice->i_nal_type == H264_NAL_SLICE_IDR )
        {
            p_ctx->prevPicOrderCnt.lsb = 0;
            p_ctx->prevPicOrderCnt.msb = 0;
        }
        else if( p_ctx->prevRefPictureHasMMCO5 )
        {
            p_ctx->prevPicOrderCnt.msb = 0;
            if( !p_ctx->prevRefPictureIsBottomField )
                p_ctx->prevPicOrderCnt.lsb = p_ctx->prevRefPictureTFOC;
            else
                p_ctx->prevPicOrderCnt.lsb = 0;
        }

        /* 8.2.1.1 */
        int pocMSB = p_ctx->prevPicOrderCnt.msb;
        int64_t orderDiff = p_slice->i_pic_order_cnt_lsb - p_ctx->prevPicOrderCnt.lsb;
        if( orderDiff < 0 && -orderDiff >= maxPocLSB / 2 )
            pocMSB += maxPocLSB;
        else if( orderDiff > maxPocLSB / 2 )
            pocMSB -= maxPocLSB;

        *p_tFOC = *p_bFOC = pocMSB + p_slice->i_pic_order_cnt_lsb;
        if( p_slice->i_field_pic_flag )
            *p_bFOC += p_slice->i_delta_pic_order_cnt_bottom;

        /* Save from ref picture */
        if( p_slice->i_nal_ref_idc /* Is reference */ )
        {
            p_ctx->prevRefPictureIsBottomField = (p_slice->i_field_pic_flag &&
                                                  p_slice->i_bottom_field_flag);
            p_ctx->prevRefPictureHasMMCO5 = p_slice->has_mmco5;
            p_ctx->prevRefPictureTFOC = *p_tFOC;
            p_ctx->prevPicOrderCnt.lsb = p_slice->i_pic_order_cnt_lsb;
            p_ctx->prevPicOrderCnt.msb = pocMSB;
        }
    }
    else
    {
        unsigned maxFrameNum = 1 << (p_sps->i_log2_max_frame_num + 4);
        unsigned frameNumOffset;
        unsigned expectedPicOrderCnt = 0;

        if( p_slice->i_nal_type == H264_NAL_SLICE_IDR )
            frameNumOffset = 0;
        else if( p_ctx->prevFrameNum > p_slice->i_frame_num )
            frameNumOffset = p_ctx->prevFrameNumOffset + maxFrameNum;
        else
            frameNumOffset = p_ctx->prevFrameNumOffset;

        if( p_sps->i_pic_order_cnt_type == 1 )
        {
            unsigned absFrameNum;

            if( p_sps->i_num_ref_frames_in_pic_order_cnt_cycle > 0 )
                absFrameNum = frameNumOffset + p_slice->i_frame_num;
            else
                absFrameNum = 0;

            if( p_slice->i_nal_ref_idc == 0 && absFrameNum > 0 )
                absFrameNum--;

            if( absFrameNum > 0 )
            {
                int32_t expectedDeltaPerPicOrderCntCycle = 0;
                for( int i=0; i<p_sps->i_num_ref_frames_in_pic_order_cnt_cycle; i++ )
                    expectedDeltaPerPicOrderCntCycle += p_sps->offset_for_ref_frame[i];

                unsigned picOrderCntCycleCnt = 0;
                unsigned frameNumInPicOrderCntCycle = 0;
                if( p_sps->i_num_ref_frames_in_pic_order_cnt_cycle )
                {
                    picOrderCntCycleCnt = ( absFrameNum - 1 ) / p_sps->i_num_ref_frames_in_pic_order_cnt_cycle;
                    frameNumInPicOrderCntCycle = ( absFrameNum - 1 ) % p_sps->i_num_ref_frames_in_pic_order_cnt_cycle;
                }

                expectedPicOrderCnt = picOrderCntCycleCnt * expectedDeltaPerPicOrderCntCycle;
                for( unsigned i=0; i <= frameNumInPicOrderCntCycle; i++ )
                    expectedPicOrderCnt = expectedPicOrderCnt + p_sps->offset_for_ref_frame[i];
            }

            if( p_slice->i_nal_ref_idc == 0 )
                expectedPicOrderCnt = expectedPicOrderCnt + p_sps->offset_for_non_ref_pic;

            *p_tFOC = expectedPicOrderCnt + p_slice->i_delta_pic_order_cnt0;
            if( !p_slice->i_field_pic_flag )
                *p_bFOC = *p_tFOC + p_sps->offset_for_top_to_bottom_field + p_slice->i_delta_pic_order_cnt1;
            else if( p_slice->i_bottom_field_flag )
                *p_bFOC = expectedPicOrderCnt + p_sps->offset_for_top_to_bottom_field + p_slice->i_delta_pic_order_cnt0;
        }
        else if( p_sps->i_pic_order_cnt_type == 2 )
        {
            unsigned tempPicOrderCnt;

            if( p_slice->i_nal_type == H264_NAL_SLICE_IDR )
                tempPicOrderCnt = 0;
            else if( p_slice->i_nal_ref_idc == 0 )
                tempPicOrderCnt = 2 * ( frameNumOffset + p_slice->i_frame_num ) - 1;
            else
                tempPicOrderCnt = 2 * ( frameNumOffset + p_slice->i_frame_num );

            *p_bFOC = *p_tFOC = tempPicOrderCnt;
        }

        p_ctx->prevFrameNum = p_slice->i_frame_num;
        if( p_slice->has_mmco5 )
            p_ctx->prevFrameNumOffset = 0;
        else
            p_ctx->prevFrameNumOffset = frameNumOffset;
    }

    /* 8.2.1 (8-1) */
    if( !p_slice->i_field_pic_flag ) /* progressive or contains both fields */
        *p_PictureOrderCount = __MIN( *p_bFOC, *p_tFOC );
    else /* split top or bottom field */
    if ( p_slice->i_bottom_field_flag )
        *p_PictureOrderCount = *p_bFOC;
    else
        *p_PictureOrderCount = *p_tFOC;
}

static uint8_t h264_infer_pic_struct( const h264_sequence_parameter_set_t *p_sps,
                                      const h264_slice_t *p_slice,
                                      uint8_t i_pic_struct, int tFOC, int bFOC )
{
    /* See D-1 and note 6 */
    if( !p_sps->vui.b_pic_struct_present_flag || i_pic_struct >= 9 )
    {
        if( p_slice->i_field_pic_flag )
            i_pic_struct = 1 + p_slice->i_bottom_field_flag;
        else if( tFOC == bFOC )
            i_pic_struct = 0;
        else if( tFOC < bFOC )
            i_pic_struct = 3;
        else
            i_pic_struct = 4;
    }

    return i_pic_struct;
}

uint8_t h264_get_num_ts( const h264_sequence_parameter_set_t *p_sps,
                         const h264_slice_t *p_slice, uint8_t i_pic_struct,
                         int tFOC, int bFOC )
{
    i_pic_struct = h264_infer_pic_struct( p_sps, p_slice, i_pic_struct, tFOC, bFOC );
    /* !WARN modified with nuit field based multiplier for values 0, 7 and 8 */
    const uint8_t rgi_numclock[9] = { 2, 1, 1, 2, 2, 3, 3, 4, 6 };
    return rgi_numclock[ i_pic_struct ];
}
